package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import android.util.SparseArray;
import android.util.Log;

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
import com.winlator.xserver.errors.BadPixmap;
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
    private static final byte PICT_OP_CLEAR = 0;
    private static final byte PICT_OP_SRC = 1;
    private static final byte PICT_OP_OVER = 3;
    private static final byte PICT_OP_IN = 5;
    private static final byte PICT_OP_OUT_REVERSE = 8;
    private static final byte PICT_OP_ADD = 12;
    private static final byte PICT_OP_SATURATE = 13;
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
        private static final byte CHANGE_PICTURE = 5;
        private static final byte SET_PICTURE_CLIP_RECTANGLES = 6;
        private static final byte FREE_PICTURE = 7;
        private static final byte COMPOSITE = 8;
        private static final byte CREATE_GLYPH_SET = 17;
        private static final byte REFERENCE_GLYPH_SET = 18;
        private static final byte FREE_GLYPH_SET = 19;
        private static final byte ADD_GLYPHS = 20;
        private static final byte FREE_GLYPHS = 22;
        private static final byte COMPOSITE_GLYPHS_8 = 23;
        private static final byte COMPOSITE_GLYPHS_16 = 24;
        private static final byte FILL_RECTANGLES = 26;
        private static final byte SET_PICTURE_FILTER = 30;
        private static final byte CREATE_SOLID_FILL = 33;
        private static final byte CREATE_LINEAR_GRADIENT = 34;
        private static final byte CREATE_RADIAL_GRADIENT = 35;
    }

    private static final class Picture {
        private final int id;
        private final int drawableId;
        private final Drawable drawable;
        private final int format;
        private final Integer solidColor;
        private final LinearGradient gradient;
        private final RadialGradient radialGradient;
        private int repeat;
        private int clipX;
        private int clipY;
        private boolean componentAlpha;
        private ArrayList<ClipRectangle> clipRectangles;
        private Drawable clipMask;

        private Picture(int id, Drawable drawable, int format) {
            this(id, drawable, format, null, null);
        }

        private Picture(int id, Drawable drawable, int format,
                        Integer solidColor) {
            this(id, drawable, format, solidColor, null, null);
        }

        private Picture(int id, Drawable drawable, int format,
                        Integer solidColor, LinearGradient gradient) {
            this(id, drawable, format, solidColor, gradient, null);
        }

        private Picture(int id, Drawable drawable, int format,
                        Integer solidColor, LinearGradient gradient,
                        RadialGradient radialGradient) {
            this.id = id;
            this.drawable = drawable;
            this.drawableId = drawable != null ? drawable.id : 0;
            this.format = format;
            this.solidColor = solidColor;
            this.gradient = gradient;
            this.radialGradient = radialGradient;
        }
    }

    private static final class LinearGradient {
        private final int x1, y1, x2, y2;
        private final int[] stops;
        private final int[] colors;

        private LinearGradient(int x1, int y1, int x2, int y2,
                               int[] stops, int[] colors) {
            this.x1 = x1;
            this.y1 = y1;
            this.x2 = x2;
            this.y2 = y2;
            this.stops = stops;
            this.colors = colors;
        }
    }

    private static final class RadialGradient {
        private final int innerX, innerY, outerX, outerY;
        private final int innerRadius, outerRadius;
        private final int[] stops;
        private final int[] colors;

        private RadialGradient(int innerX, int innerY, int outerX, int outerY,
                               int innerRadius, int outerRadius,
                               int[] stops, int[] colors) {
            this.innerX = innerX;
            this.innerY = innerY;
            this.outerX = outerX;
            this.outerY = outerY;
            this.innerRadius = innerRadius;
            this.outerRadius = outerRadius;
            this.stops = stops;
            this.colors = colors;
        }
    }

    private static final class ClipRectangle {
        private final int x, y, width, height;

        private ClipRectangle(int x, int y, int width, int height) {
            this.x = x;
            this.y = y;
            this.width = width;
            this.height = height;
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
        if (drawable == null || drawable.visual == null) return false;
        return (format == argb32Format && drawable.visual.depth == 32)
                || (format == a8Format && drawable.visual.depth == 8)
                || (format == a1Format && drawable.visual.depth == 1);
    }

    private void createPicture(XClient client, XInputStream inputStream)
            throws XRequestError {
        int pictureId = inputStream.readInt();
        int drawableId = inputStream.readInt();
        int format = inputStream.readInt();
        int valueMask = inputStream.readInt();

        Drawable drawable = xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);
        if (format != argb32Format && format != a8Format && format != a1Format)
            throw new BadValue(format);
        if (!formatMatchesDrawable(format, drawable)) throw new BadMatch();

        // A Picture created from a Window tracks that window: resize
        // replaces the backing drawable under the same id. cairo-xlib
        // keeps the Picture across ConfigureNotify.
        Picture picture = new Picture(pictureId, drawable, format);
        applyPictureAttributes(picture, valueMask, inputStream);
        registerPicture(client, picture);
    }

    private Drawable pictureDrawable(Picture picture) {
        if (picture == null) return null;
        if (picture.drawableId != 0)
            return xServer.drawableManager.getDrawable(picture.drawableId);
        return picture.drawable;
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

    private Picture requirePicture(int id) throws XRequestError {
        synchronized (pictures) {
            Picture picture = pictures.get(id);
            if (picture == null) throw new BadValue(id);
            return picture;
        }
    }

    private void changePicture(XClient client, XInputStream inputStream)
            throws XRequestError {
        Picture picture = requirePicture(inputStream.readInt());
        int valueMask = inputStream.readInt();
        applyPictureAttributes(picture, valueMask, inputStream);
    }

    private void applyPictureAttributes(Picture picture, int valueMask,
                                        XInputStream inputStream)
            throws XRequestError {
        if ((valueMask & ~0x1fff) != 0) throw new BadValue(valueMask);
        for (int bit = 0; bit < 13; bit++) {
            if ((valueMask & (1 << bit)) == 0) continue;
            int value = inputStream.readInt();
            if (bit == 0) {
                if (value < 0 || value > 3) throw new BadValue(value);
                picture.repeat = value;
            }
            else if (bit == 1 && value != 0) {
                Log.w("BionicXRender", "unsupported alpha-map picture="
                        + Integer.toUnsignedString(value));
                throw new BadImplementation();
            }
            else if (bit == 4) picture.clipX = value;
            else if (bit == 5) picture.clipY = value;
            else if (bit == 6) {
                picture.clipRectangles = null;
                picture.clipMask = null;
                if (value != 0) {
                    com.winlator.xserver.Pixmap pixmap =
                            xServer.pixmapManager.getPixmap(value);
                    if (pixmap == null) throw new BadPixmap(value);
                    if (pixmap.drawable.visual == null
                            || pixmap.drawable.visual.depth != 1)
                        throw new BadMatch();
                    picture.clipMask = pixmap.drawable;
                }
            }
            else if (bit == 12) picture.componentAlpha = value != 0;
        }
    }

    private void setPictureClipRectangles(XClient client,
                                          XInputStream inputStream)
            throws XRequestError {
        Picture picture = requirePicture(inputStream.readInt());
        picture.clipX = inputStream.readShort();
        picture.clipY = inputStream.readShort();
        int remaining = client.getRemainingRequestLength();
        if ((remaining & 7) != 0) throw new BadValue(remaining);
        ArrayList<ClipRectangle> rectangles = new ArrayList<>();
        while (remaining >= 8) {
            rectangles.add(new ClipRectangle(inputStream.readShort(),
                    inputStream.readShort(), inputStream.readUnsignedShort(),
                    inputStream.readUnsignedShort()));
            remaining -= 8;
        }
        picture.clipRectangles = rectangles;
        picture.clipMask = null;
    }

    public void setPictureClip(int pictureId, int xOrigin, int yOrigin,
                               int[] xs, int[] ys, int[] widths, int[] heights)
            throws XRequestError {
        Picture picture = requirePicture(pictureId);
        picture.clipX = xOrigin;
        picture.clipY = yOrigin;
        picture.clipMask = null;
        if (xs == null) {
            picture.clipRectangles = null;
            return;
        }
        ArrayList<ClipRectangle> rectangles = new ArrayList<>(xs.length);
        for (int i = 0; i < xs.length; i++) {
            rectangles.add(new ClipRectangle(xs[i], ys[i], widths[i], heights[i]));
        }
        picture.clipRectangles = rectangles;
    }

    private void registerPicture(XClient client, Picture picture)
            throws XRequestError {
        if (!client.isValidResourceId(picture.id))
            throw new BadIdChoice(picture.id);
        synchronized (pictures) {
            if (pictures.indexOfKey(picture.id) >= 0)
                throw new BadIdChoice(picture.id);
            pictures.put(picture.id, picture);
            ArrayList<Integer> owned = clientPictures.get(client);
            if (owned == null) {
                owned = new ArrayList<>();
                clientPictures.put(client, owned);
                client.addOnDestroyListener(onClientDestroy);
            }
            owned.add(picture.id);
        }
    }

    private void createSolidFill(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        int red = inputStream.readUnsignedShort();
        int green = inputStream.readUnsignedShort();
        int blue = inputStream.readUnsignedShort();
        int alpha = inputStream.readUnsignedShort();
        registerPicture(client, new Picture(id, null, argb32Format,
                colorToArgb(red, green, blue, alpha)));
    }

    private void createLinearGradient(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        int x1 = inputStream.readInt();
        int y1 = inputStream.readInt();
        int x2 = inputStream.readInt();
        int y2 = inputStream.readInt();
        int count = inputStream.readInt();
        if (count < 2 || count > 4096
                || (long)count * 12 > client.getRemainingRequestLength())
            throw new BadValue(count);
        int[] stops = new int[count];
        for (int index = 0; index < count; index++)
            stops[index] = inputStream.readInt();
        int[] colors = new int[count];
        for (int index = 0; index < count; index++) {
            colors[index] = colorToArgb(inputStream.readUnsignedShort(),
                    inputStream.readUnsignedShort(),
                    inputStream.readUnsignedShort(),
                    inputStream.readUnsignedShort());
        }
        registerPicture(client, new Picture(id, null, argb32Format, null,
                new LinearGradient(x1, y1, x2, y2, stops, colors)));
    }

    private void createRadialGradient(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        int innerX = inputStream.readInt();
        int innerY = inputStream.readInt();
        int outerX = inputStream.readInt();
        int outerY = inputStream.readInt();
        int innerRadius = inputStream.readInt();
        int outerRadius = inputStream.readInt();
        int count = inputStream.readInt();
        if (innerRadius < 0 || outerRadius < 0 || count < 2 || count > 4096
                || (long)count * 12 > client.getRemainingRequestLength())
            throw new BadValue(count);
        int[] stops = new int[count];
        for (int index = 0; index < count; index++)
            stops[index] = inputStream.readInt();
        int[] colors = new int[count];
        for (int index = 0; index < count; index++) {
            colors[index] = colorToArgb(inputStream.readUnsignedShort(),
                    inputStream.readUnsignedShort(),
                    inputStream.readUnsignedShort(),
                    inputStream.readUnsignedShort());
        }
        registerPicture(client, new Picture(id, null, argb32Format, null,
                null, new RadialGradient(innerX, innerY, outerX, outerY,
                        innerRadius, outerRadius, stops, colors)));
    }

    private void setPictureFilter(XClient client, XInputStream inputStream)
            throws XRequestError {
        int pictureId = inputStream.readInt();
        int nameLength = inputStream.readUnsignedShort();
        inputStream.skip(2);
        synchronized (pictures) {
            if (pictures.get(pictureId) == null) throw new BadValue(pictureId);
        }
        String filter = inputStream.readString8(nameLength);
        int remaining = client.getRemainingRequestLength();
        if (remaining > 0) inputStream.skip(remaining);
        if (!(filter.equals("nearest") || filter.equals("bilinear")
                || filter.equals("fast") || filter.equals("good")
                || filter.equals("best"))) throw new BadValue(0);
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

    private int sourceColor(Picture source) {
        if (source.solidColor != null) return source.solidColor;
        Drawable drawable = pictureDrawable(source);
        if (drawable == null) return 0xff000000;
        ByteBuffer data = drawable.getData();
        return data != null && data.capacity() >= 4 ? data.getInt(0) : 0xff000000;
    }

    private static int interpolateColor(int first, int second, double amount) {
        int alpha = (int)Math.round(((first >>> 24) & 0xff) * (1.0 - amount)
                + ((second >>> 24) & 0xff) * amount);
        int red = (int)Math.round(((first >>> 16) & 0xff) * (1.0 - amount)
                + ((second >>> 16) & 0xff) * amount);
        int green = (int)Math.round(((first >>> 8) & 0xff) * (1.0 - amount)
                + ((second >>> 8) & 0xff) * amount);
        int blue = (int)Math.round((first & 0xff) * (1.0 - amount)
                + (second & 0xff) * amount);
        return (alpha << 24) | (red << 16) | (green << 8) | blue;
    }

    private static double applyRepeat(double value, int repeat) {
        if (repeat == 1) return value - Math.floor(value);
        if (repeat == 2) return Math.max(0.0, Math.min(1.0, value));
        if (repeat == 3) {
            double period = value - Math.floor(value / 2.0) * 2.0;
            return period <= 1.0 ? period : 2.0 - period;
        }
        return value;
    }

    private static int gradientColor(int[] stops, int[] colors, double value,
                                     int repeat) {
        if (repeat == 0 && (value < 0.0 || value > 1.0)) return 0;
        double fixedValue = applyRepeat(value, repeat) * 65536.0;
        if (fixedValue <= stops[0]) return colors[0];
        int last = stops.length - 1;
        if (fixedValue >= stops[last]) return colors[last];
        for (int index = 1; index <= last; index++) {
            if (fixedValue <= stops[index]) {
                double span = stops[index] - (double)stops[index - 1];
                double amount = span == 0.0 ? 1.0
                        : (fixedValue - stops[index - 1]) / span;
                return interpolateColor(colors[index - 1], colors[index],
                        amount);
            }
        }
        return colors[last];
    }

    private int pictureColor(Picture picture, int x, int y) {
        if (picture.solidColor != null) return picture.solidColor;
        if (picture.gradient != null) {
            LinearGradient gradient = picture.gradient;
            double px = x * 65536.0 + 32768.0;
            double py = y * 65536.0 + 32768.0;
            double dx = gradient.x2 - (double)gradient.x1;
            double dy = gradient.y2 - (double)gradient.y1;
            double lengthSquared = dx * dx + dy * dy;
            double value = lengthSquared == 0.0 ? 0.0
                    : ((px - gradient.x1) * dx + (py - gradient.y1) * dy)
                      / lengthSquared;
            return gradientColor(gradient.stops, gradient.colors, value,
                    picture.repeat);
        }
        if (picture.radialGradient != null) {
            RadialGradient gradient = picture.radialGradient;
            double px = x * 65536.0 + 32768.0 - gradient.innerX;
            double py = y * 65536.0 + 32768.0 - gradient.innerY;
            double dx = gradient.outerX - (double)gradient.innerX;
            double dy = gradient.outerY - (double)gradient.innerY;
            double radiusDelta = gradient.outerRadius
                    - (double)gradient.innerRadius;
            double a = dx * dx + dy * dy - radiusDelta * radiusDelta;
            double b = -2.0 * (px * dx + py * dy
                    + gradient.innerRadius * radiusDelta);
            double c = px * px + py * py
                    - (double)gradient.innerRadius * gradient.innerRadius;
            double value;
            if (Math.abs(a) < 1.0e-12) {
                value = Math.abs(b) < 1.0e-12 ? 0.0 : -c / b;
            }
            else {
                double discriminant = b * b - 4.0 * a * c;
                if (discriminant < 0.0) return 0;
                double root = Math.sqrt(discriminant);
                double first = (-b - root) / (2.0 * a);
                double second = (-b + root) / (2.0 * a);
                boolean firstValid = gradient.innerRadius
                        + first * radiusDelta >= 0.0;
                boolean secondValid = gradient.innerRadius
                        + second * radiusDelta >= 0.0;
                if (firstValid && secondValid)
                    value = Math.max(first, second);
                else if (firstValid) value = first;
                else if (secondValid) value = second;
                else return 0;
            }
            return gradientColor(gradient.stops, gradient.colors, value,
                    picture.repeat);
        }
        Drawable drawable = pictureDrawable(picture);
        if (drawable == null) return 0;
        int width = drawable.width;
        int height = drawable.height;
        if (picture.repeat == 1) {
            x = Math.floorMod(x, width);
            y = Math.floorMod(y, height);
        }
        else if (picture.repeat == 2) {
            x = Math.max(0, Math.min(width - 1, x));
            y = Math.max(0, Math.min(height - 1, y));
        }
        else if (x < 0 || y < 0 || x >= width || y >= height) return 0;
        if (picture.format == a8Format)
            return (drawable.getPixelArgb(x, y) & 0xff) << 24;
        int pixel = drawable.getPixelArgb(x, y);
        // Core drawing stores 24-in-32 with an unused alpha byte of 0.
        // PictOpSrc would otherwise copy those samples as fully transparent
        // and leave the compositor output empty.
        if (picture.format == argb32Format && (pixel >>> 24) == 0
                && (pixel & 0x00ffffff) != 0)
            return 0xff000000 | (pixel & 0x00ffffff);
        return pixel;
    }

    private ArrayList<ClipRectangle> clippedRectangles(Picture picture,
            int x, int y, int width, int height) {
        ArrayList<ClipRectangle> base = new ArrayList<>();
        if (width == 0 || height == 0) return base;
        if (picture.clipRectangles == null) {
            base.add(new ClipRectangle(x, y, width, height));
        }
        else {
            for (ClipRectangle clip : picture.clipRectangles) {
                int clipLeft = picture.clipX + clip.x;
                int clipTop = picture.clipY + clip.y;
                int left = Math.max(x, clipLeft);
                int top = Math.max(y, clipTop);
                int right = Math.min(x + width, clipLeft + clip.width);
                int bottom = Math.min(y + height, clipTop + clip.height);
                if (left < right && top < bottom)
                    base.add(new ClipRectangle(left, top, right - left,
                            bottom - top));
            }
        }
        if (picture.clipMask == null) return base;

        ArrayList<ClipRectangle> masked = new ArrayList<>();
        Drawable mask = picture.clipMask;
        for (ClipRectangle rectangle : base) {
            for (int row = 0; row < rectangle.height; row++) {
                int destinationY = rectangle.y + row;
                int maskY = destinationY - picture.clipY;
                int runStart = -1;
                for (int column = 0; column <= rectangle.width; column++) {
                    int destinationX = rectangle.x + column;
                    int maskX = destinationX - picture.clipX;
                    boolean visible = column < rectangle.width && maskX >= 0
                            && maskY >= 0 && maskX < mask.width
                            && maskY < mask.height
                            && (mask.getPixelArgb(maskX, maskY) & 0xff) != 0;
                    if (visible && runStart < 0) runStart = destinationX;
                    else if (!visible && runStart >= 0) {
                        masked.add(new ClipRectangle(runStart, destinationY,
                                destinationX - runStart, 1));
                        runStart = -1;
                    }
                }
            }
        }
        return masked;
    }

    private void composite(XClient client, XInputStream inputStream)
            throws XRequestError {
        int operation = inputStream.readUnsignedByte();
        inputStream.skip(3);
        Picture source;
        Picture mask;
        Picture destination;
        synchronized (pictures) {
            source = pictures.get(inputStream.readInt());
            int maskId = inputStream.readInt();
            mask = maskId == 0 ? null : pictures.get(maskId);
            destination = pictures.get(inputStream.readInt());
        }
        int sourceX = inputStream.readShort();
        int sourceY = inputStream.readShort();
        int maskX = inputStream.readShort();
        int maskY = inputStream.readShort();
        int destinationX = inputStream.readShort();
        int destinationY = inputStream.readShort();
        int width = inputStream.readUnsignedShort();
        int height = inputStream.readUnsignedShort();

        if (operation != PICT_OP_SRC && operation != PICT_OP_OVER
                && operation != PICT_OP_OUT_REVERSE
                && operation != PICT_OP_ADD
                && operation != PICT_OP_SATURATE)
            throw new BadValue(operation);
        Drawable sourceDrawable = pictureDrawable(source);
        Drawable destinationDrawable = pictureDrawable(destination);
        Drawable maskDrawable = mask != null ? pictureDrawable(mask) : null;
        if (source == null
                || (source.solidColor == null && sourceDrawable == null
                    && source.gradient == null && source.radialGradient == null)
                || destination == null || destinationDrawable == null)
            throw new BadValue(0);
        if (mask != null && maskDrawable == null) throw new BadMatch();
        for (ClipRectangle rectangle : clippedRectangles(destination,
                destinationX, destinationY, width, height)) {
            int offsetX = rectangle.x - destinationX;
            int offsetY = rectangle.y - destinationY;
            int[] colors = new int[rectangle.width * rectangle.height];
            for (int row = 0; row < rectangle.height; row++) {
                for (int column = 0; column < rectangle.width; column++) {
                    colors[row * rectangle.width + column] = pictureColor(
                            source, sourceX + offsetX + column,
                            sourceY + offsetY + row);
                }
            }
            destinationDrawable.blendArgbPixels(rectangle.x, rectangle.y,
                    rectangle.width, rectangle.height, colors,
                    maskDrawable,
                    maskX + offsetX, maskY + offsetY,
                    operation);
        }
    }

    private void compositeGlyphs(XClient client, XInputStream inputStream,
                                 int glyphIdBytes)
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
        Drawable destinationDrawable = pictureDrawable(destination);
        if (source == null || destination == null || destinationDrawable == null)
            throw new BadValue(0);
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
            for (int i = 0; i < length && remaining >= glyphIdBytes; i++) {
                int id = glyphIdBytes == 1
                        ? inputStream.readUnsignedByte()
                        : inputStream.readUnsignedShort();
                remaining -= glyphIdBytes;
                Glyph glyph = set.glyphs.get(id);
                if (glyph != null) {
                    int glyphX = penX - glyph.x;
                    int glyphY = penY - glyph.y;
                    for (ClipRectangle rectangle : clippedRectangles(
                            destination, glyphX, glyphY,
                            glyph.width, glyph.height)) {
                        byte[] alpha = new byte[rectangle.width
                                * rectangle.height];
                        int sourceX = rectangle.x - glyphX;
                        int sourceY = rectangle.y - glyphY;
                        for (int row = 0; row < rectangle.height; row++)
                            System.arraycopy(glyph.alpha,
                                    (sourceY + row) * glyph.width + sourceX,
                                    alpha, row * rectangle.width,
                                    rectangle.width);
                        destinationDrawable.blendAlphaMask(rectangle.x,
                                rectangle.y, rectangle.width,
                                rectangle.height, alpha, color);
                    }
                    penX += glyph.xOff;
                    penY += glyph.yOff;
                }
            }
            int padding = (-(length * glyphIdBytes)) & 3;
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

        if (operation != PICT_OP_CLEAR && operation != PICT_OP_SRC
                && operation != PICT_OP_OVER && operation != PICT_OP_IN)
            throw new BadValue(operation);
        Picture picture;
        synchronized (pictures) {
            picture = pictures.get(pictureId);
        }
        if (picture == null) throw new BadValue(pictureId);
        Drawable drawable = pictureDrawable(picture);
        if (drawable == null) throw new BadValue(pictureId);

        int color = colorToArgb(red, green, blue, alpha);
        int remaining = client.getRemainingRequestLength();
        while (remaining >= 8) {
            int x = inputStream.readShort();
            int y = inputStream.readShort();
            int width = inputStream.readUnsignedShort();
            int height = inputStream.readUnsignedShort();
            for (ClipRectangle rectangle : clippedRectangles(picture,
                    x, y, width, height)) {
                if (operation == PICT_OP_IN) {
                    int[] colors = new int[rectangle.width * rectangle.height];
                    java.util.Arrays.fill(colors, color);
                    drawable.blendArgbPixels(rectangle.x, rectangle.y,
                            rectangle.width, rectangle.height, colors, null,
                            0, 0, PICT_OP_IN);
                }
                else if (picture.format == a8Format) {
                    drawable.fillRect(rectangle.x, rectangle.y,
                            rectangle.width, rectangle.height,
                            operation == PICT_OP_CLEAR ? 0 : (color >>> 24));
                }
                else if (operation == PICT_OP_OVER)
                    drawable.blendSolidRect(rectangle.x, rectangle.y,
                            rectangle.width, rectangle.height, color);
                else
                    drawable.fillRect(rectangle.x, rectangle.y,
                            rectangle.width, rectangle.height,
                            operation == PICT_OP_CLEAR ? 0 : color);
            }
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
            case ClientOpcodes.CHANGE_PICTURE:
                changePicture(client, inputStream);
                break;
            case ClientOpcodes.SET_PICTURE_CLIP_RECTANGLES:
                setPictureClipRectangles(client, inputStream);
                break;
            case ClientOpcodes.FREE_PICTURE:
                freePicture(client, inputStream);
                break;
            case ClientOpcodes.COMPOSITE:
                composite(client, inputStream);
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
                compositeGlyphs(client, inputStream, 1);
                break;
            case ClientOpcodes.COMPOSITE_GLYPHS_16:
                compositeGlyphs(client, inputStream, 2);
                break;
            case ClientOpcodes.SET_PICTURE_FILTER:
                setPictureFilter(client, inputStream);
                break;
            case ClientOpcodes.CREATE_SOLID_FILL:
                createSolidFill(client, inputStream);
                break;
            case ClientOpcodes.CREATE_LINEAR_GRADIENT:
                createLinearGradient(client, inputStream);
                break;
            case ClientOpcodes.CREATE_RADIAL_GRADIENT:
                createRadialGradient(client, inputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
