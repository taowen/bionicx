package com.winlator.xserver;

import com.winlator.core.Bitmask;
import com.winlator.xconnector.XInputStream;
import com.winlator.xserver.errors.BadMatch;
import com.winlator.xserver.errors.BadPixmap;
import com.winlator.xserver.errors.XRequestError;

public class WindowAttributes {
    public static final int FLAG_BACKGROUND_PIXMAP = 1<<0;
    public static final int FLAG_BACKGROUND_PIXEL = 1<<1;
    public static final int FLAG_BORDER_PIXMAP = 1<<2;
    public static final int FLAG_BORDER_PIXEL = 1<<3;
    public static final int FLAG_BIT_GRAVITY = 1<<4;
    public static final int FLAG_WIN_GRAVITY = 1<<5;
    public static final int FLAG_BACKING_STORE = 1<<6;
    public static final int FLAG_BACKING_PLANES = 1<<7;
    public static final int FLAG_BACKING_PIXEL = 1<<8;
    public static final int FLAG_OVERRIDE_REDIRECT = 1<<9;
    public static final int FLAG_SAVE_UNDER = 1<<10;
    public static final int FLAG_EVENT_MASK = 1<<11;
    public static final int FLAG_DO_NOT_PROPAGATE_MASK = 1<<12;
    public static final int FLAG_COLORMAP = 1<<13;
    public static final int FLAG_CURSOR = 1<<14;
    public static final int FLAG_MAPPED = 1<<15;
    public static final int FLAG_ENABLED = 1<<16;
    public static final int FLAG_TRANSPARENT = 1<<17;
    public static final int FLAG_RENDER_SUBWINDOWS = 1<<18;
    public static final int FLAG_VIEWABLE = 1<<19;
    public enum BackingStore {NOT_USEFUL, WHEN_MAPPED, ALWAYS}
    public enum WindowClass {COPY_FROM_PARENT, INPUT_OUTPUT, INPUT_ONLY}
    public enum BitGravity {FORGET, NORTH_WEST, NORTH, NORTH_EAST, WEST, CENTER, EAST, SOUTH_WEST, SOUTH, SOUTH_EAST, STATIC}
    public enum WinGravity {UNMAP, NORTH_WEST, NORTH, NORTH_EAST, WEST, CENTER, EAST, SOUTH_WEST, SOUTH, SOUTH_EAST, STATIC}
    private int backingPixel = 0;
    private int backingPlanes = 1;
    private BackingStore backingStore = BackingStore.NOT_USEFUL;
    private BitGravity bitGravity = BitGravity.CENTER;
    private Cursor cursor;
    private Bitmask doNotPropagateMask = new Bitmask(0);
    private Bitmask eventMask = new Bitmask(0);
    private int backgroundPixel;
    private Drawable backgroundPixmap;
    private boolean parentRelativeBackground;
    private boolean hasBackground;
    private WinGravity winGravity = WinGravity.CENTER;
    private WindowClass windowClass = WindowClass.INPUT_OUTPUT;
    private final Bitmask attributeFlags = new Bitmask(new int[]{FLAG_ENABLED, FLAG_RENDER_SUBWINDOWS, FLAG_VIEWABLE});
    public final Window window;

    public WindowAttributes(Window window) {
        this.window = window;
    }

    public int getBackingPixel() {
        return backingPixel;
    }

    public int getBackingPlanes() {
        return backingPlanes;
    }

    public BackingStore getBackingStore() {
        return backingStore;
    }

    public BitGravity getBitGravity() {
        return bitGravity;
    }

    public Cursor getCursor() {
        Window parent = window.getParent();
        return cursor == null && parent != null ? parent.attributes.getCursor() : cursor;
    }

    public Cursor getAssignedCursor() {
        return cursor;
    }

    public void setCursor(Cursor cursor) {
        this.cursor = cursor;
    }

    public Bitmask getEventMask() {
        return eventMask;
    }

    public Bitmask getDoNotPropagateMask() {
        return doNotPropagateMask;
    }

    public boolean isMapped() {
        return attributeFlags.isSet(FLAG_MAPPED);
    }

    public void setMapped(boolean mapped) {
        attributeFlags.set(FLAG_MAPPED, mapped);
    }

    public boolean isOverrideRedirect() {
        return attributeFlags.isSet(FLAG_OVERRIDE_REDIRECT);
    }

    public void setOverrideRedirect(boolean overrideRedirect) {
        attributeFlags.set(FLAG_OVERRIDE_REDIRECT, overrideRedirect);
    }

    public boolean isSaveUnder() {
        return attributeFlags.isSet(FLAG_SAVE_UNDER);
    }

    public WinGravity getWinGravity() {
        return winGravity;
    }

    public WindowClass getWindowClass() {
        return windowClass;
    }

    public void setWindowClass(WindowClass windowClass) {
        this.windowClass = windowClass;
    }

    public Window getWindow() {
        return window;
    }

    public boolean isEnabled() {
        return attributeFlags.isSet(FLAG_ENABLED);
    }

    public void setEnabled(boolean enabled) {
        attributeFlags.set(FLAG_ENABLED, enabled);
    }

    public boolean isRenderSubwindows() {
        return attributeFlags.isSet(FLAG_RENDER_SUBWINDOWS);
    }

    public void setRenderSubwindows(boolean renderSubwindows) {
        attributeFlags.set(FLAG_RENDER_SUBWINDOWS, renderSubwindows);
    }

