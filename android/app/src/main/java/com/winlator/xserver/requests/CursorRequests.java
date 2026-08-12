package com.winlator.xserver.requests;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Cursor;
import com.winlator.xserver.Pixmap;
import com.winlator.xserver.XClient;
import com.winlator.xserver.errors.BadIdChoice;
import com.winlator.xserver.errors.BadFont;
import com.winlator.xserver.errors.BadMatch;
import com.winlator.xserver.errors.BadPixmap;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;

public abstract class CursorRequests {
    private static int color(int red, int green, int blue) {
        return 0xff000000 | ((red >>> 8) << 16) | ((green >>> 8) << 8)
                | (blue >>> 8);
    }
    public static void createCursor(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int cursorId = inputStream.readInt();
        int sourcePixmapId = inputStream.readInt();
        int maskPixmapId = inputStream.readInt();

        if (!client.isValidResourceId(cursorId)) throw new BadIdChoice(cursorId);

        Pixmap sourcePixmap = client.xServer.pixmapManager.getPixmap(sourcePixmapId);
        if (sourcePixmap == null) throw new BadPixmap(sourcePixmapId);

        Pixmap maskPixmap = client.xServer.pixmapManager.getPixmap(maskPixmapId);
        if (maskPixmap != null && (
            maskPixmap.drawable.visual.depth != 1 ||
            maskPixmap.drawable.width != sourcePixmap.drawable.width ||
            maskPixmap.drawable.height != sourcePixmap.drawable.height)) {
            throw new BadMatch();
        }

        int foreRed = inputStream.readUnsignedShort();
        int foreGreen = inputStream.readUnsignedShort();
        int foreBlue = inputStream.readUnsignedShort();
        int backRed = inputStream.readUnsignedShort();
        int backGreen = inputStream.readUnsignedShort();
        int backBlue = inputStream.readUnsignedShort();
        short x = inputStream.readShort();
        short y = inputStream.readShort();

        Cursor cursor = client.xServer.cursorManager.createCursor(cursorId, x, y, sourcePixmap, maskPixmap);
        if (cursor == null) throw new BadIdChoice(cursorId);
        client.xServer.cursorManager.recolorCursor(cursor, foreRed, foreGreen, foreBlue, backRed, backGreen, backBlue);
        client.registerAsOwnerOfResource(cursor);
    }

    public static void recolorCursor(XClient client,
                                     XInputStream inputStream,
                                     XOutputStream outputStream)
            throws XRequestError {
        int cursorId = inputStream.readInt();
        Cursor cursor = client.xServer.cursorManager.getCursor(cursorId);
        if (cursor == null)
            throw new com.winlator.xserver.errors.BadCursor(cursorId);
        int foreRed = inputStream.readUnsignedShort();
        int foreGreen = inputStream.readUnsignedShort();
        int foreBlue = inputStream.readUnsignedShort();
        int backRed = inputStream.readUnsignedShort();
        int backGreen = inputStream.readUnsignedShort();
        int backBlue = inputStream.readUnsignedShort();
        client.xServer.cursorManager.recolorCursor(cursor, foreRed, foreGreen,
                foreBlue, backRed, backGreen, backBlue);
    }

    public static void freeCursor(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        client.xServer.cursorManager.freeCursor(inputStream.readInt());
    }

    public static void createGlyphCursor(XClient client,
                                         XInputStream inputStream,
                                         XOutputStream outputStream)
            throws XRequestError {
        int cursorId = inputStream.readInt();
        int sourceFont = inputStream.readInt();
        int maskFont = inputStream.readInt();
        inputStream.skip(4); // source and mask character
        int foreRed = inputStream.readShort() & 0xffff;
        int foreGreen = inputStream.readShort() & 0xffff;
        int foreBlue = inputStream.readShort() & 0xffff;
        int backRed = inputStream.readShort() & 0xffff;
        int backGreen = inputStream.readShort() & 0xffff;
        int backBlue = inputStream.readShort() & 0xffff;
        if (!client.isValidResourceId(cursorId))
            throw new BadIdChoice(cursorId);
        if (!client.hasOpenFont(sourceFont)) throw new BadFont(sourceFont);
        if (maskFont != 0 && !client.hasOpenFont(maskFont))
            throw new BadFont(maskFont);
        Cursor cursor = client.xServer.cursorManager.createGlyphCursor(cursorId,
                color(foreRed, foreGreen, foreBlue),
                color(backRed, backGreen, backBlue));
        if (cursor == null) throw new BadIdChoice(cursorId);
        client.registerAsOwnerOfResource(cursor);
    }

    public static void getPointerMapping(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        try (XStreamLock lock = outputStream.lock()) {
            final byte[] buttonsMap = {1, 2, 3};
            byte length = (byte)buttonsMap.length;

            outputStream.writeByte((byte) 1);
            outputStream.writeByte(length);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt((length + 3) / 4);
            outputStream.writePad(24);
            outputStream.write(buttonsMap);
            outputStream.writePad(-length & 3);
        }
    }
}
