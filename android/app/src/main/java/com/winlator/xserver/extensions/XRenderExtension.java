package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import android.util.SparseArray;
import android.util.Log;

import com.winlator.core.Callback;
import com.winlator.renderer.GLRenderer;
import com.winlator.renderer.GPUImage;
import com.winlator.renderer.RenderComposite;
import com.winlator.renderer.Texture;
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
 * Minimal stateful implementation of the Render protocol used by
 * software-rendered desktop clients. Advertise only opcodes this
 * server actually rasterizes: cairo skips CompositeTrapezoids unless
 * the version is at least 0.4.
 */
public class XRenderExtension extends Extension {
    public static final int MAJOR_VERSION = 0;
    public static final int MINOR_VERSION = 4;

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
    private final int rgb24Format = IDGenerator.generate();
    private final int a8Format = IDGenerator.generate();
    private final int a1Format = IDGenerator.generate();
    private final SparseArray<Picture> pictures = new SparseArray<>();
    private final SparseArray<GlyphSet> glyphSets = new SparseArray<>();
    private final IdentityHashMap<XClient, ArrayList<Integer>> clientPictures =
            new IdentityHashMap<>();
    private final Callback<XClient> onClientDestroy = this::freeClientPictures;
    private final IdentityHashMap<XClient, ArrayList<Integer>> clientGlyphSets =
            new IdentityHashMap<>();
    private boolean loggedGeometry;

    private static abstract class ClientOpcodes {
        private static final byte QUERY_VERSION = 0;
        private static final byte QUERY_PICT_FORMATS = 1;
        private static final byte CREATE_PICTURE = 4;
        private static final byte CHANGE_PICTURE = 5;
        private static final byte SET_PICTURE_CLIP_RECTANGLES = 6;
        private static final byte FREE_PICTURE = 7;
        private static final byte COMPOSITE = 8;
        private static final byte COMPOSITE_TRAPEZOIDS = 10;
        private static final byte COMPOSITE_TRIANGLES = 11;
        private static final byte COMPOSITE_TRISTRIP = 12;
        private static final byte COMPOSITE_TRIFAN = 13;
        private static final byte CREATE_GLYPH_SET = 17;
        private static final byte REFERENCE_GLYPH_SET = 18;
        private static final byte FREE_GLYPH_SET = 19;
        private static final byte ADD_GLYPHS = 20;
        private static final byte FREE_GLYPHS = 22;
        private static final byte COMPOSITE_GLYPHS_8 = 23;
        private static final byte COMPOSITE_GLYPHS_16 = 24;
        private static final byte COMPOSITE_GLYPHS_32 = 25;
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
        int payloadBytes = 4 * 28 + 8 + (8 + 8) + 8 + 8 + 4;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(payloadBytes / 4);
            outputStream.writeInt(4); // formats
            outputStream.writeInt(1); // screens
            outputStream.writeInt(3); // depths
            outputStream.writeInt(1); // visuals
            outputStream.writeInt(1); // subpixel orders
            outputStream.writeInt(0);

            writeDirectFormat(outputStream, argb32Format, 32,
                    16, 0xff, 8, 0xff, 0, 0xff, 24, 0xff);
            // cairo-xlib looks up PictStandardRGB24 for opaque surfaces.
            // Depth-24 pixmaps share the 32-bit TrueColor buffer.
            writeDirectFormat(outputStream, rgb24Format, 24,
                    16, 0xff, 8, 0xff, 0, 0xff, 0, 0);
            // Xft uses an alpha-only Render format for glyph sets. It need not
            // correspond to a screen depth or a core-protocol Pixmap format.
            writeDirectFormat(outputStream, a8Format, 8,
                    0, 0, 0, 0, 0, 0, 0, 0xff);
            writeDirectFormat(outputStream, a1Format, 1,
                    0, 0, 0, 0, 0, 0, 0, 1);

            outputStream.writeInt(3); // depths on this screen
            outputStream.writeInt(argb32Format); // fallback

