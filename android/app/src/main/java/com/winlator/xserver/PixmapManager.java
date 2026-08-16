package com.winlator.xserver;

import android.graphics.Bitmap;
import android.util.SparseArray;

public class PixmapManager extends XResourceManager {
    public final Visual visual;
    public final Visual[] supportedVisuals;
    public final PixmapFormat[] supportedPixmapFormats;
    private final SparseArray<Pixmap> pixmaps = new SparseArray<>();

    public PixmapManager() {
        visual = new Visual(IDGenerator.generate(), true, 32, 24, 0xff0000, 0x00ff00, 0x0000ff);
        supportedVisuals = new Visual[]{
            visual,
            new Visual(IDGenerator.generate(), false, 8, 8, 0, 0, 0),
            new Visual(IDGenerator.generate(), false, 1, 1, 0, 0, 0)
        };

        supportedPixmapFormats = new PixmapFormat[] {
            new PixmapFormat(1, 1, 32),
            new PixmapFormat(8, 8, 32),
            new PixmapFormat(24, 32, 32),
            new PixmapFormat(32, 32, 32)
        };
    }

    public Pixmap getPixmap(int id) {
        return pixmaps.get(id);
    }

    public Pixmap createPixmap(Drawable drawable) {
        if (pixmaps.indexOfKey(drawable.id) >= 0) return null;
        Pixmap pixmap = new Pixmap(drawable);
        pixmaps.put(drawable.id, pixmap);
        triggerOnCreateResourceListener(pixmap);
        return pixmap;
    }

    public void freePixmap(int id) {
        triggerOnFreeResourceListener(pixmaps.get(id));
        pixmaps.remove(id);
    }

    public int countForClient(int idBase, int idMask) {
        int count = 0;
        for (int i = 0; i < pixmaps.size(); i++) {
            if ((pixmaps.keyAt(i) | idMask) == (idBase | idMask)) count++;
        }
        return count;
    }

    public long bytesForClient(int idBase, int idMask) {
        long bytes = 0;
        for (int i = 0; i < pixmaps.size(); i++) {
            if ((pixmaps.keyAt(i) | idMask) != (idBase | idMask)) continue;
            Pixmap pixmap = pixmaps.valueAt(i);
            bytes += (long)pixmap.drawable.width * pixmap.drawable.height * 4;
        }
        return bytes;
    }

    public Visual getVisualForDepth(byte depth) {
        // Depth 24 is advertised as a pixmap format and stored in the same
        // 32-bit TrueColor buffer as the screen visual (8 bits unused).
        if (depth == visual.depth || depth == 24) return visual;
        for (Visual visual : supportedVisuals) {
            if (depth == visual.depth) return visual;
        }
        return null;
    }

    public Visual getVisual(int id) {
        if (id == visual.id) return visual;
        for (Visual visual : supportedVisuals) {
            if (id == visual.id && visual.displayable) return visual;
        }
        return null;
    }

    public Bitmap getWindowIcon(Window window) {
        int colorPixmapId = window.getWMHintsValue(Window.WMHints.ICON_PIXMAP);
        int maskPixmapId = window.getWMHintsValue(Window.WMHints.ICON_MASK);
        Pixmap colorPixmap = colorPixmapId != 0 ? getPixmap(colorPixmapId) : null;
        Pixmap maskPixmap = maskPixmapId != 0 ? getPixmap(maskPixmapId) : null;
        return colorPixmap != null ? colorPixmap.toBitmap(maskPixmap) : null;
    }
}
