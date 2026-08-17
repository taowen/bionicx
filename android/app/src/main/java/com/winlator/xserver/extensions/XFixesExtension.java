package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import android.util.SparseArray;

import com.winlator.core.Callback;
import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Cursor;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.Window;
import com.winlator.xserver.GraphicsContext;
import com.winlator.xserver.errors.BadGraphicsContext;
import com.winlator.xserver.Atom;
import com.winlator.xserver.SelectionManager;
import com.winlator.xserver.events.XFixesSelectionNotify;
import com.winlator.xserver.errors.BadCursor;
import com.winlator.xserver.errors.BadAtom;
import com.winlator.xserver.errors.BadIdChoice;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.BadWindow;
import com.winlator.xserver.errors.BadMatch;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;
import java.util.ArrayList;
import java.util.IdentityHashMap;

/** Stateful subset of XFixes 4.0 cursor, save-set and region resources. */
public class XFixesExtension extends Extension {
    public static final int MAJOR_VERSION = 4;
    public static final int MINOR_VERSION = 0;
    private static final String HIDE_COUNT_TAG = "xfixesHideCount";
    private static final byte FIRST_EVENT = 70;
    private static final byte FIRST_ERROR = -110;

    private final SparseArray<Region> regions = new SparseArray<>();
    private final SparseArray<String> cursorNames = new SparseArray<>();
    private final IdentityHashMap<XClient, ArrayList<Integer>> clientRegions =
            new IdentityHashMap<>();
    private final ArrayList<SelectionInput> selectionInputs = new ArrayList<>();
    private final Callback<XClient> onClientDestroy = this::freeClientState;

    private static abstract class ClientOpcodes {
        private static final byte QUERY_VERSION = 0;
        private static final byte CHANGE_SAVE_SET = 1;
        private static final byte SELECT_SELECTION_INPUT = 2;
        private static final byte SELECT_CURSOR_INPUT = 3;
        private static final byte GET_CURSOR_IMAGE = 4;
        private static final byte CREATE_REGION = 5;
        private static final byte CREATE_REGION_FROM_WINDOW = 7;
        private static final byte DESTROY_REGION = 10;
        private static final byte SET_REGION = 11;
        private static final byte COPY_REGION = 12;
        private static final byte UNION_REGION = 13;
        private static final byte INTERSECT_REGION = 14;
        private static final byte SUBTRACT_REGION = 15;
        private static final byte INVERT_REGION = 16;
        private static final byte TRANSLATE_REGION = 17;
        private static final byte REGION_EXTENTS = 18;
        private static final byte FETCH_REGION = 19;
        private static final byte SET_GC_CLIP_REGION = 20;
        private static final byte SET_WINDOW_SHAPE_REGION = 21;
        private static final byte SET_PICTURE_CLIP_REGION = 22;
        private static final byte SET_CURSOR_NAME = 23;
        private static final byte GET_CURSOR_NAME = 24;
        private static final byte GET_CURSOR_IMAGE_AND_NAME = 25;
        private static final byte CHANGE_CURSOR = 26;
        private static final byte CHANGE_CURSOR_BY_NAME = 27;
        private static final byte HIDE_CURSOR = 29;
        private static final byte SHOW_CURSOR = 30;
    }

    private static final class Rectangle {
        private final short x;
        private final short y;
        private final int width;
        private final int height;

        private Rectangle(short x, short y, int width, int height) {
            this.x = x;
            this.y = y;
            this.width = width;
            this.height = height;
        }
    }

    private static final class Region {
        private final ArrayList<Rectangle> rectangles;

        private Region(ArrayList<Rectangle> rectangles) {
            this.rectangles = rectangles;
        }
    }

    private static final class SelectionInput {
        private final XClient client;
        private final Window window;
        private final int selection;
        private int eventMask;

        private SelectionInput(XClient client, Window window, int selection, int eventMask) {
            this.client = client;
            this.window = window;
            this.selection = selection;
            this.eventMask = eventMask;
        }
    }

