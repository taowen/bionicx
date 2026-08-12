package com.winlator.xserver;

import android.util.SparseArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;

import java.nio.IntBuffer;

public class CursorManager extends XResourceManager {
    private final SparseArray<Cursor> cursors = new SparseArray<>();
    private final DrawableManager drawableManager;

    public CursorManager(DrawableManager drawableManager) {
        this.drawableManager = drawableManager;
    }

    public Cursor getCursor(int id) {
        return cursors.get(id);
    }

    public Cursor createCursor(int id, short x, short y, Pixmap sourcePixmap, Pixmap maskPixmap) {
        if (cursors.indexOfKey(id) >= 0) return null;
        Drawable drawable = drawableManager.createDrawable(0, sourcePixmap.drawable.width, sourcePixmap.drawable.height, sourcePixmap.drawable.visual);
        Cursor cursor = new Cursor(id, x, y, drawable, sourcePixmap.drawable, maskPixmap != null ? maskPixmap.drawable : null);
        cursors.put(id, cursor);
        triggerOnCreateResourceListener(cursor);
        return cursor;
    }

    private static void paintGlyphCursor(Drawable drawable, int foreground,
                                         int background) {
        Bitmap bitmap = Bitmap.createBitmap(17, 17, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Paint outline = new Paint();
        outline.setColor(background);
        outline.setStrokeWidth(5.0f);
        canvas.drawLine(8, 1, 8, 15, outline);
        canvas.drawLine(1, 8, 15, 8, outline);
        Paint stroke = new Paint();
        stroke.setColor(foreground);
        stroke.setStrokeWidth(2.0f);
        canvas.drawLine(8, 1, 8, 15, stroke);
        canvas.drawLine(1, 8, 15, 8, stroke);
        drawable.getData().rewind();
        bitmap.copyPixelsToBuffer(drawable.getData());
        drawable.getData().rewind();
        drawable.forceUpdate();
        bitmap.recycle();
    }

    public Cursor createGlyphCursor(int id, int foreground, int background) {
        if (cursors.indexOfKey(id) >= 0) return null;
        Bitmap bitmap = Bitmap.createBitmap(17, 17, Bitmap.Config.ARGB_8888);
        Drawable drawable = Drawable.fromBitmap(bitmap);
        bitmap.recycle();
        paintGlyphCursor(drawable, foreground, background);
        Cursor cursor = new Cursor(id, 8, 8, drawable, drawable, null);
        cursors.put(id, cursor);
        triggerOnCreateResourceListener(cursor);
        return cursor;
    }

    public void freeCursor(int id) {
        triggerOnFreeResourceListener(cursors.get(id));
        cursors.remove(id);
    }

    private static boolean isEmptyMaskImage(Drawable maskImage) {
        IntBuffer maskData = maskImage.getData().asIntBuffer();
        boolean result = true;
        for (int i = 0; i < maskData.capacity(); i++) {
            if (maskData.get(i) != 0x000000) {
                result = false;
                break;
            }
        }
        return result;
    }

    public void recolorCursor(Cursor cursor, int foreRed, int foreGreen,
                              int foreBlue, int backRed, int backGreen,
                              int backBlue) {
        byte foreRed8 = (byte)(foreRed >>> 8);
        byte foreGreen8 = (byte)(foreGreen >>> 8);
        byte foreBlue8 = (byte)(foreBlue >>> 8);
        byte backRed8 = (byte)(backRed >>> 8);
        byte backGreen8 = (byte)(backGreen >>> 8);
        byte backBlue8 = (byte)(backBlue >>> 8);
        if (cursor.maskImage != null) {
            boolean visible = !isEmptyMaskImage(cursor.maskImage);
            cursor.setVisible(visible);
            if (visible) cursor.cursorImage.drawAlphaMaskedBitmap(foreRed8,
                    foreGreen8, foreBlue8, backRed8, backGreen8, backBlue8,
                    cursor.sourceImage, cursor.maskImage);
        }
        else if (cursor.sourceImage == cursor.cursorImage)
            paintGlyphCursor(cursor.cursorImage,
                    0xff000000 | ((foreRed >>> 8) << 16)
                            | ((foreGreen >>> 8) << 8) | (foreBlue >>> 8),
                    0xff000000 | ((backRed >>> 8) << 16)
                            | ((backGreen >>> 8) << 8) | (backBlue >>> 8));
    }
}
