package com.winlator.xserver.requests;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Drawable;
import com.winlator.xserver.GraphicsContext;
import com.winlator.xserver.Window;
import com.winlator.xserver.XClient;
import com.winlator.xserver.errors.BadDrawable;
import com.winlator.xserver.errors.BadGraphicsContext;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.BadMatch;
import com.winlator.xserver.errors.BadWindow;
import com.winlator.xserver.errors.XRequestError;
import com.winlator.xserver.events.Expose;

import java.io.IOException;
import java.nio.ByteBuffer;

public abstract class DrawRequests {
    public enum Format {BITMAP, XY_PIXMAP, Z_PIXMAP}
    private enum CoordinateMode {ORIGIN, PREVIOUS}

    public static void putImage(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        Format format = Format.values()[client.getRequestData()];
        int drawableId = inputStream.readInt();
        int gcId = inputStream.readInt();
        short width = inputStream.readShort();
        short height = inputStream.readShort();
        short dstX = inputStream.readShort();
        short dstY = inputStream.readShort();
        byte leftPad = inputStream.readByte();
        byte depth = inputStream.readByte();
        inputStream.skip(2);
        int length = client.getRemainingRequestLength();
        ByteBuffer data = inputStream.readByteBuffer(length);

        Drawable drawable = client.xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);

        GraphicsContext graphicsContext = client.xServer.graphicsContextManager.getGraphicsContext(gcId);
        if (graphicsContext == null) throw new BadGraphicsContext(gcId);

        if (!(graphicsContext.getFunction() == GraphicsContext.Function.COPY || format == Format.Z_PIXMAP)) {
            throw new UnsupportedOperationException("GC Function other than COPY is not supported.");
        }

