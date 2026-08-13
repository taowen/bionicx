package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.Window;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.BadWindow;
import com.winlator.xserver.errors.XRequestError;
import com.winlator.xserver.events.ShapeNotify;

import java.io.IOException;
import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;

/**
 * SHAPE 1.1. Bounding and clip kinds are stored for QueryExtents/GetRectangles
 * so xterm and IceWM stop seeing a missing extension. The compositor still
 * draws rectangular windows; ShapeInput continues to clip pointer hit tests.
 */
public class ShapeExtension extends Extension {
    public static final int MAJOR_VERSION = 1;
    public static final int MINOR_VERSION = 1;
    private static final byte FIRST_EVENT = 65;

    private static final byte QUERY_VERSION = 0;
    private static final byte RECTANGLES = 1;
    private static final byte MASK = 2;
    private static final byte COMBINE = 3;
    private static final byte OFFSET = 4;
    private static final byte QUERY_EXTENTS = 5;
    private static final byte SELECT_INPUT = 6;
    private static final byte INPUT_SELECTED = 7;
    private static final byte GET_RECTANGLES = 8;

    private static final int OP_SET = 0;
    private static final int OP_UNION = 1;
    private static final int OP_INTERSECT = 2;
    private static final int OP_SUBTRACT = 3;
    private static final int OP_INVERT = 4;

    private final IdentityHashMap<XClient, ArrayList<Window>> selected =
            new IdentityHashMap<>();

