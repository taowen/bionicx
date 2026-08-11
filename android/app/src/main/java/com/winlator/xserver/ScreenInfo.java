package com.winlator.xserver;

import android.util.Rational;

import com.winlator.math.Mathf;

public class ScreenInfo {
    public static final short MIN_WIDTH = 320;
    public static final short MIN_HEIGHT = 200;
    private static final int DEFAULT_DPI = 254;
    public final short width;
    public final short height;
    private final int dpi;

    public ScreenInfo(String value) {
        String[] parts = value.split("x");
        width = Short.parseShort(parts[0]);
        height = Short.parseShort(parts[1]);
        dpi = DEFAULT_DPI;
    }

    public ScreenInfo(int width, int height) {
        this(width, height, DEFAULT_DPI);
    }

    public ScreenInfo(int width, int height, int dpi) {
        this.width = (short)width;
        this.height = (short)height;
        this.dpi = dpi;
    }

    public short getWidthInMillimeters() {
        return pixelsToMillimeters(width);
    }

    public short getHeightInMillimeters() {
        return pixelsToMillimeters(height);
    }

    private short pixelsToMillimeters(short pixels) {
        return (short)Math.round((pixels & 0xffff) * 25.4 / dpi);
    }

    public Rational aspectRatio() {
        return Mathf.farey((float)width / height, 10);
    }

    @Override
    public String toString() {
        return width+"x"+height;
    }
}
