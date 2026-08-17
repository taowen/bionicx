package com.winlator.xserver.requests;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.renderer.FullscreenTransformation;
import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.core.Bitmask;
import com.winlator.xserver.Drawable;
import com.winlator.xserver.Property;
import com.winlator.xserver.Visual;
import com.winlator.xserver.Window;
import com.winlator.xserver.WindowAttributes;
import com.winlator.xserver.WindowManager;
import com.winlator.xserver.GrabManager;
import com.winlator.xserver.XClient;
import com.winlator.xserver.errors.BadAccess;
import com.winlator.xserver.errors.BadDrawable;
import com.winlator.xserver.errors.BadIdChoice;
import com.winlator.xserver.errors.BadMatch;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.BadWindow;
import com.winlator.xserver.errors.XRequestError;
import com.winlator.xserver.events.CreateNotify;
import com.winlator.xserver.events.Event;
import com.winlator.xserver.events.Expose;
import com.winlator.xserver.events.RawEvent;

import java.io.IOException;
import java.util.List;

public abstract class WindowRequests {
    public static void createWindow(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        byte depth = client.getRequestData();
        int windowId = inputStream.readInt();
        int parentId = inputStream.readInt();

        if (!client.isValidResourceId(windowId)) throw new BadIdChoice(windowId);

        Window parent = client.xServer.windowManager.getWindow(parentId);
        if (parent == null) throw new BadWindow(parentId);

        short x = inputStream.readShort();
        short y = inputStream.readShort();
        short width = inputStream.readShort();
        short height = inputStream.readShort();
        short borderWidth = inputStream.readShort();
        WindowAttributes.WindowClass windowClass = WindowAttributes.WindowClass.values()[(byte)inputStream.readShort()];
        Visual visual = client.xServer.pixmapManager.getVisual(inputStream.readInt());
        Bitmask valueMask = new Bitmask(inputStream.readInt());

        Window window = client.xServer.windowManager.createWindow(windowId, parent, x, y, width, height, windowClass, visual, depth, client);
        window.setBorderWidth(borderWidth);
        if (!valueMask.isEmpty()) window.attributes.update(valueMask, inputStream, client);
        client.setEventListenerForWindow(window, window.attributes.getEventMask());
        client.registerAsOwnerOfResource(window);
        parent.sendEvent(Event.SUBSTRUCTURE_NOTIFY, new CreateNotify(parent, window));
    }