            outputStream.writeByte((byte)32);
            outputStream.writeByte((byte)0);
            outputStream.writeShort((short)1);
            outputStream.writeInt(0);
            outputStream.writeInt(visual.id);
            outputStream.writeInt(argb32Format);

            outputStream.writeByte((byte)24);
            outputStream.writeByte((byte)0);
            outputStream.writeShort((short)0);
            outputStream.writeInt(0);

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
                || (format == rgb24Format && drawable.visual.depth == 32)
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
        if (format != argb32Format && format != rgb24Format
                && format != a8Format && format != a1Format)
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
        if (picture.drawableId != 0) {
            Drawable live = xServer.drawableManager.getDrawable(
                    picture.drawableId);
            if (live != null) return live;
        }
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
        if (picture.format == rgb24Format)
            return 0xff000000 | (pixel & 0x00ffffff);
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
        mask.ensureCpuPixels();
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

    private boolean tryCompositeFast(Picture source, Picture destination,
            Drawable sourceDrawable, Drawable destinationDrawable,
            Picture mask, Drawable maskDrawable, int operation,
            int sourceX, int sourceY, int destinationX, int destinationY,
            int width, int height) {
        if (mask != null || maskDrawable != null) return false;
        if (destinationDrawable == null) return false;
        if (operation != PICT_OP_SRC && operation != PICT_OP_CLEAR)
            return false;
        boolean solid = source != null && source.solidColor != null
                && source.gradient == null && source.radialGradient == null;
        boolean blit = sourceDrawable != null && source != null
                && source.solidColor == null && source.gradient == null
                && source.radialGradient == null
                && source.format != a8Format && source.format != a1Format
                && source.repeat == 0;
        if (operation == PICT_OP_SRC && !solid && !blit) return false;
        for (ClipRectangle rectangle : clippedRectangles(destination,
                destinationX, destinationY, width, height)) {
            if (operation == PICT_OP_CLEAR) {
                destinationDrawable.fillRect(rectangle.x, rectangle.y,
                        rectangle.width, rectangle.height, 0);
                continue;
            }
            if (solid) {
                destinationDrawable.fillRect(rectangle.x, rectangle.y,
                        rectangle.width, rectangle.height, source.solidColor);
                continue;
            }
            int srcX = sourceX + (rectangle.x - destinationX);
            int srcY = sourceY + (rectangle.y - destinationY);
            int copyX = rectangle.x;
            int copyY = rectangle.y;
            int copyW = rectangle.width;
            int copyH = rectangle.height;
            if (srcX < 0) {
                copyX -= srcX;
                copyW += srcX;
                srcX = 0;
            }
            if (srcY < 0) {
                copyY -= srcY;
                copyH += srcY;
                srcY = 0;
            }
            if (srcX + copyW > sourceDrawable.width)
                copyW = sourceDrawable.width - srcX;
            if (srcY + copyH > sourceDrawable.height)
                copyH = sourceDrawable.height - srcY;
            if (copyX != rectangle.x || copyY != rectangle.y
                    || copyW != rectangle.width || copyH != rectangle.height)
                destinationDrawable.fillRect(rectangle.x, rectangle.y,
                        rectangle.width, rectangle.height, 0);
            if (copyW > 0 && copyH > 0) {
                destinationDrawable.copyArea((short)srcX, (short)srcY,
                        (short)copyX, (short)copyY, (short)copyW, (short)copyH,
                        sourceDrawable);
                if (source.format == rgb24Format)
                    destinationDrawable.forceOpaqueRgb(copyX, copyY, copyW,
                            copyH);
            }
        }
        return true;
    }

