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

    public Cursor createGlyphCursor(int id, int foreground, int background) {
        if (cursors.indexOfKey(id) >= 0) return null;
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
        Drawable drawable = Drawable.fromBitmap(bitmap);
        bitmap.recycle();
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

    public void recolorCursor(Cursor cursor, byte foreRed, byte foreGreen, byte foreBlue, byte backRed, byte backGreen, byte backBlue) {
        if (cursor.maskImage != null) {
            boolean visible = !isEmptyMaskImage(cursor.maskImage);
            cursor.setVisible(visible);
            if (visible) cursor.cursorImage.drawAlphaMaskedBitmap(foreRed, foreGreen, foreBlue, backRed, backGreen, backBlue, cursor.sourceImage, cursor.maskImage);
        }
    }
}
