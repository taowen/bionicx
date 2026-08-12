package com.winlator.bionicx;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Immutable, validated launch description for one glibc application. */
public final class AppProfile {
    public static final int SCHEMA_VERSION = 1;

    public final String id;
    public final String name;
    public final int displayDpi;
    public final String socketMode;
    public final String mode;
    public final String loader;
    public final String libraryPath;
    public final String executable;
    public final String workingDirectory;
    public final String argv0;
    public final boolean diagnoseSignals;
    public final boolean debugStop;
    public final List<String> arguments;
    public final Map<String, String> environment;
    public final List<String> compatibility;
    public final List<String> hostServices;

    private AppProfile(JSONObject root) throws JSONException {
        int schema = root.getInt("schemaVersion");
        if (schema != SCHEMA_VERSION)
            throw new JSONException("unsupported schemaVersion: " + schema);
        id = requireId(root.getString("id"));
        name = root.optString("name", id);

        JSONObject display = root.optJSONObject("display");
        displayDpi = display != null ? display.optInt("dpi", 144) : 144;
        socketMode = display != null
                ? display.optString("socket", "abstract") : "abstract";
        if (displayDpi < 72 || displayDpi > 400)
            throw new JSONException("display.dpi outside 72..400");
        if (!socketMode.equals("abstract") && !socketMode.equals("filesystem"))
            throw new JSONException("display.socket must be abstract or filesystem");

        JSONObject launch = root.getJSONObject("launch");
        mode = launch.optString("mode", "loader");
        if (!mode.equals("loader") && !mode.equals("direct"))
            throw new JSONException("launch.mode must be loader or direct");
        loader = launch.optString("loader", "");
        libraryPath = launch.optString("libraryPath", "");
        executable = requirePath(launch.getString("executable"), "executable");
        workingDirectory = requirePath(
                launch.optString("workingDirectory", "${APP}"),
                "workingDirectory");
        argv0 = launch.optString("argv0", "");
        diagnoseSignals = launch.optBoolean("diagnoseSignals", false);
        debugStop = launch.optBoolean("debugStop", false);
        if (mode.equals("loader") && (loader.isEmpty() || libraryPath.isEmpty()))
            throw new JSONException("loader mode needs loader and libraryPath");

        arguments = stringList(launch.optJSONArray("arguments"));
        compatibility = stringList(root.optJSONArray("compatibility"));
        hostServices = stringList(root.optJSONArray("hostServices"));
        for (String service : hostServices) {
            if (!service.equals("dbus") && !service.equals("pulseaudio")
                    && !service.equals("vulkan"))
                throw new JSONException("unsupported host service: " + service);
        }
        environment = new LinkedHashMap<>();
        JSONObject env = launch.optJSONObject("environment");
        if (env != null) {
            Iterator<String> keys = env.keys();
            while (keys.hasNext()) {
                String key = keys.next();
                if (!key.matches("[A-Za-z_][A-Za-z0-9_]*"))
                    throw new JSONException("invalid environment name: " + key);
                environment.put(key, env.getString(key));
            }
        }
    }

    public static AppProfile load(Context context) throws IOException, JSONException {
        File external = new File(context.getFilesDir(), "profiles/active.json");
        InputStream input = external.isFile()
                ? new FileInputStream(external)
                : context.getAssets().open("bionicx/profiles/default.json");
        try (InputStream stream = input;
             BufferedReader reader = new BufferedReader(new InputStreamReader(stream))) {
            StringBuilder json = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) json.append(line).append('\n');
            return new AppProfile(new JSONObject(json.toString()));
        }
    }

    public String expand(Context context, String value) throws IOException {
        File files = context.getFilesDir().getCanonicalFile();
        File app = new File(files, "apps/" + id).getCanonicalFile();
        File runtime = new File(files, "rootfs").getCanonicalFile();
        File home = new File(files, "homes/" + id).getCanonicalFile();
        File tmp = new File(files, "run/" + id).getCanonicalFile();
        File cache = context.getCacheDir().getCanonicalFile();
        String expanded = value
                .replace("${FILES}", files.getPath())
                .replace("${APP}", app.getPath())
                .replace("${RUNTIME}", runtime.getPath())
                .replace("${HOME}", home.getPath())
                .replace("${TMP}", tmp.getPath())
                .replace("${CACHE}", cache.getPath())
                .replace("${DISPLAY}", ":0");
        if (expanded.contains("${"))
            throw new IOException("unknown token in profile value: " + value);
        return expanded;
    }

    public void createPrivateDirectories(Context context) throws IOException {
        File files = context.getFilesDir().getCanonicalFile();
        File[] directories = {
                new File(files, "apps/" + id),
                new File(files, "homes/" + id),
                new File(files, "run/" + id),
                new File(files, "run/" + id + "/runtime"),
                new File(files, "rootfs/tmp")
        };
        for (File directory : directories) {
            if (!directory.isDirectory() && !directory.mkdirs())
                throw new IOException("cannot create " + directory);
        }
    }

    private static String requireId(String value) throws JSONException {
        if (!value.matches("[a-z0-9][a-z0-9._-]{0,63}"))
            throw new JSONException("invalid profile id: " + value);
        return value;
    }

    private static String requirePath(String value, String field)
            throws JSONException {
        if (value.indexOf('\0') >= 0 || value.isEmpty())
            throw new JSONException("invalid " + field);
        return value;
    }

    private static List<String> stringList(JSONArray array) throws JSONException {
        List<String> values = new ArrayList<>();
        if (array == null) return values;
        for (int i = 0; i < array.length(); ++i) values.add(array.getString(i));
        return values;
    }
}
