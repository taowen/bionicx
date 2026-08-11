package com.winlator;

import android.app.Activity;
import android.os.Bundle;
import android.system.Os;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.KeyEvent;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.Toast;

import com.winlator.bionicx.AppProfile;
import com.winlator.core.AppUtils;
import com.winlator.widget.TouchpadView;
import com.winlator.widget.XServerView;
import com.winlator.xconnector.UnixSocketConfig;
import com.winlator.xenvironment.RootFS;
import com.winlator.xenvironment.XEnvironment;
import com.winlator.xenvironment.components.XServerComponent;
import com.winlator.xserver.ScreenInfo;
import com.winlator.xserver.XServer;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Minimal Android host for one profile-selected native glibc/X11 program. */
public final class BionicXActivity extends Activity {
    private static final String TAG = "BionicX";

    private XEnvironment environment;
    private XServerView xServerView;
    private XServer xServer;
    private Process child;
    private AppProfile profile;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        AppUtils.hideSystemUI(this);
        try {
            profile = AppProfile.load(this);
            profile.createPrivateDirectories(this);
            startXServer();
            new Thread(this::launchProfile, "bionicx-launcher").start();
        }
        catch (Exception error) {
            fail("profile setup failed", error);
        }
    }

    private void startXServer() throws IOException {
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
        int width = Math.max(metrics.widthPixels, metrics.heightPixels);
        int height = Math.min(metrics.widthPixels, metrics.heightPixels);
        ScreenInfo screenInfo = new ScreenInfo(width, height, profile.displayDpi);
        Log.i(TAG, "profile=" + profile.id + " display=" + width + "x" + height
                + "@" + profile.displayDpi + " socket=" + profile.socketMode);

        xServer = new XServer(null, screenInfo);
        xServerView = new XServerView(this, xServer);
        xServer.setRenderer(xServerView.getRenderer());
        TouchpadView touchpad = new TouchpadView(this, xServer, false);
        touchpad.setMoveCursorToTouchpoint(true);

        FrameLayout root = new FrameLayout(this);
        root.addView(xServerView, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        root.addView(touchpad, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        setContentView(root);

        RootFS rootFS = RootFS.find(this);
        UnixSocketConfig socket = profile.socketMode.equals("abstract")
                ? UnixSocketConfig.createAbstract("/tmp/.X11-unix/X0")
                : UnixSocketConfig.create(rootFS.getRootDir().getAbsolutePath(),
                        UnixSocketConfig.XSERVER_PATH);
        environment = new XEnvironment(this, rootFS);
        environment.addComponent(new XServerComponent(xServer, socket));
        environment.startEnvironmentComponents();
    }

    private void launchProfile() {
        try {
            File executor = extractAsset("bionicx/bin/bionicx-exec",
                    new File(getFilesDir(), "bin/bionicx-exec"));
            Os.chmod(executor.getPath(), 0700);

            List<String> command = new ArrayList<>();
            command.add(executor.getPath());
            if (profile.diagnoseSignals) command.add("--diagnose-signals");
            if (profile.debugStop) command.add("--debug-stop");
            if (profile.mode.equals("loader")) {
                command.add("--loader");
                command.add(profile.expand(this, profile.loader));
                command.add("--library-path");
                command.add(profile.expand(this, profile.libraryPath));
            }
            else command.add("--direct");
            command.add("--cwd");
            command.add(profile.expand(this, profile.workingDirectory));
            if (!profile.argv0.isEmpty()) {
                command.add("--argv0");
                command.add(profile.argv0);
            }

            for (Map.Entry<String, String> variable : profile.environment.entrySet()) {
                if (!profile.compatibility.isEmpty()
                        && variable.getKey().equals("LD_PRELOAD")) continue;
                command.add("--env");
                command.add(variable.getKey() + "="
                        + profile.expand(this, variable.getValue()));
            }
            if (!profile.compatibility.isEmpty()) {
                List<String> preloads = new ArrayList<>();
                for (String module : profile.compatibility) {
                    if (!module.matches("[a-z0-9][a-z0-9_-]*"))
                        throw new IOException("invalid compatibility module: " + module);
                    File library = extractAsset(
                            "bionicx/lib/libbionicx-" + module + ".so",
                            new File(getFilesDir(),
                                    "lib/libbionicx-" + module + ".so"));
                    preloads.add(library.getPath());
                }
                String configured = profile.environment.containsKey("LD_PRELOAD")
                        ? profile.expand(this, profile.environment.get("LD_PRELOAD"))
                        : "";
                if (!configured.isEmpty()) preloads.add(configured);
                command.add("--env");
                command.add("LD_PRELOAD=" + String.join(":", preloads));
            }
            command.add("--");
            String executable = profile.expand(this, profile.executable);
            File executableFile = new File(executable);
            if (!executableFile.isFile())
                throw new IOException("application is not installed: " + executable);
            Os.chmod(executable, 0700);
            command.add(executable);
            for (String argument : profile.arguments)
                command.add(profile.expand(this, argument));

            Log.i(TAG, "launching " + profile.name + ": " + command);
            ProcessBuilder builder = new ProcessBuilder(command);
            builder.redirectErrorStream(true);
            child = builder.start();
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(child.getInputStream()))) {
                String line;
                while ((line = reader.readLine()) != null) Log.i(TAG, line);
            }
            Log.i(TAG, profile.id + " exited with " + child.waitFor());
        }
        catch (Exception error) {
            fail("launch failed", error);
        }
    }

    private File extractAsset(String asset, File destination) throws IOException {
        File parent = destination.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs())
            throw new IOException("cannot create " + parent);
        try (InputStream input = getAssets().open(asset);
             FileOutputStream output = new FileOutputStream(destination, false)) {
            byte[] buffer = new byte[64 * 1024];
            int count;
            while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
        }
        return destination;
    }

    private void fail(String message, Exception error) {
        Log.e(TAG, message, error);
        runOnUiThread(() -> Toast.makeText(this,
                message + ": " + error.getMessage(), Toast.LENGTH_LONG).show());
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) AppUtils.hideSystemUI(this);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        return (xServer != null && xServer.keyboard.onKeyEvent(event))
                || super.dispatchKeyEvent(event);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (xServerView != null) xServerView.onResume();
        if (environment != null) environment.onResume();
    }

    @Override
    protected void onPause() {
        if (environment != null) environment.onPause();
        if (xServerView != null) xServerView.onPause();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        if (child != null) child.destroy();
        if (environment != null) environment.stopEnvironmentComponents();
        super.onDestroy();
    }
}
