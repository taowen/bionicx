package com.winlator.xserver;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;

import androidx.annotation.Nullable;

import com.winlator.core.Callback;
import com.winlator.math.Mathf;
import com.winlator.renderer.GLRenderer;
import com.winlator.renderer.GPUImage;
import com.winlator.renderer.Texture;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Drawable extends XResource {
    public final short width;
    public final short height;
    public final Visual visual;
    private Texture texture;
    private ByteBuffer data;
    private boolean useSharedData;
    private Runnable onDrawListener;
    private boolean offscreenStorage = false;
    private Callback<Drawable> onDestroyListener;
    public final Object renderLock = new Object();
    private XServer xServer;
    private int dirtyLeft;
    private int dirtyTop;
    private int dirtyRight;
    private int dirtyBottom;
    private boolean hasDirty;
    private int gpuLeft;
    private int gpuTop;
    private int gpuRight;
    private int gpuBottom;
    private volatile boolean hasGpuDirty;

    static {
        System.loadLibrary("winlator");
    }

    public Drawable(int id, int width, int height, Visual visual) {
        super(id);
        this.width = (short)width;
        this.height = (short)height;
        this.visual = visual;
        this.texture = new Texture(this);
        this.data = ByteBuffer.allocateDirect(width * height * 4).order(ByteOrder.LITTLE_ENDIAN);
    }

    public static Drawable fromBitmap(Bitmap bitmap) {
        Drawable drawable = new Drawable(0, bitmap.getWidth(), bitmap.getHeight(), null);
        fromBitmap(bitmap, drawable.data);
        return drawable;
    }

    public boolean isOffscreenStorage() {
        return offscreenStorage;
    }

    public void setOffscreenStorage(boolean offscreenStorage) {
        this.offscreenStorage = offscreenStorage;
        if (!offscreenStorage && texture != null) texture.setNeedsUpdate(true);
    }

    public Texture getTexture() {
        return texture;
    }

    public void setXServer(XServer xServer) {
        this.xServer = xServer;
    }

    public void setTexture(Texture texture) {
        if (texture instanceof GPUImage) data = ((GPUImage)texture).getVirtualData();
        this.texture = texture;
    }

    public Texture replaceTextureKeepCpuBuffer(Texture texture) {
        Texture previous = this.texture;
        this.texture = texture;
        if (texture != null) texture.setOwner(this);
        return previous;
    }

    public void markGpuPixelsCurrent(int x, int y, int w, int h) {
        synchronized (renderLock) {
            unionGpuDirty(x, y, w, h);
            /* GPU owns this rect. Do not upload stale CPU bytes over it. */
            if (texture != null) texture.setNeedsUpdate(false);
        }
        if (onDrawListener != null) onDrawListener.run();
    }

    public int[] peekGpuDirty() {
        synchronized (renderLock) {
            if (!hasGpuDirty) return null;
            return new int[] {
                gpuLeft, gpuTop, gpuRight - gpuLeft, gpuBottom - gpuTop
            };
        }
    }

    public void clearGpuDirty() {
        synchronized (renderLock) {
            hasGpuDirty = false;
        }
    }

    /**
     * Glamor prepare_access: download GPU-only pixels before the CPU
     * reads or writes the heap. Safe to call while holding
     * DRAWABLE_MANAGER; the GL present path uses tryLock.
     */
    public boolean ensureCpuPixels() {
        if (!hasGpuDirty) return true;
        GLRenderer renderer = xServer != null ? xServer.getRenderer() : null;
        if (renderer == null || !renderer.hasEglContext()) return false;
        return renderer.downloadToCpu(this);
    }

    @Nullable
    public ByteBuffer getData() {
        return data;
    }

    public void setData(ByteBuffer data) {
        this.data = data;
    }

    private short getStride() {
        if (texture instanceof GPUImage) {
            GPUImage image = (GPUImage)texture;
            if (data != null && data == image.getVirtualData() && image.getStride() > 0)
                return image.getStride();
        }
        return width;
    }

    public short getImageStride() {
        return getStride();
    }

    public Runnable getOnDrawListener() {
        return onDrawListener;
    }

    public void setOnDrawListener(Runnable onDrawListener) {
        this.onDrawListener = onDrawListener;
    }

    public Callback<Drawable> getOnDestroyListener() {
        return onDestroyListener;
    }

    public void setOnDestroyListener(Callback<Drawable> onDestroyListener) {
        this.onDestroyListener = onDestroyListener;
    }

    public void drawImage(short srcX, short srcY, short dstX, short dstY, short width, short height, byte depth, ByteBuffer data, short totalWidth, short totalHeight) {
        if (this.data == null) return;
        ensureCpuPixels();

        if (depth == 1) {
            drawBitmap(width, height, data, this.data);
        }
        else if (depth == 8) {
            int sourceStride = (totalWidth + 3) & ~3;
            synchronized (renderLock) {
                for (int row = 0; row < height; row++) {
                    int targetY = dstY + row;
                    if (targetY < 0 || targetY >= this.height) continue;
                    for (int column = 0; column < width; column++) {
                        int targetX = dstX + column;
                        if (targetX < 0 || targetX >= this.width) continue;
                        int alpha = Byte.toUnsignedInt(data.get(
                                (srcY + row) * sourceStride + srcX + column));
                        this.data.putInt((targetY * getStride() + targetX) * 4,
                                alpha);
                    }
                }
                this.data.rewind();
            }
        }
        else if (depth == 24 || depth == 32) {
            dstX = (short)Mathf.clamp(dstX, 0, this.width-1);
            dstY = (short)Mathf.clamp(dstY, 0, this.height-1);
            if ((dstX + width) > this.width) width = (short)((this.width - dstX));
            if ((dstY + height) > this.height) height = (short)((this.height - dstY));

            copyArea(srcX, srcY, dstX, dstY, width, height, totalWidth, this.getStride(), data, this.data);
        }

        this.data.rewind();
        data.rewind();

        forceUpdate(dstX, dstY, width, height);
    }

    public ByteBuffer getImage(short x, short y, short width, short height) {
        ByteBuffer dstData = ByteBuffer.allocateDirect(width * height * 4).order(ByteOrder.LITTLE_ENDIAN);
        if (this.data == null) return dstData;
        ensureCpuPixels();

        x = (short)Mathf.clamp(x, 0, this.width-1);
        y = (short)Mathf.clamp(y, 0, this.height-1);
        if ((x + width) > this.width) width = (short)(this.width - x);
        if ((y + height) > this.height) height = (short)(this.height - y);

        copyArea(x, y, (short)0, (short)0, width, height, this.getStride(), width, this.data, dstData);

        this.data.rewind();
        dstData.rewind();
        return dstData;
    }

    public void copyArea(short srcX, short srcY, short dstX, short dstY, short width, short height, Drawable drawable) {
        copyArea(srcX, srcY, dstX, dstY, width, height, drawable, GraphicsContext.Function.COPY);
    }

    public void copyArea(short srcX, short srcY, short dstX, short dstY, short width, short height, Drawable drawable, GraphicsContext.Function gcFunction) {
        if (this.data == null || drawable.data == null) return;
        dstX = (short)Mathf.clamp(dstX, 0, this.width-1);
        dstY = (short)Mathf.clamp(dstY, 0, this.height-1);
        if ((dstX + width) > this.width)
            width = (short)(this.width - dstX);
        if ((dstY + height) > this.height)
            height = (short)(this.height - dstY);
        if (gcFunction == GraphicsContext.Function.COPY
                && tryCopyAreaGpu(srcX, srcY, dstX, dstY, width, height,
                drawable))
            return;
        ensureCpuPixels();
        drawable.ensureCpuPixels();
        synchronized (renderLock) {
            if (gcFunction == GraphicsContext.Function.COPY) {
                copyArea(srcX, srcY, dstX, dstY, width, height,
                        drawable.getStride(), this.getStride(), drawable.data,
                        this.data);
            }
            else copyAreaOp(srcX, srcY, dstX, dstY, width, height,
                    drawable.getStride(), this.getStride(), drawable.data,
                    this.data, gcFunction.ordinal());

            this.data.rewind();
            drawable.data.rewind();
            forceUpdate(dstX, dstY, width, height);
        }
    }

    public void fillColor(int color) {
        fillRect(0, 0, width, height, color);
    }

    public void fillRect(int x, int y, int width, int height, int color) {
        if (this.data == null) return;
        x = (short)Mathf.clamp(x, 0, this.width-1);
        y = (short)Mathf.clamp(y, 0, this.height-1);
        if ((x + width) > this.width) width = (short)((this.width - x));
        if ((y + height) > this.height) height = (short)((this.height - y));
        if (tryFillGpu(x, y, width, height, color)) return;
        ensureCpuPixels();
        synchronized (renderLock) {
            fillRect((short)x, (short)y, (short)width, (short)height, color,
                    this.getStride(), this.data);
            this.data.rewind();
            forceUpdate(x, y, width, height);
        }
    }

    /** Blends an unpacked 8-bit alpha glyph mask over this 32-bit drawable. */
    public void blendAlphaMask(int x, int y, int width, int height,
                               byte[] alpha, int sourceColor) {
        if (data == null || visual == null || visual.depth != 32) return;
        ensureCpuPixels();
        int stride = getStride();
        int sourceAlpha = (sourceColor >>> 24) & 0xff;
        if (sourceAlpha == 0) sourceAlpha = 0xff;
        synchronized (renderLock) {
            for (int row = 0; row < height; row++) {
                int dstY = y + row;
                if (dstY < 0 || dstY >= this.height) continue;
                for (int column = 0; column < width; column++) {
                    int dstX = x + column;
                    if (dstX < 0 || dstX >= this.width) continue;
                    int mask = Byte.toUnsignedInt(alpha[row * width + column]);
                    int a = (mask * sourceAlpha + 127) / 255;
                    if (a == 0) continue;
                    int offset = (dstY * stride + dstX) * 4;
                    int destination = opaqueIfUnusedAlpha(data.getInt(offset));
                    int inverse = 255 - a;
                    int red = (((sourceColor >>> 16) & 0xff) * a
                            + ((destination >>> 16) & 0xff) * inverse + 127) / 255;
                    int green = (((sourceColor >>> 8) & 0xff) * a
                            + ((destination >>> 8) & 0xff) * inverse + 127) / 255;
                    int blue = ((sourceColor & 0xff) * a
                            + (destination & 0xff) * inverse + 127) / 255;
                    data.putInt(offset, 0xff000000 | (red << 16) | (green << 8) | blue);
                }
            }
            data.rewind();
            forceUpdate(x, y, width, height);
        }
    }

    /** Applies a constant straight-alpha ARGB color with the Render Over op. */
    public void blendSolidRect(int x, int y, int width, int height,
                               int sourceColor) {
        if (width <= 0 || height <= 0) return;
        int[] colors = new int[width * height];
        java.util.Arrays.fill(colors, sourceColor);
        blendArgbPixels(x, y, width, height, colors, null, 0, 0, 3);
    }

    /** Applies a solid source through an A8 drawable with the Render Over op. */
    public void blendSolidMask(int maskX, int maskY, int dstX, int dstY,
                               int width, int height, Drawable mask,
                               int sourceColor) {
        if (data == null || mask == null || mask.data == null || visual == null
                || visual.depth != 32) return;
        ensureCpuPixels();
        mask.ensureCpuPixels();
        int sourceAlpha = (sourceColor >>> 24) & 0xff;
        int stride = getStride();
        int maskStride = mask.getStride();
        synchronized (renderLock) {
            for (int row = 0; row < height; row++) {
                int targetY = dstY + row;
                int sourceY = maskY + row;
                if (targetY < 0 || targetY >= this.height
                        || sourceY < 0 || sourceY >= mask.height) continue;
                for (int column = 0; column < width; column++) {
                    int targetX = dstX + column;
                    int sourceX = maskX + column;
                    if (targetX < 0 || targetX >= this.width
                            || sourceX < 0 || sourceX >= mask.width) continue;
                    int maskPixel = mask.data.getInt(
                            (sourceY * maskStride + sourceX) * 4);
                    int maskAlpha = mask.visual != null
                            && mask.visual.depth == 32
                            ? (maskPixel >>> 24) & 0xff : maskPixel & 0xff;
                    int alpha = (maskAlpha * sourceAlpha + 127) / 255;
                    if (alpha == 0) continue;
                    int offset = (targetY * stride + targetX) * 4;
                    int destination = opaqueIfUnusedAlpha(data.getInt(offset));
                    int inverse = 255 - alpha;
                    int red = (((sourceColor >>> 16) & 0xff) * alpha
                            + ((destination >>> 16) & 0xff) * inverse + 127) / 255;
                    int green = (((sourceColor >>> 8) & 0xff) * alpha
                            + ((destination >>> 8) & 0xff) * inverse + 127) / 255;
                    int blue = ((sourceColor & 0xff) * alpha
                            + (destination & 0xff) * inverse + 127) / 255;
                    int destinationAlpha = (destination >>> 24) & 0xff;
                    int resultAlpha = alpha
                            + (destinationAlpha * inverse + 127) / 255;
                    data.putInt(offset, (resultAlpha << 24) | (red << 16)
                            | (green << 8) | blue);
                }
            }
            data.rewind();
            forceUpdate(dstX, dstY, width, height);
        }
    }

    public int getPixelArgb(int x, int y) {
        if (data == null || x < 0 || y < 0 || x >= width || y >= height)
            return 0;
        ensureCpuPixels();
        return data.getInt((y * getStride() + x) * 4);
    }

    /** Applies per-pixel straight-alpha colors, optionally through a mask. */
    public void blendArgbPixels(int dstX, int dstY, int width, int height,
                                int[] sourceColors, Drawable mask,
                                int maskX, int maskY, int operation) {
        if (data == null || visual == null
                || (visual.depth != 8 && visual.depth != 32)) return;
        ensureCpuPixels();
        if (mask != null) mask.ensureCpuPixels();
        int stride = getStride();
        int maskStride = mask != null ? mask.getStride() : 0;
        synchronized (renderLock) {
            for (int row = 0; row < height; row++) {
                int targetY = dstY + row;
                if (targetY < 0 || targetY >= this.height) continue;
                for (int column = 0; column < width; column++) {
                    int targetX = dstX + column;
                    if (targetX < 0 || targetX >= this.width) continue;
                    int source = sourceColors[row * width + column];
                    int alpha = (source >>> 24) & 0xff;
                    if (mask != null) {
                        int sourceX = maskX + column;
                        int sourceY = maskY + row;
                        if (sourceX < 0 || sourceY < 0 || sourceX >= mask.width
                                || sourceY >= mask.height) continue;
                        int maskPixel = mask.data.getInt(
                                (sourceY * maskStride + sourceX) * 4);
                        int maskAlpha = mask.visual != null
                                && mask.visual.depth == 32
                                ? (maskPixel >>> 24) & 0xff : maskPixel & 0xff;
                        alpha = (alpha * maskAlpha + 127) / 255;
                    }
                    int offset = (targetY * stride + targetX) * 4;
                    int destination = data.getInt(offset);
                    if (visual.depth != 8)
                        destination = opaqueIfUnusedAlpha(destination);
                    if (visual.depth == 8) {
                        int destinationAlpha = destination & 0xff;
                        int resultAlpha;
                        if (operation == 1) resultAlpha = alpha;
                        else if (operation == 3)
                            resultAlpha = alpha + (destinationAlpha
                                    * (255 - alpha) + 127) / 255;
                        else if (operation == 5)
                            resultAlpha = (alpha * destinationAlpha + 127) / 255;
                        else if (operation == 8)
                            resultAlpha = (destinationAlpha * (255 - alpha)
                                    + 127) / 255;
                        else if (operation == 12)
                            resultAlpha = Math.min(255, alpha + destinationAlpha);
                        else if (operation == 13)
                            resultAlpha = Math.min(255, alpha + destinationAlpha);
                        else continue;
                        data.putInt(offset, resultAlpha);
                        continue;
                    }
                    if (operation == 1) {
                        data.putInt(offset, alpha == 0 ? 0
                                : (alpha << 24) | (source & 0x00ffffff));
                        continue;
                    }
                    if (operation == 5) {
                        int destinationAlpha = (destination >>> 24) & 0xff;
                        int resultAlpha = (alpha * destinationAlpha + 127) / 255;
                        data.putInt(offset, resultAlpha == 0 ? 0
                                : (resultAlpha << 24)
                                  | (source & 0x00ffffff));
                        continue;
                    }
                    if (operation == 8) {
                        int destinationAlpha = (destination >>> 24) & 0xff;
                        int resultAlpha = (destinationAlpha * (255 - alpha)
                                + 127) / 255;
                        data.putInt(offset, resultAlpha == 0 ? 0
                                : (resultAlpha << 24)
                                  | (destination & 0x00ffffff));
                        continue;
                    }
                    if (operation == 12) {
                        int destinationAlpha = (destination >>> 24) & 0xff;
                        int resultAlpha = Math.min(255,
                                alpha + destinationAlpha);
                        int red = addStraightChannel((source >>> 16) & 0xff,
                                alpha, (destination >>> 16) & 0xff,
                                destinationAlpha, resultAlpha);
                        int green = addStraightChannel((source >>> 8) & 0xff,
                                alpha, (destination >>> 8) & 0xff,
                                destinationAlpha, resultAlpha);
                        int blue = addStraightChannel(source & 0xff, alpha,
                                destination & 0xff, destinationAlpha,
                                resultAlpha);
                        data.putInt(offset, (resultAlpha << 24) | (red << 16)
                                | (green << 8) | blue);
                        continue;
                    }
                    if (operation == 13) {
                        int destinationAlpha = (destination >>> 24) & 0xff;
                        int sourceContribution = Math.min(alpha,
                                255 - destinationAlpha);
                        int resultAlpha = sourceContribution + destinationAlpha;
                        int red = addStraightChannel((source >>> 16) & 0xff,
                                sourceContribution,
                                (destination >>> 16) & 0xff,
                                destinationAlpha, resultAlpha);
                        int green = addStraightChannel((source >>> 8) & 0xff,
                                sourceContribution,
                                (destination >>> 8) & 0xff,
                                destinationAlpha, resultAlpha);
                        int blue = addStraightChannel(source & 0xff,
                                sourceContribution, destination & 0xff,
                                destinationAlpha, resultAlpha);
                        data.putInt(offset, (resultAlpha << 24) | (red << 16)
                                | (green << 8) | blue);
                        continue;
                    }
                    if (alpha == 0) continue;
                    int inverse = 255 - alpha;
                    int destinationAlpha = (destination >>> 24) & 0xff;
                    int resultAlpha = alpha
                            + (destinationAlpha * inverse + 127) / 255;
                    int red = overStraightChannel((source >>> 16) & 0xff,
                            alpha, (destination >>> 16) & 0xff,
                            destinationAlpha, inverse, resultAlpha);
                    int green = overStraightChannel((source >>> 8) & 0xff,
                            alpha, (destination >>> 8) & 0xff,
                            destinationAlpha, inverse, resultAlpha);
                    int blue = overStraightChannel(source & 0xff, alpha,
                            destination & 0xff, destinationAlpha, inverse,
                            resultAlpha);
                    data.putInt(offset, (resultAlpha << 24) | (red << 16)
                            | (green << 8) | blue);
                }
            }
            data.rewind();
            forceUpdate(dstX, dstY, width, height);
        }
    }

    /**
     * Core drawing stores 24-in-32 with an unused alpha byte of 0.
     * Render Over must treat those samples as opaque or a later
     * low-alpha tile (xfwm4 Default title pixmaps) replaces the fill
     * with a nearly transparent color and the frame goes black.
     */
    static int opaqueIfUnusedAlpha(int pixel) {
        if ((pixel >>> 24) == 0 && (pixel & 0x00ffffff) != 0)
            return 0xff000000 | (pixel & 0x00ffffff);
        return pixel;
    }

    private static int addStraightChannel(int source, int sourceAlpha,
            int destination, int destinationAlpha, int resultAlpha) {
        if (resultAlpha == 0) return 0;
        int premultiplied = Math.min(255 * 255,
                source * sourceAlpha + destination * destinationAlpha);
        return Math.min(255, (premultiplied + resultAlpha / 2) / resultAlpha);
    }

    private static int overStraightChannel(int source, int sourceAlpha,
            int destination, int destinationAlpha, int inverseSourceAlpha,
            int resultAlpha) {
        if (resultAlpha == 0) return 0;
        int destinationContribution = (destination * destinationAlpha
                * inverseSourceAlpha + 127) / 255;
        int premultiplied = source * sourceAlpha + destinationContribution;
        return Math.min(255, (premultiplied + resultAlpha / 2) / resultAlpha);
    }

    public void drawLines(int color, int lineWidth, short... points) {
        for (int i = 2; i < points.length; i += 2) {
            drawLine(points[i-2], points[i-1], points[i+0], points[i+1], color, (short)lineWidth);
        }
    }

    public void drawLine(int x0, int y0, int x1, int y1, int color, int lineWidth) {
        if (this.data == null) return;
        ensureCpuPixels();
        x0 = Mathf.clamp(x0, 0, width-lineWidth);
        y0 = Mathf.clamp(y0, 0, height-lineWidth);
        x1 = Mathf.clamp(x1, 0, width-lineWidth);
        y1 = Mathf.clamp(y1, 0, height-lineWidth);

        drawLine((short)x0, (short)y0, (short)x1, (short)y1, color, (short)lineWidth, this.getStride(), this.data);

        this.data.rewind();
        int left = Math.min(x0, x1);
        int top = Math.min(y0, y1);
        forceUpdate(left, top, Math.abs(x1 - x0) + lineWidth,
                Math.abs(y1 - y0) + lineWidth);
    }

    public static final int TEXT8_WIDTH = 8;
    public static final int TEXT8_ASCENT = 11;
    public static final int TEXT8_DESCENT = 3;

    public int drawText8(int x, int baseline, String text, int color) {
        if (data == null || text.isEmpty()) return 0;
        ensureCpuPixels();
        int stride = getStride();
        Bitmap bitmap = Bitmap.createBitmap(stride, height, Bitmap.Config.ARGB_8888);
        data.rewind();
        bitmap.copyPixelsFromBuffer(data);

        Paint paint = text8Paint(color);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawText(text, x, baseline, paint);
        int advance = text.length() * TEXT8_WIDTH;

        data.rewind();
        bitmap.copyPixelsToBuffer(data);
        data.rewind();
        bitmap.recycle();
        forceUpdate(x, baseline - TEXT8_ASCENT, advance,
                TEXT8_ASCENT + TEXT8_DESCENT);
        return advance;
    }

    public int drawImageText8(int x, int baseline, String text, int foreground,
                              int background) {
        int width = text.length() * TEXT8_WIDTH;
        int top = baseline - TEXT8_ASCENT;
        int height = TEXT8_ASCENT + TEXT8_DESCENT;
        if (width > 0 && height > 0) fillRect(x, top, width, height, background);
        if (!text.isEmpty()) drawText8(x, baseline, text, foreground);
        return width;
    }

    private static Paint text8Paint(int color) {
        Paint paint = new Paint();
        paint.setAntiAlias(false);
        paint.setTypeface(Typeface.MONOSPACE);
        paint.setTextSize(TEXT8_ASCENT);
        paint.setColor(0xff000000 | (color & 0x00ffffff));
        return paint;
    }

    public void drawAlphaMaskedBitmap(byte foreRed, byte foreGreen, byte foreBlue, byte backRed, byte backGreen, byte backBlue, Drawable srcDrawable, Drawable maskDrawable) {
        if (this.data == null || srcDrawable.data == null || maskDrawable.data == null) return;
        ensureCpuPixels();
        srcDrawable.ensureCpuPixels();
        maskDrawable.ensureCpuPixels();
        drawAlphaMaskedBitmap(foreRed, foreGreen, foreBlue, backRed, backGreen, backBlue, srcDrawable.data, maskDrawable.data, this.data);
        this.data.rewind();

        forceUpdate();
    }

    public void forceUpdate() {
        forceUpdate(0, 0, width, height);
    }

    public void forceUpdate(int x, int y, int w, int h) {
        synchronized (renderLock) {
            unionDirty(x, y, w, h);
            if (texture != null) texture.setNeedsUpdate(true);
        }
        if (onDrawListener != null) onDrawListener.run();
    }

    public void forceOpaqueRgb(int x, int y, int w, int h) {
        if (data == null || w <= 0 || h <= 0) return;
        if (tryForceOpaqueGpu(x, y, w, h)) return;
        ensureCpuPixels();
        int stride = getStride();
        synchronized (renderLock) {
            for (int row = 0; row < h; row++) {
                int destY = y + row;
                if (destY < 0 || destY >= height) continue;
                for (int column = 0; column < w; column++) {
                    int destX = x + column;
                    if (destX < 0 || destX >= width) continue;
                    int offset = (destY * stride + destX) * 4;
                    int pixel = data.getInt(offset);
                    if ((pixel >>> 24) == 0)
                        data.putInt(offset, 0xff000000 | (pixel & 0x00ffffff));
                }
            }
        }
    }

    private boolean tryFillGpu(int x, int y, int width, int height, int color) {
        if (width <= 0 || height <= 0) return false;
        if (visual == null || visual.depth != 32) return false;
        if (texture == null) return false;
        GLRenderer renderer = xServer != null ? xServer.getRenderer() : null;
        if (renderer == null || !renderer.hasEglContext()) return false;
        java.util.ArrayList<int[]> clips = new java.util.ArrayList<>();
        clips.add(new int[] {x, y, width, height});
        return renderer.fillGpu(this, color, clips);
    }

    private boolean tryForceOpaqueGpu(int x, int y, int w, int h) {
        if (visual == null || visual.depth != 32) return false;
        if (texture == null) return false;
        GLRenderer renderer = xServer != null ? xServer.getRenderer() : null;
        if (renderer == null || !renderer.hasEglContext()) return false;
        return renderer.forceOpaqueGpu(this, x, y, w, h);
    }

    private boolean tryCopyAreaGpu(short srcX, short srcY, short dstX,
            short dstY, short width, short height, Drawable source) {
        if (width <= 0 || height <= 0) return false;
        if (visual == null || visual.depth != 32) return false;
        if (source.visual == null || source.visual.depth != 32) return false;
        if (texture == null || source.texture == null) return false;
        GLRenderer renderer = xServer != null ? xServer.getRenderer() : null;
        if (renderer == null || !renderer.hasEglContext()) return false;
        return renderer.copyAreaGpu(source, srcX, srcY, this, dstX, dstY,
                width, height);
    }

    private void unionGpuDirty(int x, int y, int w, int h) {
        if (w <= 0 || h <= 0) return;
        int left = Math.max(0, x);
        int top = Math.max(0, y);
        int right = Math.min(width, x + w);
        int bottom = Math.min(height, y + h);
        if (right <= left || bottom <= top) return;
        if (!hasGpuDirty) {
            gpuLeft = left;
            gpuTop = top;
            gpuRight = right;
            gpuBottom = bottom;
            hasGpuDirty = true;
            return;
        }
        gpuLeft = Math.min(gpuLeft, left);
        gpuTop = Math.min(gpuTop, top);
        gpuRight = Math.max(gpuRight, right);
        gpuBottom = Math.max(gpuBottom, bottom);
    }

    private void unionDirty(int x, int y, int w, int h) {
        if (w <= 0 || h <= 0) return;
        int left = Math.max(0, x);
        int top = Math.max(0, y);
        int right = Math.min(width, x + w);
        int bottom = Math.min(height, y + h);
        if (right <= left || bottom <= top) return;
        if (!hasDirty) {
            dirtyLeft = left;
            dirtyTop = top;
            dirtyRight = right;
            dirtyBottom = bottom;
            hasDirty = true;
            return;
        }
        dirtyLeft = Math.min(dirtyLeft, left);
        dirtyTop = Math.min(dirtyTop, top);
        dirtyRight = Math.max(dirtyRight, right);
        dirtyBottom = Math.max(dirtyBottom, bottom);
    }

    public int[] takeDirtyRect() {
        if (!hasDirty) return null;
        int[] rect = new int[] {
            dirtyLeft, dirtyTop,
            dirtyRight - dirtyLeft, dirtyBottom - dirtyTop
        };
        hasDirty = false;
        return rect;
    }

    public void clearDirty() {
        hasDirty = false;
    }

    public boolean isUseSharedData() {
        return useSharedData;
    }

    public void setUseSharedData(boolean useSharedData) {
        this.useSharedData = useSharedData;
    }

    private static native void drawBitmap(short width, short height, ByteBuffer srcData, ByteBuffer dstData);

    private static native void drawAlphaMaskedBitmap(byte foreRed, byte foreGreen, byte foreBlue, byte backRed, byte backGreen, byte backBlue, ByteBuffer srcData, ByteBuffer maskData, ByteBuffer dstData);

    private static native void copyArea(short srcX, short srcY, short dstX, short dstY, short width, short height, short srcStride, short dstStride, ByteBuffer srcData, ByteBuffer dstData);

    private static native void copyAreaOp(short srcX, short srcY, short dstX, short dstY, short width, short height, short srcStride, short dstStride, ByteBuffer srcData, ByteBuffer dstData, int gcFunction);

    private static native void fillRect(short x, short y, short width, short height, int color, short stride, ByteBuffer data);

    private static native void drawLine(short x0, short y0, short x1, short y1, int color, short lineWidth, short stride, ByteBuffer data);

    private static native void fromBitmap(Bitmap bitmap, ByteBuffer data);
}