    public XFixesExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
        xServer.selectionManager.addOnSelectionModificationListener(this::onSelectionModified);
    }

    @Override
    public String getName() {
        return "XFIXES";
    }

    @Override
    public byte getFirstEventId() {
        return FIRST_EVENT;
    }

    @Override
    public byte getFirstErrorId() {
        return FIRST_ERROR;
    }

    private XRequestError badRegion(int id) {
        return new XRequestError(Byte.toUnsignedInt(FIRST_ERROR), id);
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

    private void createRegion(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        if (!client.isValidResourceId(id)) throw new BadIdChoice(id);

        int remaining = client.getRemainingRequestLength();
        ArrayList<Rectangle> rectangles = new ArrayList<>(remaining / 8);
        while (remaining >= 8) {
            rectangles.add(new Rectangle(inputStream.readShort(), inputStream.readShort(),
                    inputStream.readUnsignedShort(), inputStream.readUnsignedShort()));
            remaining -= 8;
        }
        if (remaining > 0) inputStream.skip(remaining);
        putOwnedRegion(client, id, rectangles);
    }

    private void putOwnedRegion(XClient client, int id,
                                ArrayList<Rectangle> rectangles)
            throws XRequestError {
        synchronized (regions) {
            if (regions.indexOfKey(id) >= 0) throw new BadIdChoice(id);
            regions.put(id, new Region(rectangles));
            ArrayList<Integer> owned = clientRegions.get(client);
            if (owned == null) {
                owned = new ArrayList<>();
                clientRegions.put(client, owned);
                client.addOnDestroyListener(onClientDestroy);
            }
            owned.add(id);
        }
    }

    private void createRegionFromWindow(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        int windowId = inputStream.readInt();
        int kind = inputStream.readUnsignedByte();
        inputStream.skip(3);
        if (!client.isValidResourceId(id)) throw new BadIdChoice(id);
        if (kind > 2) throw new BadValue(kind);
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);

        ArrayList<Rectangle> rectangles = new ArrayList<>();
        if (window.isWindowShaped(kind)) {
            for (Window.ShapeRectangle rectangle : window.copyWindowShape(kind)) {
                rectangles.add(new Rectangle((short)rectangle.x, (short)rectangle.y,
                        rectangle.width, rectangle.height));
            }
        } else {
            int[] extents = window.getWindowShapeExtents(kind);
            rectangles.add(new Rectangle((short)extents[0], (short)extents[1],
                    extents[2], extents[3]));
        }
        putOwnedRegion(client, id, rectangles);
    }

    private void setRegion(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        Region dest = requireRegion(id);
        int remaining = client.getRemainingRequestLength();
        ArrayList<Rectangle> rectangles = new ArrayList<>(remaining / 8);
        while (remaining >= 8) {
            rectangles.add(new Rectangle(inputStream.readShort(), inputStream.readShort(),
                    inputStream.readUnsignedShort(), inputStream.readUnsignedShort()));
            remaining -= 8;
        }
        if (remaining > 0) inputStream.skip(remaining);
        dest.rectangles.clear();
        dest.rectangles.addAll(rectangles);
    }

    private void translateRegion(XInputStream inputStream) throws XRequestError {
        int id = inputStream.readInt();
        short dx = inputStream.readShort();
        short dy = inputStream.readShort();
        Region region = requireRegion(id);
        ArrayList<Rectangle> moved = new ArrayList<>(region.rectangles.size());
        for (Rectangle rectangle : region.rectangles) {
            moved.add(new Rectangle((short)(rectangle.x + dx),
                    (short)(rectangle.y + dy), rectangle.width, rectangle.height));
        }
        region.rectangles.clear();
        region.rectangles.addAll(moved);
    }

    private void invertRegion(XInputStream inputStream) throws XRequestError {
        int sourceId = inputStream.readInt();
        short x = inputStream.readShort();
        short y = inputStream.readShort();
        int width = inputStream.readUnsignedShort();
        int height = inputStream.readUnsignedShort();
        int destId = inputStream.readInt();
        Region source = requireRegion(sourceId);
        Region dest = requireRegion(destId);
        ArrayList<Rectangle> box = new ArrayList<>(1);
        box.add(new Rectangle(x, y, width, height));
        ArrayList<Rectangle> inverted = combineRectangles(
                box, source.rectangles, COMBINE_SUBTRACT);
        dest.rectangles.clear();
        dest.rectangles.addAll(inverted);
    }

    private void regionExtents(XInputStream inputStream) throws XRequestError {
        int sourceId = inputStream.readInt();
        int destId = inputStream.readInt();
        Region source = requireRegion(sourceId);
        Region dest = requireRegion(destId);
        dest.rectangles.clear();
        int[] box = extentsOf(source);
        if (box != null) {
            dest.rectangles.add(new Rectangle((short)box[0], (short)box[1],
                    box[2], box[3]));
        }
    }

    private static int[] extentsOf(Region region) {
        if (region.rectangles.isEmpty()) return null;
        Rectangle first = region.rectangles.get(0);
        int minX = first.x;
        int minY = first.y;
        int maxX = first.x + first.width;
        int maxY = first.y + first.height;
        for (int i = 1; i < region.rectangles.size(); i++) {
            Rectangle rectangle = region.rectangles.get(i);
            minX = Math.min(minX, rectangle.x);
            minY = Math.min(minY, rectangle.y);
            maxX = Math.max(maxX, rectangle.x + rectangle.width);
            maxY = Math.max(maxY, rectangle.y + rectangle.height);
        }
        return new int[]{minX, minY, maxX - minX, maxY - minY};
    }

    private Region requireRegion(int id) throws XRequestError {
        synchronized (regions) {
            Region region = regions.get(id);
            if (region == null) throw badRegion(id);
            return region;
        }
    }

    private void copyRegion(XInputStream inputStream) throws XRequestError {
        int sourceId = inputStream.readInt();
        int destId = inputStream.readInt();
        Region source = requireRegion(sourceId);
        Region dest = requireRegion(destId);
        ArrayList<Rectangle> copied = new ArrayList<>(source.rectangles);
        dest.rectangles.clear();
        dest.rectangles.addAll(copied);
    }

    private void combineRegion(XInputStream inputStream, int op)
            throws XRequestError {
        int source1Id = inputStream.readInt();
        int source2Id = inputStream.readInt();
        int destId = inputStream.readInt();
        Region source1 = requireRegion(source1Id);
        Region source2 = requireRegion(source2Id);
        Region dest = requireRegion(destId);
        ArrayList<Rectangle> combined = combineRectangles(
                source1.rectangles, source2.rectangles, op);
        dest.rectangles.clear();
        dest.rectangles.addAll(combined);
    }

    private static final int COMBINE_UNION = 1;
    private static final int COMBINE_INTERSECT = 2;
    private static final int COMBINE_SUBTRACT = 3;

    private static ArrayList<Rectangle> combineRectangles(
            ArrayList<Rectangle> left, ArrayList<Rectangle> right, int op) {
        ArrayList<Rectangle> out = new ArrayList<>();
        if (op == COMBINE_UNION) {
            out.addAll(left);
            out.addAll(right);
            return out;
        }
        if (op == COMBINE_INTERSECT) {
            for (Rectangle a : left) {
                for (Rectangle b : right) {
                    Rectangle hit = intersectRect(a, b);
                    if (hit != null) out.add(hit);
                }
            }
            return out;
        }
        out.addAll(left);
        for (Rectangle hole : right) {
            ArrayList<Rectangle> next = new ArrayList<>();
            for (Rectangle piece : out) next.addAll(subtractRect(piece, hole));
            out = next;
        }
        return out;
    }

    private static Rectangle intersectRect(Rectangle a, Rectangle b) {
        int x1 = Math.max(a.x, b.x);
        int y1 = Math.max(a.y, b.y);
        int x2 = Math.min(a.x + a.width, b.x + b.width);
        int y2 = Math.min(a.y + a.height, b.y + b.height);
        if (x2 <= x1 || y2 <= y1) return null;
        return new Rectangle((short)x1, (short)y1, x2 - x1, y2 - y1);
    }

    private static ArrayList<Rectangle> subtractRect(Rectangle piece,
                                                     Rectangle hole) {
        ArrayList<Rectangle> out = new ArrayList<>(4);
        Rectangle overlap = intersectRect(piece, hole);
        if (overlap == null) {
            out.add(piece);
            return out;
        }
        int px2 = piece.x + piece.width;
        int py2 = piece.y + piece.height;
        int ox2 = overlap.x + overlap.width;
        int oy2 = overlap.y + overlap.height;
        if (piece.y < overlap.y)
            out.add(new Rectangle(piece.x, piece.y, piece.width,
                    overlap.y - piece.y));
        if (oy2 < py2)
            out.add(new Rectangle(piece.x, (short)oy2, piece.width,
                    py2 - oy2));
        if (piece.x < overlap.x)
            out.add(new Rectangle(piece.x, overlap.y,
                    overlap.x - piece.x, overlap.height));
        if (ox2 < px2)
            out.add(new Rectangle((short)ox2, overlap.y,
                    px2 - ox2, overlap.height));
        return out;
    }

    private void destroyRegion(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        synchronized (regions) {
            if (regions.indexOfKey(id) < 0) throw badRegion(id);
            regions.remove(id);
            ArrayList<Integer> owned = clientRegions.get(client);
            if (owned != null) owned.remove(Integer.valueOf(id));
        }
    }

    private void freeClientState(XClient client) {
        synchronized (regions) {
            ArrayList<Integer> owned = clientRegions.remove(client);
            if (owned == null) return;
            for (int id : owned) regions.remove(id);
        }
        synchronized (selectionInputs) {
            selectionInputs.removeIf(input -> input.client == client);
        }
    }

    private void selectSelectionInput(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        int selection = inputStream.readInt();
        int eventMask = inputStream.readInt();
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        if (!Atom.isValid(selection)) throw new BadAtom(selection);
        if ((eventMask & ~0x7) != 0) throw new BadValue(eventMask);

        synchronized (selectionInputs) {
            SelectionInput existing = null;
            for (SelectionInput input : selectionInputs) {
                if (input.client == client && input.window == window
                        && input.selection == selection) {
                    existing = input;
                    break;
                }
            }
            if (eventMask == 0) {
                if (existing != null) selectionInputs.remove(existing);
            }
            else if (existing != null) {
                existing.eventMask = eventMask;
            }
            else {
                selectionInputs.add(new SelectionInput(client, window, selection, eventMask));
                client.addOnDestroyListener(onClientDestroy);
            }
        }
    }

    private void onSelectionModified(int selection, Window owner, int selectionTimestamp,
                                     int reason, XClient ownerClient) {
        ArrayList<SelectionInput> snapshot;
        synchronized (selectionInputs) {
            snapshot = new ArrayList<>(selectionInputs);
        }
        for (SelectionInput input : snapshot) {
            int reasonMask = 1 << reason;
            if (input.selection == selection && (input.eventMask & reasonMask) != 0
                    && !(reason == SelectionManager.REASON_CLIENT_CLOSE
                         && input.client == ownerClient)) {
                input.client.sendEvent(new XFixesSelectionNotify(
                        Byte.toUnsignedInt(FIRST_EVENT), reason, input.window.id,
                        owner != null ? owner.id : 0, selection, selectionTimestamp));
            }
        }
    }

    private void fetchRegion(XClient client, XInputStream inputStream,
                             XOutputStream outputStream)
            throws IOException, XRequestError {
        int id = inputStream.readInt();
        Region region;
        synchronized (regions) {
            region = regions.get(id);
        }
        if (region == null) throw badRegion(id);

        int[] box = extentsOf(region);
        int minX = box != null ? box[0] : 0;
        int minY = box != null ? box[1] : 0;
        int width = box != null ? box[2] : 0;
        int height = box != null ? box[3] : 0;

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(region.rectangles.size() * 2);
            outputStream.writeShort((short)minX);
            outputStream.writeShort((short)minY);
            outputStream.writeShort((short)width);
            outputStream.writeShort((short)height);
            outputStream.writePad(16);
            for (Rectangle rectangle : region.rectangles) {
                outputStream.writeShort(rectangle.x);
                outputStream.writeShort(rectangle.y);
                outputStream.writeShort((short)rectangle.width);
                outputStream.writeShort((short)rectangle.height);
            }
        }
    }

    private void setWindowShapeRegion(XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        int shapeKind = inputStream.readUnsignedByte();
        inputStream.skip(3);
        short xOffset = inputStream.readShort();
        short yOffset = inputStream.readShort();
        int regionId = inputStream.readInt();

        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        if (shapeKind > 2) throw new BadValue(shapeKind);

        // XFixes: region None is an empty region, not "remove the shape".
        // xfwm4 sets overlay Bounding and Input to empty so the overlay
        // tree is a hole and sibling clients keep the pointer.
        ArrayList<Window.ShapeRectangle> copied;
        if (regionId == 0) {
            copied = new ArrayList<>();
        } else {
            Region region;
            synchronized (regions) {
                region = regions.get(regionId);
            }
            if (region == null) throw badRegion(regionId);
            copied = new ArrayList<>(region.rectangles.size());
            for (Rectangle rectangle : region.rectangles) {
                copied.add(new Window.ShapeRectangle(rectangle.x, rectangle.y,
                        rectangle.width, rectangle.height));
            }
        }
        window.setWindowShape(shapeKind, copied, xOffset, yOffset);
        if (shapeKind == Window.SHAPE_INPUT)
            xServer.inputDeviceManager.updatePointWindow();
    }

    private void setGCClipRegion(XInputStream inputStream) throws XRequestError {
        int gcId = inputStream.readInt();
        int regionId = inputStream.readInt();
        short xOrigin = inputStream.readShort();
        short yOrigin = inputStream.readShort();
        GraphicsContext gc = xServer.graphicsContextManager.getGraphicsContext(gcId);
        if (gc == null) throw new BadGraphicsContext(gcId);
        gc.setClipXOrigin(xOrigin);
        gc.setClipYOrigin(yOrigin);
        if (regionId == 0) {
            gc.clearClipMask();
            return;
        }
        Region region = requireRegion(regionId);
        ArrayList<GraphicsContext.ClipRectangle> rectangles =
                new ArrayList<>(region.rectangles.size());
        for (Rectangle rectangle : region.rectangles) {
            rectangles.add(new GraphicsContext.ClipRectangle(rectangle.x,
                    rectangle.y, rectangle.width, rectangle.height));
        }
        gc.setClipRectangles(rectangles);
    }

    private void setPictureClipRegion(XInputStream inputStream)
            throws XRequestError {
        int pictureId = inputStream.readInt();
        int regionId = inputStream.readInt();
        short xOrigin = inputStream.readShort();
        short yOrigin = inputStream.readShort();
        Extension render = xServer.getExtensionByName("RENDER");
        if (!(render instanceof XRenderExtension)) throw new BadImplementation();
        if (regionId == 0) {
            ((XRenderExtension)render).setPictureClip(pictureId, xOrigin, yOrigin,
                    null, null, null, null);
            return;
        }
        Region region = requireRegion(regionId);
        int count = region.rectangles.size();
        int[] xs = new int[count];
        int[] ys = new int[count];
        int[] widths = new int[count];
        int[] heights = new int[count];
        for (int i = 0; i < count; i++) {
            Rectangle rectangle = region.rectangles.get(i);
            xs[i] = rectangle.x;
            ys[i] = rectangle.y;
            widths[i] = rectangle.width;
            heights[i] = rectangle.height;
        }
        ((XRenderExtension)render).setPictureClip(pictureId, xOrigin, yOrigin,
                xs, ys, widths, heights);
    }

    private void setCursorName(XInputStream inputStream) throws XRequestError {
        int cursorId = inputStream.readInt();
        int nameLength = inputStream.readUnsignedShort();
        inputStream.skip(2);
        if (xServer.cursorManager.getCursor(cursorId) == null)
            throw new BadCursor(cursorId);
        String name = inputStream.readString8(nameLength);
        synchronized (cursorNames) {
            cursorNames.put(cursorId, name);
        }
    }

    private void changeSaveSet(XClient client, XInputStream inputStream)
            throws XRequestError {
        int mode = inputStream.readUnsignedByte();
        inputStream.skip(3);
        int windowId = inputStream.readInt();
        if (mode > 1) throw new BadValue(mode);
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        if (!client.changeSaveSet(window, mode == 0)) throw new BadMatch();
    }

    private void selectCursorInput(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        int eventMask = inputStream.readInt();
        if (xServer.windowManager.getWindow(windowId) == null)
            throw new BadWindow(windowId);
        if ((eventMask & ~1) != 0) throw new BadValue(eventMask);
    }

    private String getStoredCursorName(int cursorId) {
        synchronized (cursorNames) {
            return cursorNames.get(cursorId);
        }
    }

    private void writeCursorImageHeader(XClient client, XOutputStream outputStream,
                                        int extraWords) {
        outputStream.writeByte(RESPONSE_CODE_SUCCESS);
        outputStream.writeByte((byte)0);
        outputStream.writeShort(client.getSequenceNumber());
        outputStream.writeInt(extraWords);
        outputStream.writeShort((short)xServer.pointer.getClampedX());
        outputStream.writeShort((short)xServer.pointer.getClampedY());
        outputStream.writeShort((short)1);
        outputStream.writeShort((short)1);
        outputStream.writeShort((short)0);
        outputStream.writeShort((short)0);
        outputStream.writeInt(1);
    }

    private void getCursorImage(XClient client, XOutputStream outputStream)
            throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            writeCursorImageHeader(client, outputStream, 1);
            outputStream.writePad(8);
            outputStream.writeInt(0);
        }
    }

    private void getCursorImageAndName(XClient client, XOutputStream outputStream)
            throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            writeCursorImageHeader(client, outputStream, 1);
            outputStream.writeInt(0);
            outputStream.writeShort((short)0);
            outputStream.writePad(2);
            outputStream.writeInt(0);
        }
    }

    private void getCursorName(XClient client, XInputStream inputStream,
                               XOutputStream outputStream)
            throws IOException, XRequestError {
        int cursorId = inputStream.readInt();
        if (xServer.cursorManager.getCursor(cursorId) == null)
            throw new BadCursor(cursorId);
        String name = getStoredCursorName(cursorId);
        if (name == null) name = "";
        int atom = name.isEmpty() ? 0 : Atom.internAtom(name);
        int nbytes = name.length();
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt((nbytes + 3) / 4);
            outputStream.writeInt(atom);
            outputStream.writeShort((short)nbytes);
            outputStream.writePad(18);
            if (nbytes > 0) outputStream.writeString8(name);
        }
    }

    private void replaceAssignedCursor(Window window, Cursor source, Cursor destination) {
        if (window.attributes.getAssignedCursor() == source)
            window.attributes.setCursor(destination);
        for (Window child : window.getChildren())
            replaceAssignedCursor(child, source, destination);
    }

    private void changeCursor(XInputStream inputStream) throws XRequestError {
        int sourceId = inputStream.readInt();
        int destinationId = inputStream.readInt();
        Cursor source = xServer.cursorManager.getCursor(sourceId);
        if (source == null) throw new BadCursor(sourceId);
        Cursor destination = xServer.cursorManager.getCursor(destinationId);
        if (destination == null) throw new BadCursor(destinationId);
        replaceAssignedCursor(xServer.windowManager.rootWindow, source, destination);
    }

    private int findCursorIdByName(String name) {
        synchronized (cursorNames) {
            for (int i = 0; i < cursorNames.size(); i++) {
                if (name.equals(cursorNames.valueAt(i)))
                    return cursorNames.keyAt(i);
            }
        }
        return 0;
    }

    private void changeCursorByName(XInputStream inputStream) throws XRequestError {
        int destinationId = inputStream.readInt();
        int nameLength = inputStream.readUnsignedShort();
        inputStream.skip(2);
        Cursor destination = xServer.cursorManager.getCursor(destinationId);
        if (destination == null) throw new BadCursor(destinationId);
        String name = inputStream.readString8(nameLength);
        int sourceId = findCursorIdByName(name);
        Cursor source = sourceId == 0 ? null : xServer.cursorManager.getCursor(sourceId);
        if (source != null)
            replaceAssignedCursor(xServer.windowManager.rootWindow, source, destination);
    }

    private void hideCursor(XInputStream inputStream) throws XRequestError {
        int windowId = inputStream.readInt();
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        Integer count = (Integer)window.getTag(HIDE_COUNT_TAG, Integer.valueOf(0));
        window.setTag(HIDE_COUNT_TAG, Integer.valueOf(count + 1));
    }

    private void showCursor(XInputStream inputStream) throws XRequestError {
        int windowId = inputStream.readInt();
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        Integer count = (Integer)window.getTag(HIDE_COUNT_TAG, Integer.valueOf(0));
        if (count <= 0) throw new BadMatch();
        if (count == 1) window.removeTag(HIDE_COUNT_TAG);
        else window.setTag(HIDE_COUNT_TAG, Integer.valueOf(count - 1));
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        switch (client.getRequestData()) {
            case ClientOpcodes.QUERY_VERSION:
                queryVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.CHANGE_SAVE_SET:
                changeSaveSet(client, inputStream);
                break;
            case ClientOpcodes.SELECT_CURSOR_INPUT:
                selectCursorInput(client, inputStream);
                break;
            case ClientOpcodes.GET_CURSOR_IMAGE:
                getCursorImage(client, outputStream);
                break;
            case ClientOpcodes.GET_CURSOR_IMAGE_AND_NAME:
                getCursorImageAndName(client, outputStream);
                break;
            case ClientOpcodes.GET_CURSOR_NAME:
                getCursorName(client, inputStream, outputStream);
                break;
            case ClientOpcodes.CHANGE_CURSOR:
                changeCursor(inputStream);
                break;
            case ClientOpcodes.CHANGE_CURSOR_BY_NAME:
                changeCursorByName(inputStream);
                break;
            case ClientOpcodes.HIDE_CURSOR:
                hideCursor(inputStream);
                break;
            case ClientOpcodes.SHOW_CURSOR:
                showCursor(inputStream);
                break;
            case ClientOpcodes.SELECT_SELECTION_INPUT:
                selectSelectionInput(client, inputStream);
                break;
            case ClientOpcodes.CREATE_REGION:
                createRegion(client, inputStream);
                break;
            case ClientOpcodes.CREATE_REGION_FROM_WINDOW:
                createRegionFromWindow(client, inputStream);
                break;
            case ClientOpcodes.SET_REGION:
                setRegion(client, inputStream);
                break;
            case ClientOpcodes.INVERT_REGION:
                invertRegion(inputStream);
                break;
            case ClientOpcodes.TRANSLATE_REGION:
                translateRegion(inputStream);
                break;
            case ClientOpcodes.REGION_EXTENTS:
                regionExtents(inputStream);
                break;
            case ClientOpcodes.DESTROY_REGION:
                destroyRegion(client, inputStream);
                break;
            case ClientOpcodes.COPY_REGION:
                copyRegion(inputStream);
                break;
            case ClientOpcodes.UNION_REGION:
                combineRegion(inputStream, COMBINE_UNION);
                break;
            case ClientOpcodes.INTERSECT_REGION:
                combineRegion(inputStream, COMBINE_INTERSECT);
                break;
            case ClientOpcodes.SUBTRACT_REGION:
                combineRegion(inputStream, COMBINE_SUBTRACT);
                break;
            case ClientOpcodes.FETCH_REGION:
                fetchRegion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.SET_GC_CLIP_REGION:
                setGCClipRegion(inputStream);
                break;
            case ClientOpcodes.SET_WINDOW_SHAPE_REGION:
                setWindowShapeRegion(inputStream);
                break;
            case ClientOpcodes.SET_PICTURE_CLIP_REGION:
                setPictureClipRegion(inputStream);
                break;
            case ClientOpcodes.SET_CURSOR_NAME:
                setCursorName(inputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