    private void maybeAttachAhb(Drawable drawable) {
        if (drawable == null) return;
        synchronized (drawable.renderLock) {
            if (!RenderComposite.shouldUseAhb(drawable)) return;
            GPUImage image = new GPUImage(drawable, true, true);
            if (image.getHardwareBufferPtr() == 0) return;
            image.setPreferEglImage(true);
            Texture previous = drawable.replaceTextureKeepCpuBuffer(image);
            GLRenderer renderer = xServer.getRenderer();
            if (previous != null && renderer != null)
                renderer.xServerView.queueEvent(previous::destroy);
        }
    }

    private boolean tryCompositeGpu(Picture source, Picture destination,
            Drawable sourceDrawable, Drawable destinationDrawable,
            Picture mask, Drawable maskDrawable, int operation,
            int sourceX, int sourceY, int maskX, int maskY,
            int destinationX, int destinationY, int width, int height) {
        if (operation != PICT_OP_OVER) return false;
        if (destinationDrawable == null || destinationDrawable.getData() == null)
            return false;
        if (destinationDrawable.visual == null
                || destinationDrawable.visual.depth != 32)
            return false;
        if (source == null) return false;
        if (source.gradient != null || source.radialGradient != null)
            return false;
        if (source.componentAlpha || destination.componentAlpha) return false;
        if (source.solidColor == null && (source.format == a8Format
                || source.format == a1Format))
            return false;
        if (mask != null && maskDrawable == null) return false;
        GLRenderer renderer = xServer.getRenderer();
        if (renderer == null || !renderer.hasEglContext()) return false;
        ArrayList<int[]> clips = new ArrayList<>();
        for (ClipRectangle rectangle : clippedRectangles(destination,
                destinationX, destinationY, width, height))
            clips.add(new int[] {rectangle.x, rectangle.y, rectangle.width,
                    rectangle.height});
        if (clips.isEmpty()) return true;
        maybeAttachAhb(destinationDrawable);
        boolean maskIsA8 = mask != null && (mask.format == a8Format
                || (maskDrawable.visual != null
                    && maskDrawable.visual.depth != 32));
        return renderer.compositeOver(source.solidColor != null
                        ? null : sourceDrawable,
                source.repeat == 1, sourceX, sourceY, maskDrawable, maskX,
                maskY, maskIsA8, destinationDrawable, destinationX,
                destinationY, width, height, source.solidColor, clips);
    }

    private boolean tryFillOverGpu(Picture destination, Drawable drawable,
            int color, ArrayList<int[]> clips) {
        if (drawable == null || drawable.getData() == null) return false;
        if (clips.isEmpty()) return true;
        GLRenderer renderer = xServer.getRenderer();
        if (renderer == null || !renderer.hasEglContext()) return false;
        maybeAttachAhb(drawable);
        return renderer.compositeOver(null, false, 0, 0, null, 0, 0, false,
                drawable, 0, 0, drawable.width, drawable.height, color, clips);
    }

