package com.winlator.xserver;

import android.util.Log;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * BadImplementation used to land on WinlatorXRequest:W. Accept scripts
 * only collect BionicX:I, so a missing Render request looked like a
 * black title bar.
 */
public final class UnimplementedRequest {
    private static final ConcurrentHashMap<String, AtomicInteger> COUNTS =
            new ConcurrentHashMap<>();

    private UnimplementedRequest() {}

    public static void note(String extension, String request, int major,
                            int minor) {
        String key = (major & 0xff) + "." + (minor & 0xff);
        int count = COUNTS.computeIfAbsent(key, ignored -> new AtomicInteger())
                .incrementAndGet();
        if (count != 1 && (count & (count - 1)) != 0) return;
        Log.i("BionicX", "BXINFO unimplemented " + extension + " " + request
                + " major=" + (major & 0xff) + " minor=" + (minor & 0xff)
                + " count=" + count);
    }
}
