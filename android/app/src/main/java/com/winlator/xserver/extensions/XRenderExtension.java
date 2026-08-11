package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import android.util.SparseArray;

import com.winlator.core.Callback;
import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Drawable;
import com.winlator.xserver.IDGenerator;
import com.winlator.xserver.Visual;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadDrawable;
import com.winlator.xserver.errors.BadIdChoice;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadMatch;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;
import java.util.ArrayList;
import java.util.IdentityHashMap;

/**
 * Minimal stateful implementation of the initial Render protocol used by
 * software-rendered desktop clients. Unsupported operations are not
 * advertised through a newer protocol version.
 */
public class XRenderExtension extends Extension {
    public static final int MAJOR_VERSION = 0;
    public static final int MINOR_VERSION = 1;

    private static final byte PICT_TYPE_DIRECT = 1;
    private static final byte PICT_OP_SRC = 1;
    private static final int SUBPIXEL_UNKNOWN = 0;

    private final int argb32Format = IDGenerator.generate();
    private final int a1Format = IDGenerator.generate();
    private final SparseArray<Picture> pictures = new SparseArray<>();
    private final IdentityHashMap<XClient, ArrayList<Integer>> clientPictures =
            new IdentityHashMap<>();
    private final Callback<XClient> onClientDestroy = this::freeClientPictures;

    private static abstract class ClientOpcodes {
        private static final byte QUERY_VERSION = 0;
        private static final byte QUERY_PICT_FORMATS = 1;
        private static final byte CREATE_PICTURE = 4;
        private static final byte FREE_PICTURE = 7;
        private static final byte FILL_RECTANGLES = 26;
    }

    private static final class Picture {
        private final int id;
        private final Drawable drawable;
        private final int format;

        private Picture(int id, Drawable drawable, int format) {
            this.id = id;
            this.drawable = drawable;
            this.format = format;
        }
    }