        switch (format) {
            case BITMAP:
                if (leftPad != 0) throw new UnsupportedOperationException("PutImage.leftPad cannot be != 0.");
                if (depth == 1) {
                    drawable.drawImage((short)0, (short)0, dstX, dstY, width, height, (byte)1, data, width, height);
                }
                else throw new BadMatch();
                break;
            case XY_PIXMAP:
                if (drawable.visual.depth != depth) throw new BadMatch();
                break;
            case Z_PIXMAP:
                if (leftPad == 0) {
                    drawable.drawImage((short)0, (short)0, dstX, dstY, width, height, depth, data, width, height);
                }
                else throw new BadMatch();
                break;
        }
    }

    public static void getImage(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        Format format = Format.values()[client.getRequestData()];
        int drawableId = inputStream.readInt();
        short x = inputStream.readShort();
        short y = inputStream.readShort();
        short width = inputStream.readShort();
        short height = inputStream.readShort();
        inputStream.skip(4);

        if (format != Format.Z_PIXMAP) throw new UnsupportedOperationException("Only Z_PIXMAP is supported.");

        Drawable drawable =  client.xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);
        int visualId = client.xServer.pixmapManager.getPixmap(drawableId) == null ? drawable.visual.id : 0;
        ByteBuffer data = drawable.getImage(x, y, width, height);
        int length = data.limit();

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(drawable.visual.depth);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt((length + 3) / 4);
            outputStream.writeInt(visualId);
            outputStream.writePad(20);
            outputStream.write(data);
            if ((-length & 3) > 0) outputStream.writePad(-length & 3);
        }
    }

    public static void clearArea(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        boolean exposures = client.getRequestData() == 1;
        int windowId = inputStream.readInt();
        short x = inputStream.readShort();
        short y = inputStream.readShort();
        short width = inputStream.readShort();
        short height = inputStream.readShort();

        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        if (!window.isInputOutput()) throw new BadMatch();

        window.attributes.clearBackground(x, y, width, height);

        if (exposures) window.sendEvent(new Expose(window));
    }

    public static void copyArea(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int srcDrawableId = inputStream.readInt();
        int dstDrawableId = inputStream.readInt();
        int gcId = inputStream.readInt();
        short srcX = inputStream.readShort();
        short srcY = inputStream.readShort();
        short dstX = inputStream.readShort();
        short dstY = inputStream.readShort();
        short width = inputStream.readShort();
        short height = inputStream.readShort();

        Drawable srcDrawable =  client.xServer.drawableManager.getDrawable(srcDrawableId);
        if (srcDrawable == null) throw new BadDrawable(srcDrawableId);

        Drawable dstDrawable =  client.xServer.drawableManager.getDrawable(dstDrawableId);
        if (dstDrawable == null) throw new BadDrawable(dstDrawableId);

        GraphicsContext graphicsContext =  client.xServer.graphicsContextManager.getGraphicsContext(gcId);
        if (graphicsContext == null) throw new BadGraphicsContext(gcId);

        if (srcDrawable.visual.depth != dstDrawable.visual.depth) throw new BadMatch();

        synchronized (dstDrawable.renderLock) {
            if (graphicsContext.getClipRectangles() == null) {
                dstDrawable.copyArea(srcX, srcY, dstX, dstY, width, height,
                        srcDrawable, graphicsContext.getFunction());
            }
            else for (GraphicsContext.ClipRectangle clip
                    : graphicsContext.getClipRectangles()) {
                int clipLeft = graphicsContext.getClipXOrigin() + clip.x;
                int clipTop = graphicsContext.getClipYOrigin() + clip.y;
                int left = Math.max(dstX, clipLeft);
                int top = Math.max(dstY, clipTop);
                int right = Math.min(dstX + width, clipLeft + clip.width);
                int bottom = Math.min(dstY + height, clipTop + clip.height);
                if (left >= right || top >= bottom) continue;
                dstDrawable.copyArea((short)(srcX + left - dstX),
                        (short)(srcY + top - dstY), (short)left, (short)top,
                        (short)(right - left), (short)(bottom - top),
                        srcDrawable, graphicsContext.getFunction());
            }
        }
    }

    public static void polyLine(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        CoordinateMode coordinateMode = CoordinateMode.values()[client.getRequestData()];
        int drawableId = inputStream.readInt();
        int gcId = inputStream.readInt();

        Drawable drawable = client.xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);
        GraphicsContext graphicsContext = client.xServer.graphicsContextManager.getGraphicsContext(gcId);
        if (graphicsContext == null) throw new BadGraphicsContext(gcId);
        int length = client.getRemainingRequestLength();

        short[] points = new short[length / 2];
        int i = 0;
        while (length != 0) {
            points[i++] = inputStream.readShort();
            points[i++] = inputStream.readShort();
            length -= 4;
        }

        if (coordinateMode == CoordinateMode.ORIGIN && graphicsContext.getLineWidth() > 0) {
            drawable.drawLines(graphicsContext.getForeground(), graphicsContext.getLineWidth(), points);
        }
    }

    public static void polyPoint(XClient client, XInputStream inputStream,
                                 XOutputStream outputStream)
            throws XRequestError {
        int mode = client.getRequestData();
        if (mode < 0 || mode >= CoordinateMode.values().length)
            throw new BadValue(mode);
        CoordinateMode coordinateMode = CoordinateMode.values()[mode];
        int drawableId = inputStream.readInt();
        int gcId = inputStream.readInt();
        Drawable drawable = client.xServer.drawableManager
                .getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);
        GraphicsContext graphicsContext = client.xServer.graphicsContextManager
                .getGraphicsContext(gcId);
        if (graphicsContext == null) throw new BadGraphicsContext(gcId);

        int remaining = client.getRemainingRequestLength();
        int x = 0;
        int y = 0;
        while (remaining >= 4) {
            int requestX = inputStream.readShort();
            int requestY = inputStream.readShort();
            if (coordinateMode == CoordinateMode.PREVIOUS) {
                x += requestX;
                y += requestY;
            }
            else {
                x = requestX;
                y = requestY;
            }
            boolean visible = graphicsContext.getClipRectangles() == null;
            if (!visible) {
                for (GraphicsContext.ClipRectangle clip
                        : graphicsContext.getClipRectangles()) {
                    int left = graphicsContext.getClipXOrigin() + clip.x;
                    int top = graphicsContext.getClipYOrigin() + clip.y;
                    if (x >= left && y >= top && x < left + clip.width
                            && y < top + clip.height) {
                        visible = true;
                        break;
                    }
                }
            }
            if (visible) drawable.fillRect(x, y, 1, 1,
                    graphicsContext.getForeground());
            remaining -= 4;
        }
        if (remaining > 0) inputStream.skip(remaining);
    }

    public static void polyFillRectangle(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int drawableId = inputStream.readInt();
        int gcId = inputStream.readInt();

        Drawable drawable = client.xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);
        GraphicsContext graphicsContext = client.xServer.graphicsContextManager.getGraphicsContext(gcId);
        if (graphicsContext == null) throw new BadGraphicsContext(gcId);
        int length = client.getRemainingRequestLength();

        while (length != 0) {
            short x = inputStream.readShort();
            short y = inputStream.readShort();
            short width = inputStream.readShort();
            short height = inputStream.readShort();
            java.util.List<GraphicsContext.ClipRectangle> clips =
                    graphicsContext.getClipRectangles();
            if (clips == null) {
                drawable.fillRect(x, y, width, height, graphicsContext.getForeground());
            } else {
                int color = graphicsContext.getForeground();
                for (GraphicsContext.ClipRectangle clip : clips) {
                    int left = Math.max(x, graphicsContext.getClipXOrigin() + clip.x);
                    int top = Math.max(y, graphicsContext.getClipYOrigin() + clip.y);
                    int right = Math.min(x + width,
                            graphicsContext.getClipXOrigin() + clip.x + clip.width);
                    int bottom = Math.min(y + height,
                            graphicsContext.getClipYOrigin() + clip.y + clip.height);
                    if (left < right && top < bottom) {
                        drawable.fillRect(left, top, right - left, bottom - top, color);
                    }
                }
            }
            length -= 8;
        }
    }

    public static void polyText8(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int drawableId = inputStream.readInt();
        int gcId = inputStream.readInt();
        int x = inputStream.readShort();
        int y = inputStream.readShort();

        Drawable drawable = client.xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);
        GraphicsContext graphicsContext = client.xServer.graphicsContextManager.getGraphicsContext(gcId);
        if (graphicsContext == null) throw new BadGraphicsContext(gcId);

        int remaining = client.getRemainingRequestLength();
        while (remaining > 0) {
            int length = inputStream.readUnsignedByte();
            --remaining;
            if (length == 255) {
                if (remaining < 4) {
                    inputStream.skip(remaining);
                    break;
                }
                inputStream.skip(4); // FontShift; core font selection is not yet modeled.
                remaining -= 4;
                continue;
            }
            if (remaining == 0) break; // Four-byte request padding.
            int delta = inputStream.readByte();
            --remaining;
            if (length > remaining) {
                inputStream.skip(remaining);
                break;
            }
            byte[] bytes = new byte[length];
            inputStream.read(bytes);
            remaining -= length;
            x += delta;
            String text = new String(bytes, client.xServer.LATIN1_CHARSET);
            x += drawable.drawText8(x, y, text, graphicsContext.getForeground());
        }
    }

    public static void imageText8(XClient client, XInputStream inputStream,
                                  XOutputStream outputStream) throws XRequestError {
        int drawableId = inputStream.readInt();
        int gcId = inputStream.readInt();
        int x = inputStream.readShort();
        int y = inputStream.readShort();
        int count = client.getRequestData() & 0xff;
        int remaining = client.getRemainingRequestLength();
        if (count > remaining) count = remaining;
        byte[] bytes = new byte[count];
        if (count > 0) inputStream.read(bytes);
        if (remaining > count) inputStream.skip(remaining - count);

        Drawable drawable = client.xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);
        GraphicsContext graphicsContext = client.xServer.graphicsContextManager
                .getGraphicsContext(gcId);
        if (graphicsContext == null) throw new BadGraphicsContext(gcId);

        String text = new String(bytes, client.xServer.LATIN1_CHARSET);
        drawable.drawImageText8(x, y, text, graphicsContext.getForeground(),
                graphicsContext.getBackground());
    }

    public static void imageText16(XClient client, XInputStream inputStream,
                                   XOutputStream outputStream) throws XRequestError {
        int drawableId = inputStream.readInt();
        int gcId = inputStream.readInt();
        int x = inputStream.readShort();
        int y = inputStream.readShort();
        int count = client.getRequestData() & 0xff;
        int remaining = client.getRemainingRequestLength();
        int available = remaining / 2;
        if (count > available) count = available;
        byte[] latin1 = new byte[count];
        for (int i = 0; i < count; i++) {
            int byte1 = inputStream.readUnsignedByte();
            int byte2 = inputStream.readUnsignedByte();
            latin1[i] = (byte)(byte1 == 0 ? byte2 : '?');
        }
        int consumed = count * 2;
        if (remaining > consumed) inputStream.skip(remaining - consumed);

        Drawable drawable = client.xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);
        GraphicsContext graphicsContext = client.xServer.graphicsContextManager
                .getGraphicsContext(gcId);
        if (graphicsContext == null) throw new BadGraphicsContext(gcId);

        String text = new String(latin1, client.xServer.LATIN1_CHARSET);
        drawable.drawImageText8(x, y, text, graphicsContext.getForeground(),
                graphicsContext.getBackground());
    }
}