    private boolean tryGlyphsGpu(Picture source, Picture destination,
            Drawable destinationDrawable,
            ArrayList<RenderComposite.GlyphQuad> quads) {
        if (destinationDrawable == null || destinationDrawable.getData() == null)
            return false;
        if (destinationDrawable.visual == null
                || destinationDrawable.visual.depth != 32)
            return false;
        if (source == null || source.gradient != null
                || source.radialGradient != null)
            return false;
        if (quads.isEmpty()) return true;
        GLRenderer renderer = xServer.getRenderer();
        if (renderer == null || !renderer.hasEglContext()) return false;
        maybeAttachAhb(destinationDrawable);
        return renderer.compositeGlyphs(destinationDrawable,
                sourceColor(source), quads);
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
        if (tryCompositeFast(source, destination, sourceDrawable,
                destinationDrawable, mask, maskDrawable, operation,
                sourceX, sourceY, destinationX, destinationY, width, height))
            return;
        if (tryCompositeGpu(source, destination, sourceDrawable,
                destinationDrawable, mask, maskDrawable, operation,
                sourceX, sourceY, maskX, maskY, destinationX, destinationY,
                width, height))
            return;
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
        ArrayList<RenderComposite.GlyphQuad> quads = new ArrayList<>();
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
                        : glyphIdBytes == 2
                        ? inputStream.readUnsignedShort()
                        : inputStream.readInt();
                remaining -= glyphIdBytes;
                Glyph glyph = set.glyphs.get(id);
                if (glyph != null) {
                    int glyphX = penX - glyph.x;
                    int glyphY = penY - glyph.y;
                    for (ClipRectangle rectangle : clippedRectangles(
                            destination, glyphX, glyphY,
                            glyph.width, glyph.height)) {
                        quads.add(new RenderComposite.GlyphQuad(glyph.alpha,
                                glyph.width, glyph.height, rectangle.x,
                                rectangle.y, rectangle.width, rectangle.height,
                                rectangle.x - glyphX, rectangle.y - glyphY));
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
        if (tryGlyphsGpu(source, destination, destinationDrawable, quads))
            return;
        for (RenderComposite.GlyphQuad quad : quads) {
            byte[] alpha = new byte[quad.width * quad.height];
            for (int row = 0; row < quad.height; row++)
                System.arraycopy(quad.alpha,
                        (quad.maskY + row) * quad.glyphWidth + quad.maskX,
                        alpha, row * quad.width, quad.width);
            destinationDrawable.blendAlphaMask(quad.x, quad.y, quad.width,
                    quad.height, alpha, color);
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
        if (operation == PICT_OP_OVER && picture.format != a8Format
                && drawable.visual != null && drawable.visual.depth == 32) {
            ArrayList<int[]> clips = new ArrayList<>();
            while (remaining >= 8) {
                int x = inputStream.readShort();
                int y = inputStream.readShort();
                int width = inputStream.readUnsignedShort();
                int height = inputStream.readUnsignedShort();
                for (ClipRectangle rectangle : clippedRectangles(picture,
                        x, y, width, height))
                    clips.add(new int[] {rectangle.x, rectangle.y,
                            rectangle.width, rectangle.height});
                remaining -= 8;
            }
            if (remaining > 0) inputStream.skip(remaining);
            if (tryFillOverGpu(picture, drawable, color, clips)) return;
            for (int[] clip : clips)
                drawable.blendSolidRect(clip[0], clip[1], clip[2], clip[3],
                        color);
            return;
        }
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

    private static double fromFixed(int value) {
        return value / 65536.0;
    }

    private static double lineX(int x1, int y1, int x2, int y2, double y) {
        double firstY = fromFixed(y1);
        double secondY = fromFixed(y2);
        double delta = secondY - firstY;
        if (Math.abs(delta) < 1.0e-6) return fromFixed(x1);
        double amount = (y - firstY) / delta;
        return fromFixed(x1) + amount * (fromFixed(x2) - fromFixed(x1));
    }

    private void fillCoverageSpan(Picture source, Picture destination,
                                  Drawable drawable, int operation,
                                  int x, int y, int width) {
        if (width <= 0) return;
        for (ClipRectangle rectangle : clippedRectangles(destination,
                x, y, width, 1)) {
            int color = pictureColor(source, rectangle.x, rectangle.y);
            if (destination.format == a8Format) {
                int[] coverage = new int[rectangle.width];
                java.util.Arrays.fill(coverage, color);
                drawable.blendArgbPixels(rectangle.x, rectangle.y,
                        rectangle.width, rectangle.height, coverage, null,
                        0, 0, operation);
            }
            else if (operation == PICT_OP_OVER)
                drawable.blendSolidRect(rectangle.x, rectangle.y,
                        rectangle.width, rectangle.height, color);
            else if (operation == PICT_OP_ADD) {
                int[] colors = new int[rectangle.width];
                java.util.Arrays.fill(colors, color);
                drawable.blendArgbPixels(rectangle.x, rectangle.y,
                        rectangle.width, rectangle.height, colors, null,
                        0, 0, PICT_OP_ADD);
            }
            else
                drawable.fillRect(rectangle.x, rectangle.y,
                        rectangle.width, rectangle.height, color);
        }
    }

    private void fillTrapezoid(Picture source, Picture destination,
                               Drawable drawable, int operation,
                               int top, int bottom,
                               int leftX1, int leftY1, int leftX2, int leftY2,
                               int rightX1, int rightY1, int rightX2, int rightY2) {
        int y0 = Math.max(0, (int)Math.ceil(fromFixed(top)));
        int y1 = Math.min(drawable.height, (int)Math.floor(fromFixed(bottom)));
        for (int y = y0; y < y1; y++) {
            double mid = y + 0.5;
            double left = lineX(leftX1, leftY1, leftX2, leftY2, mid);
            double right = lineX(rightX1, rightY1, rightX2, rightY2, mid);
            if (right < left) {
                double swap = left;
                left = right;
                right = swap;
            }
            int x0 = (int)Math.ceil(left);
            int x1 = (int)Math.floor(right);
            fillCoverageSpan(source, destination, drawable, operation,
                    x0, y, x1 - x0);
        }
    }

    private void fillTriangle(Picture source, Picture destination,
                              Drawable drawable, int operation,
                              int x1, int y1, int x2, int y2, int x3, int y3) {
        int minY = Math.min(y1, Math.min(y2, y3));
        int maxY = Math.max(y1, Math.max(y2, y3));
        int y0 = Math.max(0, (int)Math.ceil(fromFixed(minY)));
        int y1Scan = Math.min(drawable.height, (int)Math.floor(fromFixed(maxY)));
        int[] xs = new int[3];
        int[] ys = new int[3];
        xs[0] = x1; ys[0] = y1;
        xs[1] = x2; ys[1] = y2;
        xs[2] = x3; ys[2] = y3;
        for (int y = y0; y < y1Scan; y++) {
            double mid = y + 0.5;
            double first = Double.NaN;
            double second = Double.NaN;
            for (int edge = 0; edge < 3; edge++) {
                int next = (edge + 1) % 3;
                double edgeY0 = fromFixed(ys[edge]);
                double edgeY1 = fromFixed(ys[next]);
                if (mid < Math.min(edgeY0, edgeY1)
                        || mid >= Math.max(edgeY0, edgeY1))
                    continue;
                double x = lineX(xs[edge], ys[edge], xs[next], ys[next], mid);
                if (Double.isNaN(first)) first = x;
                else second = x;
            }
            if (Double.isNaN(first) || Double.isNaN(second)) continue;
            if (second < first) {
                double swap = first;
                first = second;
                second = swap;
            }
            fillCoverageSpan(source, destination, drawable, operation,
                    (int)Math.ceil(first), y, (int)Math.floor(second)
                            - (int)Math.ceil(first));
        }
    }

    private boolean supportedGeometryOp(int operation) {
        return operation == PICT_OP_SRC || operation == PICT_OP_OVER
                || operation == PICT_OP_ADD;
    }

    private void compositeTrapezoids(XClient client, XInputStream inputStream)
            throws XRequestError {
        int operation = inputStream.readUnsignedByte();
        inputStream.skip(3);
        Picture source;
        Picture destination;
        synchronized (pictures) {
            source = pictures.get(inputStream.readInt());
            destination = pictures.get(inputStream.readInt());
        }
        inputStream.readInt(); // mask format; opaque coverage is enough
        inputStream.skip(4); // xSrc, ySrc
        Drawable drawable = pictureDrawable(destination);
        if (source == null || destination == null || drawable == null)
            throw new BadValue(0);
        if (!supportedGeometryOp(operation)) throw new BadValue(operation);
        if (!loggedGeometry) {
            loggedGeometry = true;
            Log.i("BionicXRender", "geometry trapezoids op=" + operation);
        }

        int remaining = client.getRemainingRequestLength();
        while (remaining >= 40) {
            int top = inputStream.readInt();
            int bottom = inputStream.readInt();
            int leftX1 = inputStream.readInt();
            int leftY1 = inputStream.readInt();
            int leftX2 = inputStream.readInt();
            int leftY2 = inputStream.readInt();
            int rightX1 = inputStream.readInt();
            int rightY1 = inputStream.readInt();
            int rightX2 = inputStream.readInt();
            int rightY2 = inputStream.readInt();
            remaining -= 40;
            fillTrapezoid(source, destination, drawable, operation,
                    top, bottom, leftX1, leftY1, leftX2, leftY2,
                    rightX1, rightY1, rightX2, rightY2);
        }
        if (remaining > 0) inputStream.skip(remaining);
    }

    private void compositeTriangles(XClient client, XInputStream inputStream)
            throws XRequestError {
        int operation = inputStream.readUnsignedByte();
        inputStream.skip(3);
        Picture source;
        Picture destination;
        synchronized (pictures) {
            source = pictures.get(inputStream.readInt());
            destination = pictures.get(inputStream.readInt());
        }
        inputStream.readInt();
        inputStream.skip(4);
        Drawable drawable = pictureDrawable(destination);
        if (source == null || destination == null || drawable == null)
            throw new BadValue(0);
        if (!supportedGeometryOp(operation)) throw new BadValue(operation);
        int remaining = client.getRemainingRequestLength();
        while (remaining >= 24) {
            int x1 = inputStream.readInt();
            int y1 = inputStream.readInt();
            int x2 = inputStream.readInt();
            int y2 = inputStream.readInt();
            int x3 = inputStream.readInt();
            int y3 = inputStream.readInt();
            remaining -= 24;
            fillTriangle(source, destination, drawable, operation,
                    x1, y1, x2, y2, x3, y3);
        }
        if (remaining > 0) inputStream.skip(remaining);
    }

    private void compositeTriList(XClient client, XInputStream inputStream,
                                  boolean fan) throws XRequestError {
        int operation = inputStream.readUnsignedByte();
        inputStream.skip(3);
        Picture source;
        Picture destination;
        synchronized (pictures) {
            source = pictures.get(inputStream.readInt());
            destination = pictures.get(inputStream.readInt());
        }
        inputStream.readInt();
        inputStream.skip(4);
        Drawable drawable = pictureDrawable(destination);
        if (source == null || destination == null || drawable == null)
            throw new BadValue(0);
        if (!supportedGeometryOp(operation)) throw new BadValue(operation);
        int remaining = client.getRemainingRequestLength();
        if (remaining < 24) {
            if (remaining > 0) inputStream.skip(remaining);
            return;
        }
        int firstX = inputStream.readInt();
        int firstY = inputStream.readInt();
        int prevX = inputStream.readInt();
        int prevY = inputStream.readInt();
        remaining -= 16;
        boolean havePrev = true;
        while (remaining >= 8) {
            int x = inputStream.readInt();
            int y = inputStream.readInt();
            remaining -= 8;
            if (havePrev)
                fillTriangle(source, destination, drawable, operation,
                        firstX, firstY, prevX, prevY, x, y);
            if (!fan) {
                firstX = prevX;
                firstY = prevY;
            }
            prevX = x;
            prevY = y;
            havePrev = true;
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
            case ClientOpcodes.COMPOSITE_TRAPEZOIDS:
                compositeTrapezoids(client, inputStream);
                break;
            case ClientOpcodes.COMPOSITE_TRIANGLES:
                compositeTriangles(client, inputStream);
                break;
            case ClientOpcodes.COMPOSITE_TRISTRIP:
                compositeTriList(client, inputStream, false);
                break;
            case ClientOpcodes.COMPOSITE_TRIFAN:
                compositeTriList(client, inputStream, true);
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
            case ClientOpcodes.COMPOSITE_GLYPHS_32:
                compositeGlyphs(client, inputStream, 4);
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
                Log.w("BionicXRender", "unsupported opcode="
                        + (client.getRequestData() & 0xff));
                throw new BadImplementation();
        }
    }
}
