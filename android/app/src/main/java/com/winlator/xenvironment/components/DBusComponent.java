package com.winlator.xenvironment.components;

import android.util.Log;

import com.winlator.xenvironment.EnvironmentComponent;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Arrays;

/** App-private glibc D-Bus session daemon from the shared Debian rootfs. */
public final class DBusComponent extends EnvironmentComponent {
    private static final String TAG = "BionicX";
    private final File socket;
    private final File home;
    private java.lang.Process process;
    private Thread logThread;

    public DBusComponent(File socket, File home) {
        this.socket = socket;
        this.home = home;
    }

    @Override
    public void start() {
        stop();
        File rootfs = environment.getRootFS().getRootDir();
        File daemon = new File(rootfs, "usr/bin/dbus-daemon");
        File config = new File(rootfs, "usr/share/dbus-1/session.conf");
        File files = rootfs.getParentFile();
        File executor = new File(files, "bin/bionicx-exec");
        File runtime = new File(files, "lib/libbionicx-runtime.so");
        if (!daemon.isFile() || !config.isFile() || !executor.isFile()
                || !runtime.isFile())
            throw new IllegalStateException("D-Bus package is absent from rootfs");
        File parent = socket.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs())
            throw new IllegalStateException("cannot create D-Bus socket directory");
        if (socket.exists() && !socket.delete())
            throw new IllegalStateException("cannot remove stale D-Bus socket");

        ProcessBuilder builder = new ProcessBuilder(Arrays.asList(
                executor.getAbsolutePath(),
                "--cwd", home.getAbsolutePath(),
                "--env", "HOME=" + home.getAbsolutePath(),
                "--env", "TMPDIR=" + parent.getAbsolutePath(),
                "--env", "LD_LIBRARY_PATH=" + new File(rootfs, "usr/lib") + ":"
                        + new File(rootfs, "usr/lib/aarch64-linux-gnu"),
                "--env", "LD_PRELOAD=" + runtime.getAbsolutePath(),
                "--env", "BIONICX_ROOTFS=" + rootfs.getAbsolutePath(),
                "--env", "BIONICX_TMPDIR=" + parent.getAbsolutePath(),
                "--",
                daemon.getAbsolutePath(),
                "--nofork", "--nopidfile", "--nosyslog",
                "--address=unix:path=" + socket.getAbsolutePath(),
                "--config-file=" + config.getAbsolutePath()));
        builder.directory(home);
        builder.redirectErrorStream(true);
        try {
            process = builder.start();
            logThread = new Thread(() -> pump(process), "bionicx-dbus-log");
            logThread.start();
            for (int attempt = 0; attempt < 100 && !socket.exists(); attempt++) {
                if (!process.isAlive())
                    throw new IllegalStateException("D-Bus daemon exited before readiness");
                Thread.sleep(20);
            }
            if (!socket.exists())
                throw new IllegalStateException("D-Bus socket readiness timed out");
        }
        catch (IOException | InterruptedException error) {
            stop();
            if (error instanceof InterruptedException) Thread.currentThread().interrupt();
            throw new IllegalStateException("cannot start D-Bus session daemon", error);
        }
    }

    private void pump(java.lang.Process running) {
        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(running.getInputStream()))) {
            String line;
            while ((line = reader.readLine()) != null) Log.i(TAG, "dbus: " + line);
        }
        catch (IOException error) {
            if (running.isAlive()) Log.w(TAG, "D-Bus log stream failed", error);
        }
    }

    @Override
    public void stop() {
        java.lang.Process running = process;
        process = null;
        if (running != null) {
            running.destroy();
            try {
                if (!running.waitFor(500, java.util.concurrent.TimeUnit.MILLISECONDS))
                    running.destroyForcibly();
            }
            catch (InterruptedException error) {
                Thread.currentThread().interrupt();
                running.destroyForcibly();
            }
        }
        if (socket.exists() && !socket.delete())
            Log.w(TAG, "cannot remove D-Bus socket " + socket);
    }
}