    public XRenderExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
    }

    @Override
    public String getName() {
        return "RENDER";
    }

    private void queryVersion(XClient client, XInputStream inputStream,
                              XOutputStream outputStream) throws IOException {
        inputStream.skip(8);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(MAJOR_VERSION);
            outputStream.writeInt(MINOR_VERSION);
            outputStream.writePad(16);
        }
    }

    private void writeDirectFormat(XOutputStream outputStream, int id, int depth,
                                   int red, int redMask, int green, int greenMask,
                                   int blue, int blueMask, int alpha, int alphaMask) {
        outputStream.writeInt(id);
        outputStream.writeByte(PICT_TYPE_DIRECT);
        outputStream.writeByte((byte)depth);
        outputStream.writeShort((short)0);
        outputStream.writeShort((short)red);
        outputStream.writeShort((short)redMask);
        outputStream.writeShort((short)green);
        outputStream.writeShort((short)greenMask);
        outputStream.writeShort((short)blue);
        outputStream.writeShort((short)blueMask);
        outputStream.writeShort((short)alpha);
        outputStream.writeShort((short)alphaMask);
        outputStream.writeInt(0); // no colormap for direct formats
    }

    private void queryPictFormats(XClient client, XOutputStream outputStream)
            throws IOException {
        Visual visual = xServer.pixmapManager.visual;
        int payloadBytes = 2 * 28 + 8 + (8 + 8) + 8 + 4;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(payloadBytes / 4);
            outputStream.writeInt(2); // formats
            outputStream.writeInt(1); // screens
            outputStream.writeInt(2); // depths
            outputStream.writeInt(1); // visuals
            outputStream.writeInt(1); // subpixel orders
            outputStream.writeInt(0);

            writeDirectFormat(outputStream, argb32Format, 32,
                    16, 0xff, 8, 0xff, 0, 0xff, 24, 0xff);
            writeDirectFormat(outputStream, a1Format, 1,
                    0, 0, 0, 0, 0, 0, 0, 1);

            outputStream.writeInt(2); // depths on this screen
            outputStream.writeInt(argb32Format); // fallback

            outputStream.writeByte((byte)32);
            outputStream.writeByte((byte)0);
            outputStream.writeShort((short)1);
            outputStream.writeInt(0);
            outputStream.writeInt(visual.id);
            outputStream.writeInt(argb32Format);

            outputStream.writeByte((byte)1);
            outputStream.writeByte((byte)0);
            outputStream.writeShort((short)0);
            outputStream.writeInt(0);

            outputStream.writeInt(SUBPIXEL_UNKNOWN);
        }
    }

    private boolean formatMatchesDrawable(int format, Drawable drawable) {
        return (format == argb32Format && drawable.visual.depth == 32)
                || (format == a1Format && drawable.visual.depth == 1);
    }

    private void createPicture(XClient client, XInputStream inputStream)
            throws XRequestError {
        int pictureId = inputStream.readInt();
        int drawableId = inputStream.readInt();
        int format = inputStream.readInt();
        int valueMask = inputStream.readInt();

        if (!client.isValidResourceId(pictureId)) throw new BadIdChoice(pictureId);
        Drawable drawable = xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);
        if (format != argb32Format && format != a1Format) throw new BadValue(format);
        if (!formatMatchesDrawable(format, drawable)) throw new BadMatch();

        synchronized (pictures) {
            if (pictures.indexOfKey(pictureId) >= 0) throw new BadIdChoice(pictureId);
            pictures.put(pictureId, new Picture(pictureId, drawable, format));
            ArrayList<Integer> owned = clientPictures.get(client);
            if (owned == null) {
                owned = new ArrayList<>();
                clientPictures.put(client, owned);
                client.addOnDestroyListener(onClientDestroy);
            }
            owned.add(pictureId);
        }

        int valueCount = Integer.bitCount(valueMask);
        if (valueCount > 0) inputStream.skip(valueCount * 4);
    }

    private void freePicture(XClient client, XInputStream inputStream)
            throws XRequestError {
        int pictureId = inputStream.readInt();
        synchronized (pictures) {
            Picture picture = pictures.get(pictureId);
            if (picture == null) throw new BadValue(pictureId);
            pictures.remove(pictureId);
            ArrayList<Integer> owned = clientPictures.get(client);
            if (owned != null) owned.remove(Integer.valueOf(pictureId));
        }
    }

    private void freeClientPictures(XClient client) {
        synchronized (pictures) {
            ArrayList<Integer> owned = clientPictures.remove(client);
            if (owned == null) return;
            for (int id : owned) pictures.remove(id);
        }
    }

    private static int colorToArgb(int red, int green, int blue, int alpha) {
        return ((alpha >>> 8) << 24) | ((red >>> 8) << 16)
                | ((green >>> 8) << 8) | (blue >>> 8);
    }

    private void fillRectangles(XClient client, XInputStream inputStream)
            throws XRequestError {
        int operation = inputStream.readUnsignedByte();
        inputStream.skip(3);
        int pictureId = inputStream.readInt();
        int red = inputStream.readUnsignedShort();
        int green = inputStream.readUnsignedShort();
        int blue = inputStream.readUnsignedShort();
        int alpha = inputStream.readUnsignedShort();

        if (operation != PICT_OP_SRC) throw new BadValue(operation);
        Picture picture;
        synchronized (pictures) {
            picture = pictures.get(pictureId);
        }
        if (picture == null) throw new BadValue(pictureId);

        int color = colorToArgb(red, green, blue, alpha);
        int remaining = client.getRemainingRequestLength();
        while (remaining >= 8) {
            int x = inputStream.readShort();
            int y = inputStream.readShort();
            int width = inputStream.readUnsignedShort();
            int height = inputStream.readUnsignedShort();
            picture.drawable.fillRect(x, y, width, height, color);
            remaining -= 8;
        }
        if (remaining > 0) inputStream.skip(remaining);
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        switch (client.getRequestData()) {
            case ClientOpcodes.QUERY_VERSION:
                queryVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.QUERY_PICT_FORMATS:
                queryPictFormats(client, outputStream);
                break;
            case ClientOpcodes.CREATE_PICTURE:
                createPicture(client, inputStream);
                break;
            case ClientOpcodes.FREE_PICTURE:
                freePicture(client, inputStream);
                break;
            case ClientOpcodes.FILL_RECTANGLES:
                fillRectangles(client, inputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
