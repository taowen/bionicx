package com.winlator.xserver;

public class Cursor extends XResource {
    public final int hotSpotX;
    public final int hotSpotY;
    public final Drawable cursorImage;
    public final Drawable sourceImage;
    public final Drawable maskImage;
    public final boolean argb;
    private boolean visible = true;

    public Cursor(int id, int hotSpotX, int hotSpotY, Drawable cursorImage, Drawable sourceImage, Drawable maskImage) {
        this(id, hotSpotX, hotSpotY, cursorImage, sourceImage, maskImage, false);
    }

    public Cursor(int id, int hotSpotX, int hotSpotY, Drawable cursorImage,
                  Drawable sourceImage, Drawable maskImage, boolean argb) {
        super(id);
        this.hotSpotX = hotSpotX;
        this.hotSpotY = hotSpotY;
        this.cursorImage = cursorImage;
        this.sourceImage = sourceImage;
        this.maskImage = maskImage;
        this.argb = argb;
    }

    public boolean isVisible() {
        return visible;
    }

    public void setVisible(boolean visible) {
        this.visible = visible;
    }
}
