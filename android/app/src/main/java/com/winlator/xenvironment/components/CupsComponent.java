package com.winlator.xenvironment.components;

import android.util.Log;

import com.winlator.xenvironment.EnvironmentComponent;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Arrays;

/** App-private CUPS daemon shared by Debian GUI profiles. */
public final class CupsComponent extends EnvironmentComponent {
    private static final String TAG = "BionicX";
    private Process process;
    private Thread logThread;
    private File serverRoot;

    @Override
    public void start() {
        stop();
        File rootfs = environment.getRootFS().getRootDir();
        File files = rootfs.getParentFile();
        File executor = new File(files, "bin/bionicx-exec");
        File runtime = new File(files, "lib/libbionicx-runtime.so");
        File daemon = new File(rootfs, "usr/sbin/cupsd");
        if (!daemon.isFile() || !executor.isFile() || !runtime.isFile())
            throw new IllegalStateException("CUPS package is absent from rootfs");
        serverRoot = new File(files, "run/cups");
        File run = new File(serverRoot, "run");
        File spool = new File(serverRoot, "spool");
        File cache = new File(serverRoot, "cache");
        File config = new File(serverRoot, "cupsd.conf");
        File filesConfig = new File(serverRoot, "cups-files.conf");
        if (!run.mkdirs() && !run.isDirectory()) throw new IllegalStateException("cannot create CUPS run dir");
        if (!spool.mkdirs() && !spool.isDirectory()) throw new IllegalStateException("cannot create CUPS spool dir");
        if (!cache.mkdirs() && !cache.isDirectory()) throw new IllegalStateException("cannot create CUPS cache dir");
        write(config, "LogLevel warn\nListen " + new File(run, "cups.sock").getAbsolutePath()
                + "\nBrowsing Off\nWebInterface No\nMaxLogSize 0\nSystemGroup lpadmin\n");
        write(filesConfig, "StateDir " + serverRoot.getAbsolutePath() + "\n"
                + "CacheDir " + cache.getAbsolutePath() + "\n"
                + "RequestRoot " + spool.getAbsolutePath() + "\n"
                + "TempDir " + run.getAbsolutePath() + "\n"
                + "FileDevice Yes\n");
        ProcessBuilder builder = new ProcessBuilder(Arrays.asList(
                executor.getAbsolutePath(), "--cwd", serverRoot.getAbsolutePath(),
                "--env", "HOME=" + serverRoot.getAbsolutePath(),
                "--env", "TMPDIR=" + run.getAbsolutePath(),
                "--env", "LD_PRELOAD=" + runtime.getAbsolutePath(),
                "--env", "BIONICX_ROOTFS=" + rootfs.getAbsolutePath(),
                "--env", "BIONICX_TMPDIR=" + run.getAbsolutePath(),
                "--env", "CUPS_SERVERROOT=" + serverRoot.getAbsolutePath(),
                "--env", "CUPS_STATEDIR=" + serverRoot.getAbsolutePath(),
                "--env", "CUPS_DATADIR=" + new File(rootfs, "usr/share/cups"),
                "--", daemon.getAbsolutePath(), "-f", "-c", config.getAbsolutePath()));
        builder.directory(serverRoot);
        builder.redirectErrorStream(true);
        try {
            process = builder.start();
            logThread = new Thread(() -> pump(process), "bionicx-cups-log");
            logThread.start();
        } catch (IOException error) {
            stop();
            throw new IllegalStateException("cannot start CUPS daemon", error);
        }
    }

    private static void write(File file, String value) {
        try (FileWriter writer = new FileWriter(file, false)) { writer.write(value); }
        catch (IOException error) { throw new IllegalStateException("cannot write CUPS config", error); }
    }

    private void pump(Process running) {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(running.getInputStream()))) {
            String line; while ((line = reader.readLine()) != null) Log.i(TAG, "cups: " + line);
        } catch (IOException error) { if (running.isAlive()) Log.w(TAG, "CUPS log stream failed", error); }
    }

    @Override public void stop() {
        Process running = process; process = null;
        if (running != null) {
            running.destroy();
            try { if (!running.waitFor(500, java.util.concurrent.TimeUnit.MILLISECONDS)) running.destroyForcibly(); }
            catch (InterruptedException error) { Thread.currentThread().interrupt(); running.destroyForcibly(); }
        }
    }
}