    public static void getWindowAttributes(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)window.attributes.getBackingStore().ordinal());
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(3);
            outputStream.writeInt(window.isInputOutput() ? window.getContent().visual.id : 0);
            outputStream.writeShort((short)window.attributes.getWindowClass().ordinal());
            outputStream.writeByte((byte)window.attributes.getBitGravity().ordinal());
            outputStream.writeByte((byte)window.attributes.getWinGravity().ordinal());
            outputStream.writeInt(window.attributes.getBackingPlanes());
            outputStream.writeInt(window.attributes.getBackingPixel());
            outputStream.writeByte((byte)(window.attributes.isSaveUnder() ? 1 : 0));
            outputStream.writeByte((byte)1);
            outputStream.writeByte((byte)window.getMapState().ordinal());
            outputStream.writeByte((byte)(window.attributes.isOverrideRedirect() ? 1 : 0));
            outputStream.writeInt(0);
            outputStream.writeInt(window.getAllEventMasks().getBits());
            outputStream.writeInt(client.getEventMaskForWindow(window).getBits());
            outputStream.writeShort((short)window.attributes.getDoNotPropagateMask().getBits());
            outputStream.writeShort((short)0);
        }
    }

    public static void changeWindowAttributes(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int windowId = inputStream.readInt();
        Bitmask valueMask = new Bitmask(inputStream.readInt());
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        if (!valueMask.isEmpty()) {
            window.attributes.update(valueMask, inputStream, client);

            if (valueMask.isSet(WindowAttributes.FLAG_EVENT_MASK)) {
                boolean hadExposure = window.hasEventListenerFor(Event.EXPOSURE);
                if (isClientCanSelectFor(Event.SUBSTRUCTURE_REDIRECT, window, client) &&
                    isClientCanSelectFor(Event.RESIZE_REDIRECT, window, client) &&
                    isClientCanSelectFor(Event.BUTTON_PRESS, window, client)) {
                    client.setEventListenerForWindow(window, window.attributes.getEventMask());
                    // XSelectInput on a viewable window with a newly selected
                    // ExposureMask must generate Expose. GDK maps a GtkTextView
                    // TEXT child first, then selects Exposure; without this
                    // the child pixmap stays unpainted and a compositor shows
                    // a blank editor.
                    if (!hadExposure
                            && window.attributes.getEventMask().isSet(
                                    Event.EXPOSURE)
                            && window.isInputOutput()
                            && window.getMapState() == Window.MapState.VIEWABLE)
                        window.sendEvent(Event.EXPOSURE, new Expose(window));
                }
                else throw new BadAccess();
            }
        }
    }

    private static boolean isClientCanSelectFor(int eventId, Window window, XClient client) {
        return !window.attributes.getEventMask().isSet(eventId) || !(window.hasEventListenerFor(eventId) && !client.isInterestedIn(eventId, window));
    }

    public static void destroyWindow(XClient client, XInputStream inputStream, XOutputStream outputStream) {
        client.xServer.windowManager.destroyWindow(inputStream.readInt());
    }

    public static void destroySubWindows(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);

        for (Window child : window.getChildren()) client.xServer.windowManager.destroyWindow(child.id);
    }

    public static void changeSaveSet(XClient client, XInputStream inputStream,
                                     XOutputStream outputStream)
            throws XRequestError {
        int mode = client.getRequestData() & 0xff;
        if (mode > 1) throw new BadValue(mode);
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        if (!client.changeSaveSet(window, mode == 0)) throw new BadMatch();
    }

    public static void reparentWindow(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int windowId = inputStream.readInt();
        int parentId = inputStream.readInt();
        short x = inputStream.readShort();
        short y = inputStream.readShort();

        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);

        Window parent = client.xServer.windowManager.getWindow(parentId);
        if (parent == null) throw new BadWindow(parentId);

        client.xServer.windowManager.reparentWindow(window, parent, x, y, client);
    }

    public static void mapWindow(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        client.xServer.windowManager.mapWindow(window, client);
    }

    public static void mapSubWindows(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        client.xServer.windowManager.mapSubWindows(window, client);
    }

    public static void unmapWindow(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        client.xServer.windowManager.unmapWindow(window);
    }

    public static void changeProperty(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        Property.Mode mode = Property.Mode.values()[client.getRequestData()];
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);

        int atom = inputStream.readInt();
        int type = inputStream.readInt();
        byte format = inputStream.readByte();
        inputStream.skip(3);
        int length  = inputStream.readInt();
        int totalSize = length * (format >> 3);

        byte[] data = null;
        if (totalSize > 0) {
            data = new byte[totalSize];
            inputStream.read(data);
            inputStream.skip(-totalSize & 3);
        }

        Property property = window.modifyProperty(atom, type, Property.Format.valueOf(format), mode, data);
        if (property == null) throw new BadMatch();

        client.xServer.windowManager.triggerOnModifyWindowProperty(window, property);
    }

    public static void deleteProperty(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        window.removeProperty(inputStream.readInt());
    }

    public static void listProperties(XClient client, XInputStream inputStream,
                                      XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        int[] names = window.getPropertyNames();
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(names.length);
            outputStream.writeShort((short)names.length);
            outputStream.writePad(22);
            for (int name : names) outputStream.writeInt(name);
        }
    }

    public static void getProperty(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        boolean delete = client.getRequestData() == 1;
        short sequenceNumber = client.getSequenceNumber();
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);

        int atom = inputStream.readInt();
        int type = inputStream.readInt();
        int longOffset = inputStream.readInt();
        int longLength = inputStream.readInt();
        Property property = window.getProperty(atom);

        int bytesAfter = 0;
        try (XStreamLock lock = outputStream.lock()) {
            if (property == null) {
                outputStream.writeByte(RESPONSE_CODE_SUCCESS);
                outputStream.writeByte((byte)0);
                outputStream.writeShort(sequenceNumber);
                outputStream.writeInt(0);
                outputStream.writeInt(0);
                outputStream.writeInt(0);
                outputStream.writeInt(0);
                outputStream.writePad(12);
            }
            else if (property.type != type && type != 0) {
                outputStream.writeByte(RESPONSE_CODE_SUCCESS);
                outputStream.writeByte(property.format.value);
                outputStream.writeShort(sequenceNumber);
                outputStream.writeInt(0);
                outputStream.writeInt(property.type);
                outputStream.writeInt(0);
                outputStream.writeInt(0);
                outputStream.writePad(12);
            }
            else {
                byte[] data = property.data.array();
                // long-offset / long-length are CARD32. WMs pass G_MAXLONG
                // (0x7fffffff); signed int multiply overflows and used to
                // throw BadValue, so getAtomList/getCardinalList failed for
                // every existing property (TYPE_DOCK, STRUT_PARTIAL).
                long offsetBytes = Integer.toUnsignedLong(longOffset) * 4L;
                if (offsetBytes > data.length) throw new BadValue(longOffset);
                int offset = (int)offsetBytes;
                long wantBytes = Integer.toUnsignedLong(longLength) * 4L;
                int remaining = data.length - offset;
                int length = wantBytes >= remaining ? remaining : (int)wantBytes;

                outputStream.writeByte(RESPONSE_CODE_SUCCESS);
                outputStream.writeByte(property.format.value);
                outputStream.writeShort(sequenceNumber);
                outputStream.writeInt((length + 3) / 4);
                outputStream.writeInt(property.type);
                outputStream.writeInt(bytesAfter);
                outputStream.writeInt(length / (property.format.value / 8));
                outputStream.writePad(12);
                outputStream.write(data, offset, length);
                if ((-length & 3) > 0) outputStream.writePad(-length & 3);
            }
        }

        if (delete && property != null && bytesAfter == 0) {
            window.removeProperty(atom);
        }
    }

    public static void queryPointer(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        short rootX = client.xServer.pointer.getClampedX();
        short rootY = client.xServer.pointer.getClampedY();
        Window child = window.getChildByCoords(rootX, rootY, true);
        short[] localPoint = window.rootPointToLocal(rootX, rootY);

        if (child != null) {
            FullscreenTransformation fullscreenTransformation = child.getFullscreenTransformation();
            if (fullscreenTransformation != null) {
                short[] transformedPoint = fullscreenTransformation.transformPointerCoords(rootX, rootY);
                rootX = transformedPoint[0];
                rootY = transformedPoint[1];
                localPoint = child.rootPointToLocal(rootX, rootY);
            }
        }

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)(!client.xServer.isRelativeMouseMovement() ? 1 : 0));
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(client.xServer.windowManager.rootWindow.id);
            outputStream.writeInt(child != null ? child.id : 0);
            outputStream.writeShort(rootX);
            outputStream.writeShort(rootY);
            outputStream.writeShort(localPoint[0]);
            outputStream.writeShort(localPoint[1]);
            outputStream.writeShort((short)client.xServer.inputDeviceManager.getKeyButMask().getBits());
            outputStream.writePad(6);
        }
    }

    public static void translateCoordinates(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        int srcWindowId = inputStream.readInt();
        int dstWindowId = inputStream.readInt();
        short srcX = inputStream.readShort();
        short srcY = inputStream.readShort();

        Window srcWindow = client.xServer.windowManager.getWindow(srcWindowId);
        Window dstWindow = client.xServer.windowManager.getWindow(dstWindowId);

        if (srcWindow == null) throw new BadWindow(srcWindowId);
        if (dstWindow == null) throw new BadWindow(dstWindowId);

        short[] rootPoint = srcWindow.localPointToRoot(srcX, srcY);
        short[] localPoint = dstWindow.rootPointToLocal(rootPoint[0], rootPoint[1]);
        Window child = dstWindow.getChildByCoords(rootPoint[0], rootPoint[1]);

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)1);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(child != null ? child.id : 0);
            outputStream.writeShort(localPoint[0]);
            outputStream.writeShort(localPoint[1]);
            outputStream.writePad(16);
        }
    }

    public static void warpPointer(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        if (client.xServer.isRelativeMouseMovement()) {
            client.skipRequest();
            return;
        }

        Window srcWindow = client.xServer.windowManager.getWindow(inputStream.readInt());
        Window dstWindow = client.xServer.windowManager.getWindow(inputStream.readInt());
        short srcX = inputStream.readShort();
        short srcY = inputStream.readShort();
        short srcWidth = inputStream.readShort();
        short srcHeight = inputStream.readShort();
        short dstX = inputStream.readShort();
        short dstY = inputStream.readShort();

        if (srcWindow != null) {
            if (srcWidth == 0) srcWidth = (short)(srcWindow.getWidth() - srcX);
            if (srcHeight == 0) srcHeight = (short)(srcWindow.getHeight() - srcY);

            short[] localPoint = srcWindow.rootPointToLocal(client.xServer.pointer.getX(), client.xServer.pointer.getY());
            boolean isContained = localPoint[0] >= srcX && localPoint[1] >= srcY && localPoint[0] < (srcX + srcWidth) && localPoint[1] < (srcY + srcHeight);
            if (!isContained) return;
        }

        if (dstWindow == null) {
            client.xServer.pointer.setX(client.xServer.pointer.getX() + dstX);
            client.xServer.pointer.setY(client.xServer.pointer.getY() + dstY);
        }
        else {
            short[] localPoint = dstWindow.localPointToRoot(dstX, dstY);
            client.xServer.pointer.setX(localPoint[0]);
            client.xServer.pointer.setY(localPoint[1]);
        }
    }

    public static void setInputFocus(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int revertValue = client.getRequestData() & 0xff;
        if (revertValue > WindowManager.FocusRevertTo.PARENT.ordinal())
            throw new BadValue(revertValue);
        WindowManager.FocusRevertTo focusRevertTo =
                WindowManager.FocusRevertTo.values()[revertValue];
        int windowId = inputStream.readInt();
        inputStream.skip(4);

        // The request's data byte is the policy used only if the selected
        // focus window later becomes unviewable.  It does not select the
        // focus window itself; None and PointerRoot are special window IDs.
        Window focusedWindow;
        if (windowId == 0) focusedWindow = null;
        else if (windowId == 1) {
            client.xServer.windowManager.setPointerRootFocus(focusRevertTo);
            return;
        }
        else {
            focusedWindow = client.xServer.windowManager.getWindow(windowId);
            if (focusedWindow == null) throw new BadWindow(windowId);
            if (!focusedWindow.attributes.isViewable()) throw new BadMatch();
        }
        client.xServer.windowManager.setFocus(focusedWindow, focusRevertTo);
    }

    public static void getInputFocus(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        Window focusedWindow = client.xServer.windowManager.getFocusedWindow();

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)client.xServer.windowManager.getFocusRevertTo().ordinal());
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(focusedWindow == null ? 0
                    : client.xServer.windowManager.isPointerRootFocus()
                    ? 1 : focusedWindow.id);
            outputStream.writePad(20);
            // After the XSync reply so GDK reads this Expose on the next
            // event-loop turn, not during ensure_native's XSync.
            client.xServer.windowManager.flushExposeAfterRoundTrip();
        }
    }

    public static void configureWindow(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        Bitmask valueMask = new Bitmask(inputStream.readShort());
        inputStream.skip(2);
        if (!valueMask.isEmpty()) client.xServer.windowManager.configureWindow(
                window, valueMask, inputStream, client);
    }

    public static void getGeometry(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        int drawableId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(drawableId);
        Drawable drawable = client.xServer.drawableManager.getDrawable(drawableId);
        // Core X11 explicitly permits InputOnly windows in GetGeometry even
        // though they cannot be used by drawing requests and have no Drawable.
        if (window == null && drawable == null) throw new BadDrawable(drawableId);
        short x = window != null ? window.getX() : 0;
        short y = window != null ? window.getY() : 0;
        short borderWidth = window != null ? window.getBorderWidth() : 0;
        byte depth = window != null && !window.isInputOutput()
                ? 0 : drawable.visual.depth;
        short width = window != null ? window.getWidth() : drawable.width;
        short height = window != null ? window.getHeight() : drawable.height;

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(depth);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(client.xServer.windowManager.rootWindow.id);
            outputStream.writeShort(x);
            outputStream.writeShort(y);
            outputStream.writeShort(width);
            outputStream.writeShort(height);
            outputStream.writeShort(borderWidth);
            outputStream.writePad(10);
        }
    }

    public static void queryTree(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        Window parent = window.getParent();
        List<Window> children = window.getChildren();

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(children.size());
            outputStream.writeInt(client.xServer.windowManager.rootWindow.id);
            outputStream.writeInt(parent != null ? parent.id : 0);
            outputStream.writeShort((short)children.size());
            outputStream.writePad(14);

            // X11: children from bottom-most (first) to top-most (last).
            for (int i = 0; i < children.size(); i++) outputStream.writeInt(children.get(i).id);
        }
    }

    public static void sendEvent(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        boolean propagate = client.getRequestData() != 0;
        int windowId = inputStream.readInt();
        Bitmask eventMask = new Bitmask(inputStream.readInt());
        byte[] data = new byte[32];
        inputStream.read(data);
        Event event = new RawEvent(data);
        if (client.xServer.grabManager.isPointerSynchronous()) {
            GrabManager.grabTrace("BXINFO grab-send-event client="
                    + GrabManager.describeClient(client)
                    + " dest=0x" + Integer.toHexString(windowId)
                    + " type=" + (data[0] & 0x7f)
                    + " send=" + ((data[0] & 0x80) != 0)
                    + " mask=" + eventMask.getBits()
                    + " propagate=" + propagate);
        }

        Window destination = resolveSendEventDestination(client, windowId);
        if (destination == null) return;

        if (eventMask.isEmpty()) {
            if (destination.originClient != null) destination.originClient.sendEvent(event);
            return;
        }
        if (propagate) {
            destination = destination.getAncestorWithEventMask(eventMask);
            if (destination == null) return;
        }
        destination.sendEvent(eventMask, event);
    }

    // PointerWindow (0) is the window containing the pointer. InputFocus (1)
    // is None (discard), PointerRoot (the pointer window), or the focus
    // window. If the focus window contains the pointer, including descendants,
    // destination is the pointer window — GTK menus and IM use that path.
    // WarpPointer only updates coordinates, so look up from the current
    // pointer rather than the cached pointWindow.
    private static Window resolveSendEventDestination(XClient client, int windowId) throws XRequestError {
        WindowManager windows = client.xServer.windowManager;
        if (windowId == 0) {
            Window pointed = windows.findPointWindow(
                    client.xServer.pointer.getClampedX(),
                    client.xServer.pointer.getClampedY(),
                    true);
            return pointed != null ? pointed : windows.rootWindow;
        }
        if (windowId == 1) {
            Window focused = windows.getFocusedWindow();
            if (focused == null) return null;
            Window pointed = windows.findPointWindow(
                    client.xServer.pointer.getClampedX(),
                    client.xServer.pointer.getClampedY(),
                    true);
            if (windows.isPointerRootFocus()) {
                return pointed != null ? pointed : windows.rootWindow;
            }
            if (pointed != null && (pointed == focused || focused.isAncestorOf(pointed))) {
                return pointed;
            }
            return focused;
        }
        Window destination = windows.getWindow(windowId);
        if (destination == null) throw new BadWindow(windowId);
        return destination;
    }

    public static void getScreenSaver(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)600);
            outputStream.writeShort((short)600);
            outputStream.writeByte((byte)1);
            outputStream.writeByte((byte)1);
            outputStream.writePad(18);
        }
    }
}
