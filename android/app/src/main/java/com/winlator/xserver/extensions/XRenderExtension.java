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
import java.nio.ByteBuffer;
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
    private static final byte PICT_OP_OVER = 3;
    private static final int SUBPIXEL_UNKNOWN = 0;

    private final int argb32Format = IDGenerator.generate();
    private final int a8Format = IDGenerator.generate();
    private final int a1Format = IDGenerator.generate();
    private final SparseArray<Picture> pictures = new SparseArray<>();
    private final SparseArray<GlyphSet> glyphSets = new SparseArray<>();
    private final IdentityHashMap<XClient, ArrayList<Integer>> clientPictures =
            new IdentityHashMap<>();
    private final Callback<XClient> onClientDestroy = this::freeClientPictures;
    private final IdentityHashMap<XClient, ArrayList<Integer>> clientGlyphSets =
            new IdentityHashMap<>();

    private static abstract class ClientOpcodes {
        private static final byte QUERY_VERSION = 0;
        private static final byte QUERY_PICT_FORMATS = 1;
        private static final byte CREATE_PICTURE = 4;
        private static final byte FREE_PICTURE = 7;
        private static final byte CREATE_GLYPH_SET = 17;
        private static final byte REFERENCE_GLYPH_SET = 18;
        private static final byte FREE_GLYPH_SET = 19;
        private static final byte ADD_GLYPHS = 20;
        private static final byte FREE_GLYPHS = 22;
        private static final byte COMPOSITE_GLYPHS_8 = 23;
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

    private static final class Glyph {
        private final int width, height, x, y, xOff, yOff;
        private final byte[] alpha;

        private Glyph(int width, int height, int x, int y, int xOff, int yOff,
                      byte[] alpha) {
            this.width = width;
            this.height = height;
            this.x = x;
            this.y = y;
            this.xOff = xOff;
            this.yOff = yOff;
            this.alpha = alpha;
        }
    }

    private static final class GlyphSet {
        private final int id;
        private final int format;
        private final SparseArray<Glyph> glyphs = new SparseArray<>();

        private GlyphSet(int id, int format) {
            this.id = id;
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
        int payloadBytes = 3 * 28 + 8 + (8 + 8) + 8 + 4;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(payloadBytes / 4);
            outputStream.writeInt(3); // formats
            outputStream.writeInt(1); // screens
            outputStream.writeInt(2); // depths
            outputStream.writeInt(1); // visuals
            outputStream.writeInt(1); // subpixel orders
            outputStream.writeInt(0);

            writeDirectFormat(outputStream, argb32Format, 32,
                    16, 0xff, 8, 0xff, 0, 0xff, 24, 0xff);
            // Xft uses an alpha-only Render format for glyph sets. It need not
            // correspond to a screen depth or a core-protocol Pixmap format.
            writeDirectFormat(outputStream, a8Format, 8,
                    0, 0, 0, 0, 0, 0, 0, 0xff);
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
        synchronized (glyphSets) {
            ArrayList<Integer> owned = clientGlyphSets.remove(client);
            if (owned != null) for (int id : owned) glyphSets.remove(id);
        }
    }

    private void createGlyphSet(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        int format = inputStream.readInt();
        if (!client.isValidResourceId(id)) throw new BadIdChoice(id);
        if (format != a8Format && format != a1Format) throw new BadValue(format);
        synchronized (glyphSets) {
            if (glyphSets.indexOfKey(id) >= 0) throw new BadIdChoice(id);
            glyphSets.put(id, new GlyphSet(id, format));
            ArrayList<Integer> owned = clientGlyphSets.get(client);
            if (owned == null) {
                owned = new ArrayList<>();
                clientGlyphSets.put(client, owned);
                client.addOnDestroyListener(onClientDestroy);
            }
            owned.add(id);
        }
    }

    private GlyphSet requireGlyphSet(int id) throws XRequestError {
        synchronized (glyphSets) {
            GlyphSet set = glyphSets.get(id);
            if (set == null) throw new BadValue(id);
            return set;
        }
    }

    private void referenceGlyphSet(XClient client, XInputStream inputStream)
            throws XRequestError {
        int newId = inputStream.readInt();
        GlyphSet source = requireGlyphSet(inputStream.readInt());
        if (!client.isValidResourceId(newId)) throw new BadIdChoice(newId);
        synchronized (glyphSets) {
            if (glyphSets.indexOfKey(newId) >= 0) throw new BadIdChoice(newId);
            glyphSets.put(newId, source);
            ArrayList<Integer> owned = clientGlyphSets.get(client);
            if (owned == null) {
                owned = new ArrayList<>();
                clientGlyphSets.put(client, owned);
                client.addOnDestroyListener(onClientDestroy);
            }
            owned.add(newId);
        }
    }

    private void freeGlyphSet(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        requireGlyphSet(id);
        synchronized (glyphSets) {
            glyphSets.remove(id);
            ArrayList<Integer> owned = clientGlyphSets.get(client);
            if (owned != null) owned.remove(Integer.valueOf(id));
        }
    }

    private void addGlyphs(XClient client, XInputStream inputStream)
            throws XRequestError {
        GlyphSet set = requireGlyphSet(inputStream.readInt());
        int count = inputStream.readInt();
        if (count < 0 || count > 65536
                || (long)count * 16 > client.getRemainingRequestLength())
            throw new BadValue(count);
        int[] ids = new int[count];
        for (int i = 0; i < count; i++) ids[i] = inputStream.readInt();
        int[][] info = new int[count][6];
        for (int i = 0; i < count; i++) {
            info[i][0] = inputStream.readUnsignedShort();
            info[i][1] = inputStream.readUnsignedShort();
            info[i][2] = inputStream.readShort();
            info[i][3] = inputStream.readShort();
            info[i][4] = inputStream.readShort();
            info[i][5] = inputStream.readShort();
        }
        int remaining = client.getRemainingRequestLength();
        for (int i = 0; i < count; i++) {
            int width = info[i][0], height = info[i][1];
            int rowBytes = set.format == a8Format ? width : (width + 7) / 8;
            int paddedRowBytes = (rowBytes + 3) & ~3;
            long encodedLength = (long)paddedRowBytes * height;
            long pixelCount = (long)width * height;
            if (encodedLength > remaining || pixelCount > 16 * 1024 * 1024)
                throw new BadValue(ids[i]);
            ByteBuffer encoded = inputStream.readByteBuffer((int)encodedLength);
            remaining -= (int)encodedLength;
            byte[] alpha = new byte[(int)pixelCount];
            for (int row = 0; row < height; row++) {
                for (int column = 0; column < width; column++) {
                    if (set.format == a8Format)
                        alpha[row * width + column] = encoded.get(row * paddedRowBytes + column);
                    else {
                        int bits = Byte.toUnsignedInt(encoded.get(
                                row * paddedRowBytes + column / 8));
                        alpha[row * width + column] =
                                (byte)((bits & (1 << (column & 7))) != 0 ? 0xff : 0);
                    }
                }
            }
            set.glyphs.put(ids[i], new Glyph(width, height, info[i][2], info[i][3],
                    info[i][4], info[i][5], alpha));
        }
    }

    private void freeGlyphs(XClient client, XInputStream inputStream) throws XRequestError {
        GlyphSet set = requireGlyphSet(inputStream.readInt());
        int remaining = client.getRemainingRequestLength();
        while (remaining >= 4) {
            set.glyphs.remove(inputStream.readInt());
            remaining -= 4;
        }
    }

    private static int sourceColor(Picture source) {
        ByteBuffer data = source.drawable.getData();
        return data != null && data.capacity() >= 4 ? data.getInt(0) : 0xff000000;
    }

    private void compositeGlyphs8(XClient client, XInputStream inputStream)
            throws XRequestError {
        int operation = inputStream.readUnsignedByte();
        inputStream.skip(3);
        Picture source;
        Picture destination;
        synchronized (pictures) {
            source = pictures.get(inputStream.readInt());
            destination = pictures.get(inputStream.readInt());
        }
        int maskFormat = inputStream.readInt();
        GlyphSet set = requireGlyphSet(inputStream.readInt());
        inputStream.skip(4); // xSrc, ySrc
        if (operation != PICT_OP_OVER)
            throw new BadValue(operation);
        if (source == null || destination == null) throw new BadValue(0);
        if (maskFormat != 0 && maskFormat != a8Format && maskFormat != a1Format)
            throw new BadValue(maskFormat);

        int penX = 0, penY = 0;
        int remaining = client.getRemainingRequestLength();
        int color = sourceColor(source);
        while (remaining >= 8) {
            int length = inputStream.readUnsignedByte();
            inputStream.skip(3);
            int deltaX = inputStream.readShort();
            int deltaY = inputStream.readShort();
            remaining -= 8;
            if (length == 0xff) {
                if (remaining < 4) break;
                set = requireGlyphSet(inputStream.readInt());
                remaining -= 4;
                continue;
            }
            penX += deltaX;
            penY += deltaY;
            for (int i = 0; i < length && remaining > 0; i++) {
                int id = inputStream.readUnsignedByte();
                remaining--;
                Glyph glyph = set.glyphs.get(id);
                if (glyph != null) {
                    destination.drawable.blendAlphaMask(
                            penX - glyph.x, penY - glyph.y,
                            glyph.width, glyph.height, glyph.alpha, color);
                    penX += glyph.xOff;
                    penY += glyph.yOff;
                }
            }
            int padding = (-length) & 3;
            if (padding > remaining) break;
            inputStream.skip(padding);
            remaining -= padding;
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
            case ClientOpcodes.CREATE_GLYPH_SET:
                createGlyphSet(client, inputStream);
                break;
            case ClientOpcodes.REFERENCE_GLYPH_SET:
                referenceGlyphSet(client, inputStream);
                break;
            case ClientOpcodes.FREE_GLYPH_SET:
                freeGlyphSet(client, inputStream);
                break;
            case ClientOpcodes.ADD_GLYPHS:
                addGlyphs(client, inputStream);
                break;
            case ClientOpcodes.FREE_GLYPHS:
                freeGlyphs(client, inputStream);
                break;
            case ClientOpcodes.COMPOSITE_GLYPHS_8:
                compositeGlyphs8(client, inputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