    public ShapeExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
    }

    @Override
    public String getName() {
        return "SHAPE";
    }

    @Override
    public byte getFirstEventId() {
        return FIRST_EVENT;
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        switch (client.getRequestData()) {
            case QUERY_VERSION:
                queryVersion(client, outputStream);
                break;
            case RECTANGLES:
                rectangles(client, inputStream);
                break;
            case MASK:
                mask(inputStream);
                break;
            case COMBINE:
                combine(inputStream);
                break;
            case OFFSET:
                offset(inputStream);
                break;
            case QUERY_EXTENTS:
                queryExtents(client, inputStream, outputStream);
                break;
            case SELECT_INPUT:
                selectInput(client, inputStream);
                break;
            case INPUT_SELECTED:
                inputSelected(client, inputStream, outputStream);
                break;
            case GET_RECTANGLES:
                getRectangles(client, inputStream, outputStream);
                break;
            default:
                throw new BadValue(client.getRequestData() & 0xff);
        }
    }

    private void queryVersion(XClient client, XOutputStream outputStream)
            throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)MAJOR_VERSION);
            outputStream.writeShort((short)MINOR_VERSION);
            outputStream.writePad(20);
        }
    }

    private void rectangles(XClient client, XInputStream inputStream)
            throws XRequestError {
        int operation = inputStream.readUnsignedByte();
        int kind = inputStream.readUnsignedByte();
        inputStream.skip(2);
        int windowId = inputStream.readInt();
        short xOffset = inputStream.readShort();
        short yOffset = inputStream.readShort();
        Window window = requireWindow(windowId);
        checkKind(kind);
        checkOperation(operation);

        int remaining = client.getRemainingRequestLength();
        ArrayList<Window.ShapeRectangle> source = new ArrayList<>();
        while (remaining >= 8) {
            short x = inputStream.readShort();
            short y = inputStream.readShort();
            int width = inputStream.readUnsignedShort();
            int height = inputStream.readUnsignedShort();
            source.add(new Window.ShapeRectangle(x + xOffset, y + yOffset,
                    width, height));
            remaining -= 8;
        }
        if (remaining > 0) inputStream.skip(remaining);
        apply(window, kind, operation, source);
    }

    private void mask(XInputStream inputStream) throws XRequestError {
        int operation = inputStream.readUnsignedByte();
        int kind = inputStream.readUnsignedByte();
        inputStream.skip(2);
        int windowId = inputStream.readInt();
        inputStream.skip(4);
        int pixmapId = inputStream.readInt();
        Window window = requireWindow(windowId);
        checkKind(kind);
        checkOperation(operation);
        if (pixmapId == 0) {
            apply(window, kind, OP_SET, null);
            return;
        }
        ArrayList<Window.ShapeRectangle> source = new ArrayList<>();
        source.add(new Window.ShapeRectangle(0, 0, window.getWidth(),
                window.getHeight()));
        apply(window, kind, operation, source);
    }

    private void combine(XInputStream inputStream) throws XRequestError {
        int operation = inputStream.readUnsignedByte();
        int destKind = inputStream.readUnsignedByte();
        int sourceKind = inputStream.readUnsignedByte();
        inputStream.skip(1);
        int destId = inputStream.readInt();
        short xOffset = inputStream.readShort();
        short yOffset = inputStream.readShort();
        int sourceId = inputStream.readInt();
        Window dest = requireWindow(destId);
        Window sourceWindow = requireWindow(sourceId);
        checkKind(destKind);
        checkKind(sourceKind);
        checkOperation(operation);
        ArrayList<Window.ShapeRectangle> source = new ArrayList<>();
        if (!sourceWindow.isWindowShaped(sourceKind)) {
            source.add(new Window.ShapeRectangle(xOffset, yOffset,
                    sourceWindow.getWidth(), sourceWindow.getHeight()));
        }
        else {
            for (Window.ShapeRectangle rectangle
                    : sourceWindow.copyWindowShape(sourceKind)) {
                source.add(new Window.ShapeRectangle(rectangle.x + xOffset,
                        rectangle.y + yOffset, rectangle.width,
                        rectangle.height));
            }
        }
        apply(dest, destKind, operation, source);
    }

    private void offset(XInputStream inputStream) throws XRequestError {
        int kind = inputStream.readUnsignedByte();
        inputStream.skip(3);
        int windowId = inputStream.readInt();
        short xOffset = inputStream.readShort();
        short yOffset = inputStream.readShort();
        Window window = requireWindow(windowId);
        checkKind(kind);
        if (!window.isWindowShaped(kind)) return;
        ArrayList<Window.ShapeRectangle> moved = new ArrayList<>();
        for (Window.ShapeRectangle rectangle : window.copyWindowShape(kind)) {
            moved.add(new Window.ShapeRectangle(rectangle.x + xOffset,
                    rectangle.y + yOffset, rectangle.width, rectangle.height));
        }
        apply(window, kind, OP_SET, moved);
    }

    private void queryExtents(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        Window window = requireWindow(windowId);
        int[] bounding = window.getWindowShapeExtents(Window.SHAPE_BOUNDING);
        int[] clip = window.getWindowShapeExtents(Window.SHAPE_CLIP);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeByte((byte)(window.isWindowShaped(
                    Window.SHAPE_BOUNDING) ? 1 : 0));
            outputStream.writeByte((byte)(window.isWindowShaped(
                    Window.SHAPE_CLIP) ? 1 : 0));
            outputStream.writePad(2);
            outputStream.writeShort((short)bounding[0]);
            outputStream.writeShort((short)bounding[1]);
            outputStream.writeShort((short)bounding[2]);
            outputStream.writeShort((short)bounding[3]);
            outputStream.writeShort((short)clip[0]);
            outputStream.writeShort((short)clip[1]);
            outputStream.writeShort((short)clip[2]);
            outputStream.writeShort((short)clip[3]);
            outputStream.writePad(4);
        }
    }

    private void selectInput(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        boolean enable = inputStream.readByte() != 0;
        inputStream.skip(3);
        Window window = requireWindow(windowId);
        synchronized (selected) {
            ArrayList<Window> windows = selected.get(client);
            if (enable) {
                if (windows == null) {
                    windows = new ArrayList<>();
                    selected.put(client, windows);
                }
                if (!windows.contains(window)) windows.add(window);
            }
            else if (windows != null) {
                windows.remove(window);
                if (windows.isEmpty()) selected.remove(client);
            }
        }
    }

    private void inputSelected(XClient client, XInputStream inputStream,
                               XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        Window window = requireWindow(windowId);
        boolean enabled;
        synchronized (selected) {
            ArrayList<Window> windows = selected.get(client);
            enabled = windows != null && windows.contains(window);
        }
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)(enabled ? 1 : 0));
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writePad(24);
        }
    }

    private void getRectangles(XClient client, XInputStream inputStream,
                               XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        int kind = inputStream.readUnsignedByte();
        inputStream.skip(3);
        Window window = requireWindow(windowId);
        checkKind(kind);
        List<Window.ShapeRectangle> rectangles = window.isWindowShaped(kind)
                ? window.copyWindowShape(kind)
                : java.util.Collections.singletonList(
                        new Window.ShapeRectangle(0, 0, window.getWidth(),
                                window.getHeight()));
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(rectangles.size() * 2);
            outputStream.writeInt(rectangles.size());
            outputStream.writePad(20);
            for (Window.ShapeRectangle rectangle : rectangles) {
                outputStream.writeShort((short)rectangle.x);
                outputStream.writeShort((short)rectangle.y);
                outputStream.writeShort((short)rectangle.width);
                outputStream.writeShort((short)rectangle.height);
            }
        }
    }

    private void apply(Window window, int kind, int operation,
                       List<Window.ShapeRectangle> source) {
        List<Window.ShapeRectangle> current = window.isWindowShaped(kind)
                ? window.copyWindowShape(kind)
                : defaultShape(window);
        List<Window.ShapeRectangle> result;
        switch (operation) {
            case OP_SET:
                result = source;
                break;
            case OP_UNION:
                result = new ArrayList<>(current);
                if (source != null) result.addAll(source);
                break;
            case OP_INTERSECT:
                result = intersect(current, source);
                break;
            case OP_SUBTRACT:
                result = source == null || source.isEmpty() ? current : null;
                break;
            case OP_INVERT:
                result = defaultShape(window);
                break;
            default:
                result = source;
                break;
        }
        window.setWindowShape(kind, result, 0, 0);
        if (kind == Window.SHAPE_INPUT) {
            xServer.inputDeviceManager.updatePointWindow();
        }
        notifyShape(window, kind);
    }

    private void notifyShape(Window window, int kind) {
        int[] extents = window.getWindowShapeExtents(kind);
        ShapeNotify event = new ShapeNotify(FIRST_EVENT, kind, window.id,
                extents[0], extents[1], extents[2], extents[3],
                window.isWindowShaped(kind));
        synchronized (selected) {
            for (java.util.Map.Entry<XClient, ArrayList<Window>> entry
                    : selected.entrySet()) {
                if (entry.getValue().contains(window)) {
                    entry.getKey().sendEvent(event);
                }
            }
        }
    }

    private static ArrayList<Window.ShapeRectangle> defaultShape(Window window) {
        ArrayList<Window.ShapeRectangle> rectangles = new ArrayList<>(1);
        rectangles.add(new Window.ShapeRectangle(0, 0, window.getWidth(),
                window.getHeight()));
        return rectangles;
    }

    private static List<Window.ShapeRectangle> intersect(
            List<Window.ShapeRectangle> dest,
            List<Window.ShapeRectangle> source) {
        if (source == null || source.isEmpty()) return new ArrayList<>();
        ArrayList<Window.ShapeRectangle> result = new ArrayList<>();
        for (Window.ShapeRectangle left : dest) {
            for (Window.ShapeRectangle right : source) {
                int leftEdge = Math.max(left.x, right.x);
                int top = Math.max(left.y, right.y);
                int rightEdge = Math.min(left.x + left.width,
                        right.x + right.width);
                int bottom = Math.min(left.y + left.height,
                        right.y + right.height);
                if (leftEdge < rightEdge && top < bottom) {
                    result.add(new Window.ShapeRectangle(leftEdge, top,
                            rightEdge - leftEdge, bottom - top));
                }
            }
        }
        return result;
    }

    private Window requireWindow(int windowId) throws BadWindow {
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        return window;
    }

    private static void checkKind(int kind) throws BadValue {
        if (kind < Window.SHAPE_BOUNDING || kind > Window.SHAPE_INPUT) {
            throw new BadValue(kind);
        }
    }

    private static void checkOperation(int operation) throws BadValue {
        if (operation < OP_SET || operation > OP_INVERT) {
            throw new BadValue(operation);
        }
    }
}
