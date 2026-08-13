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
import com.winlator.core.NetworkHelper;
import com.winlator.core.TarCompressorUtils;
import com.winlator.widget.TouchpadView;
import com.winlator.widget.XServerView;
import com.winlator.xconnector.UnixSocketConfig;
import com.winlator.xenvironment.RootFS;
import com.winlator.xenvironment.XEnvironment;
import com.winlator.xenvironment.components.DBusComponent;
import com.winlator.xenvironment.components.CupsComponent;
import com.winlator.xenvironment.components.PulseAudioComponent;
import com.winlator.xenvironment.components.SysVSharedMemoryComponent;
import com.winlator.xenvironment.components.XServerComponent;
import com.winlator.xenvironment.components.VortekRendererComponent;
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
            File executor = extractAsset("bionicx/bin/bionicx-exec",
                    new File(getFilesDir(), "bin/bionicx-exec"));
            Os.chmod(executor.getPath(), 0700);
            extractAsset("bionicx/lib/libbionicx-runtime.so",
                    new File(getFilesDir(), "lib/libbionicx-runtime.so"));
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
        environment.addComponent(new SysVSharedMemoryComponent(xServer,
                UnixSocketConfig.create(rootFS.getRootDir().getAbsolutePath(),
                        UnixSocketConfig.SYSVSHM_SERVER_PATH)));
        environment.addComponent(new XServerComponent(xServer, socket));
        if (profile.hostServices.contains("dbus")) {
            File busSocket = new File(getFilesDir(),
                    "run/" + profile.id + "/runtime/bus");
            environment.addComponent(new DBusComponent(busSocket,
                    new File(getFilesDir(), "homes/" + profile.id)));
            Log.i(TAG, "enabled D-Bus session service at " + busSocket);
        }
        if (profile.hostServices.contains("pulseaudio")) {
            File pulseDir = new File(getFilesDir(), "pulseaudio");
            if (!TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD,
                    this, "pulseaudio.tzst", pulseDir))
                throw new IOException("cannot extract PulseAudio modules");
            UnixSocketConfig pulseSocket = UnixSocketConfig.create(
                    rootFS.getRootDir().getAbsolutePath(),
                    UnixSocketConfig.PULSE_SERVER_PATH);
            environment.addComponent(new PulseAudioComponent(pulseSocket));
            Log.i(TAG, "enabled PulseAudio host service at " + pulseSocket.path);
        }
        if (profile.hostServices.contains("cups")) {
            environment.addComponent(new CupsComponent());
            Log.i(TAG, "enabled app-private CUPS service");
        }
        if (profile.hostServices.contains("vulkan")) {
            UnixSocketConfig vortekSocket = UnixSocketConfig.create(
                    rootFS.getRootDir().getAbsolutePath(),
                    UnixSocketConfig.VORTEK_SERVER_PATH);
            environment.addComponent(new VortekRendererComponent(
                    this, xServer, vortekSocket,
                    new VortekRendererComponent.Options()));
            Log.i(TAG, "enabled Vulkan host service at " + vortekSocket.path);
        }
        environment.startEnvironmentComponents();
    }

    private void launchProfile() {
        try {
            File runtimeRun = new File(getCacheDir(), "run");
            if (!runtimeRun.isDirectory() && !runtimeRun.mkdirs())
                throw new IOException("cannot create " + runtimeRun);
            File executor = new File(getFilesDir(), "bin/bionicx-exec");
            Os.chmod(executor.getPath(), 0700);

            List<String> command = new ArrayList<>();
            command.add(executor.getPath());
            if (profile.diagnoseSignals) command.add("--diagnose-signals");
            if (profile.debugStop) command.add("--debug-stop");
            command.add("--cwd");
            command.add(profile.expand(this, profile.workingDirectory));
            if (!profile.argv0.isEmpty()) {
                command.add("--argv0");
                command.add(profile.expand(this, profile.argv0));
            }

            for (Map.Entry<String, String> variable : profile.environment.entrySet()) {
                command.add("--env");
                command.add(variable.getKey() + "="
                        + profile.expand(this, variable.getValue()));
            }
            String executable = profile.expand(this, profile.executable);
            File executableFile = new File(executable);
            List<String> servers = new NetworkHelper(this).getDnsServers();
            Log.i(TAG, "runtime DNS servers=" + servers.size());
            command.add("--env");
            command.add("BIONICX_DNS_SERVERS=" + String.join(",", servers));
            command.add("--env");
            command.add("BIONICX_ROOTFS=" + profile.expand(this, "${RUNTIME}"));
            command.add("--env");
            command.add("BIONICX_APP=" + profile.expand(this, "${APP}"));
            File appLib = new File(profile.expand(this, "${APP}"), "lib");
            if (appLib.isDirectory()) {
                command.add("--env");
                command.add("LD_LIBRARY_PATH=" + appLib.getPath());
            }
            command.add("--env");
            command.add("BIONICX_TMPDIR=" + profile.expand(this, "${CACHE}"));
            command.add("--env");
            command.add("FONTCONFIG_PATH="
                    + profile.expand(this, "${RUNTIME}/etc/fonts"));
            command.add("--env");
            command.add("FONTCONFIG_FILE=fonts.conf");
            command.add("--env");
            command.add("FONTCONFIG_SYSROOT=" + profile.expand(this, "${RUNTIME}"));
            if (profile.hostServices.contains("dbus")
                    && !profile.environment.containsKey("DBUS_SESSION_BUS_ADDRESS")) {
                command.add("--env");
                command.add("DBUS_SESSION_BUS_ADDRESS=unix:path="
                        + profile.expand(this, "${TMP}/runtime/bus"));
            }
            if (profile.hostServices.contains("cups")
                    && !profile.environment.containsKey("CUPS_SERVER")) {
                command.add("--env");
                command.add("CUPS_SERVER=" + profile.expand(this,
                        "${TMP}/../cups/run/cups.sock"));
            }
            command.add("--env");
            command.add("LD_PRELOAD="
                    + new File(getFilesDir(), "lib/libbionicx-runtime.so").getPath());
            command.add("--");
            if (!executableFile.isFile())
                throw new IOException("application is not installed: " + executable);
            Os.chmod(executable, 0700);
            command.add(executable);
            for (String argument : profile.arguments)
                command.add(profile.expand(this, argument));

            Log.i(TAG, "launching " + profile.name + ": " + command);
            ProcessBuilder builder = new ProcessBuilder(command);
            builder.redirectErrorStream(false);
            child = builder.start();
            Thread stdout = pumpLog(child.getInputStream(), "stdout");
            Thread stderr = pumpLog(child.getErrorStream(), "stderr");
            int status = child.waitFor();
            stdout.join();
            stderr.join();
            Log.i(TAG, profile.id + " exited with " + status);
        }
        catch (Exception error) {
            fail("launch failed", error);
        }
    }

    private Thread pumpLog(InputStream stream, String name) {
        Thread thread = new Thread(() -> {
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(stream))) {
                String line;
                while ((line = reader.readLine()) != null) Log.i(TAG, line);
            }
            catch (IOException error) {
                Log.w(TAG, name + " log stream failed", error);
            }
        }, "bionicx-" + name);
        thread.start();
        return thread;
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
