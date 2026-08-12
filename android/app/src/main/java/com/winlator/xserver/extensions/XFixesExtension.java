package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import android.util.SparseArray;

import com.winlator.core.Callback;
import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.Window;
import com.winlator.xserver.Atom;
import com.winlator.xserver.SelectionManager;
import com.winlator.xserver.events.XFixesSelectionNotify;
import com.winlator.xserver.errors.BadCursor;
import com.winlator.xserver.errors.BadAtom;
import com.winlator.xserver.errors.BadIdChoice;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.BadWindow;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;
import java.util.ArrayList;
import java.util.IdentityHashMap;

/** Stateful subset of XFixes 2.0 region resources. */
public class XFixesExtension extends Extension {
    public static final int MAJOR_VERSION = 2;
    public static final int MINOR_VERSION = 0;
    private static final byte FIRST_EVENT = 70;
    private static final byte FIRST_ERROR = -110;

    private final SparseArray<Region> regions = new SparseArray<>();
    private final IdentityHashMap<XClient, ArrayList<Integer>> clientRegions =
            new IdentityHashMap<>();
    private final ArrayList<SelectionInput> selectionInputs = new ArrayList<>();
    private final Callback<XClient> onClientDestroy = this::freeClientState;

    private static abstract class ClientOpcodes {
        private static final byte QUERY_VERSION = 0;
        private static final byte SELECT_SELECTION_INPUT = 2;
        private static final byte CREATE_REGION = 5;
        private static final byte DESTROY_REGION = 10;
        private static final byte FETCH_REGION = 19;
        private static final byte SET_WINDOW_SHAPE_REGION = 21;
        private static final byte SET_CURSOR_NAME = 23;
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

        int minX = 0, minY = 0, maxX = 0, maxY = 0;
        if (!region.rectangles.isEmpty()) {
            Rectangle first = region.rectangles.get(0);
            minX = first.x;
            minY = first.y;
            maxX = first.x + first.width;
            maxY = first.y + first.height;
            for (int i = 1; i < region.rectangles.size(); i++) {
                Rectangle rectangle = region.rectangles.get(i);
                minX = Math.min(minX, rectangle.x);
                minY = Math.min(minY, rectangle.y);
                maxX = Math.max(maxX, rectangle.x + rectangle.width);
                maxY = Math.max(maxY, rectangle.y + rectangle.height);
            }
        }

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(region.rectangles.size() * 2);
            outputStream.writeShort((short)minX);
            outputStream.writeShort((short)minY);
            outputStream.writeShort((short)(maxX - minX));
            outputStream.writeShort((short)(maxY - minY));
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
        // ShapeInput is the only kind that affects BionicX's current renderer:
        // it clips pointer hit testing. Do not claim bounding/clip rendering
        // semantics until the GL compositor can apply those regions too.
        if (shapeKind != 2) throw new BadImplementation();

        if (regionId == 0) {
            window.setInputShape(null, 0, 0);
            xServer.inputDeviceManager.updatePointWindow();
            return;
        }

        Region region;
        synchronized (regions) {
            region = regions.get(regionId);
        }
        if (region == null) throw badRegion(regionId);
        ArrayList<Window.ShapeRectangle> copied = new ArrayList<>(region.rectangles.size());
        for (Rectangle rectangle : region.rectangles) {
            copied.add(new Window.ShapeRectangle(rectangle.x, rectangle.y,
                    rectangle.width, rectangle.height));
        }
        window.setInputShape(copied, xOffset, yOffset);
        xServer.inputDeviceManager.updatePointWindow();
    }

    private void setCursorName(XInputStream inputStream) throws XRequestError {
        int cursorId = inputStream.readInt();
        int nameLength = inputStream.readUnsignedShort();
        inputStream.skip(2);
        if (xServer.cursorManager.getCursor(cursorId) == null)
            throw new BadCursor(cursorId);
        inputStream.skip(nameLength);
        inputStream.skip((-nameLength) & 3);
        // Cursor names are descriptive metadata. The Android compositor uses
        // the cursor resource's pixels and does not need to retain the name.
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        switch (client.getRequestData()) {
            case ClientOpcodes.QUERY_VERSION:
                queryVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.SELECT_SELECTION_INPUT:
                selectSelectionInput(client, inputStream);
                break;
            case ClientOpcodes.CREATE_REGION:
                createRegion(client, inputStream);
                break;
            case ClientOpcodes.DESTROY_REGION:
                destroyRegion(client, inputStream);
                break;
            case ClientOpcodes.FETCH_REGION:
                fetchRegion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.SET_WINDOW_SHAPE_REGION:
                setWindowShapeRegion(inputStream);
                break;
            case ClientOpcodes.SET_CURSOR_NAME:
                setCursorName(inputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