    public boolean isViewable() {
        return attributeFlags.isSet(FLAG_VIEWABLE);
    }

    public void setViewable(boolean viewable) {
        attributeFlags.set(FLAG_VIEWABLE, viewable);
    }

    public void update(Bitmask valueMask, XInputStream inputStream,
                       XClient client) throws XRequestError {
        for (int index : valueMask) {
            switch (index) {
                case FLAG_BACKGROUND_PIXEL:
                    backgroundPixel = inputStream.readInt();
                    backgroundPixmap = null;
                    parentRelativeBackground = false;
                    hasBackground = true;
                    // XChangeWindowAttributes(CWBackPixel) only stores the
                    // color used by later Map/ClearArea. GDK maps a child,
                    // then tmp_reset_bg sets the pixel again; filling here
                    // would wipe the Expose paint (blank GtkTextView).
                    break;
                case FLAG_BACKING_PIXEL:
                    backingPixel = inputStream.readInt();
                    break;
                case FLAG_BACKING_PLANES:
                    backingPlanes = inputStream.readInt();
                    break;
                case FLAG_BIT_GRAVITY:
                    bitGravity = BitGravity.values()[inputStream.readInt()];
                    break;
                case FLAG_WIN_GRAVITY:
                    winGravity = WinGravity.values()[inputStream.readInt()];
                    break;
                case FLAG_BACKING_STORE:
                    backingStore = BackingStore.values()[inputStream.readInt()];
                    break;
                case FLAG_SAVE_UNDER:
                case FLAG_OVERRIDE_REDIRECT:
                    attributeFlags.set(index, inputStream.readInt() == 1);
                    break;
                case FLAG_EVENT_MASK:
                    eventMask = new Bitmask(inputStream.readInt());
                    break;
                case FLAG_DO_NOT_PROPAGATE_MASK:
                    doNotPropagateMask = new Bitmask(inputStream.readInt());
                    break;
                case FLAG_CURSOR:
                    cursor = client.xServer.cursorManager.getCursor(inputStream.readInt());
                    break;
                case FLAG_BACKGROUND_PIXMAP:
                    int pixmapId = inputStream.readInt();
                    backgroundPixmap = null;
                    parentRelativeBackground = pixmapId == 1;
                    hasBackground = pixmapId != 0;
                    if (parentRelativeBackground) {
                        Window parent = window.getParent();
                        if (parent == null || !parent.isInputOutput()
                                || parent.getContent().visual.depth
                                != window.getContent().visual.depth)
                            throw new BadMatch();
                    }
                    else if (pixmapId != 0) {
                        Pixmap pixmap = client.xServer.pixmapManager
                                .getPixmap(pixmapId);
                        if (pixmap == null) throw new BadPixmap(pixmapId);
                        if (pixmap.drawable.visual.depth
                                != window.getContent().visual.depth)
                            throw new BadMatch();
                        backgroundPixmap = pixmap.drawable;
                    }
                    break;
                case FLAG_BORDER_PIXMAP:
                case FLAG_BORDER_PIXEL:
                case FLAG_COLORMAP:
                    inputStream.skip(4);
                    break;
            }
        }

        client.xServer.windowManager.triggerOnUpdateWindowAttributes(window, valueMask);
    }

    public void clearBackground(int x, int y, int width, int height) {
        Drawable destination = window.getContent();
        int left = Math.max(0, x);
        int top = Math.max(0, y);
        int right = width == 0 ? destination.width
                : Math.min(destination.width, x + width);
        int bottom = height == 0 ? destination.height
                : Math.min(destination.height, y + height);
        if (left >= right || top >= bottom) return;
        if (!hasBackground) return;

        if (parentRelativeBackground) {
            Window parent = window.getParent();
            if (parent != null && parent.isInputOutput())
                destination.copyArea((short)(window.getX() + left),
                        (short)(window.getY() + top), (short)left, (short)top,
                        (short)(right - left), (short)(bottom - top),
                        parent.getContent());
            return;
        }
        if (backgroundPixmap == null) {
            destination.fillRect(left, top, right - left, bottom - top,
                    backgroundPixel);
            return;
        }

        int tileWidth = backgroundPixmap.width;
        int tileHeight = backgroundPixmap.height;
        java.util.ArrayList<int[]> copies = new java.util.ArrayList<>();
        for (int tileY = Math.floorDiv(top, tileHeight) * tileHeight;
                tileY < bottom; tileY += tileHeight) {
            for (int tileX = Math.floorDiv(left, tileWidth) * tileWidth;
                    tileX < right; tileX += tileWidth) {
                int copyLeft = Math.max(left, tileX);
                int copyTop = Math.max(top, tileY);
                int copyRight = Math.min(right, tileX + tileWidth);
                int copyBottom = Math.min(bottom, tileY + tileHeight);
                copies.add(new int[] {
                        copyLeft - tileX, copyTop - tileY, copyLeft, copyTop,
                        copyRight - copyLeft, copyBottom - copyTop});
            }
        }
        destination.copyAreas(backgroundPixmap, GraphicsContext.Function.COPY,
                copies);
    }

    public boolean isTransparent() {
        return attributeFlags.isSet(FLAG_TRANSPARENT);
    }

    public void setTransparent(boolean transparent) {
        attributeFlags.set(FLAG_TRANSPARENT, transparent);
    }
}
