package com.winlator.xserver.requests;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.core.Bitmask;
import com.winlator.xserver.Window;
import com.winlator.xserver.XClient;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadAccess;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.BadWindow;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;

public abstract class GrabRequests {
    private enum Status {SUCCESS, ALREADY_GRABBED, INVALID_TIME, NOT_VIEWABLE, FROZEN}

    public static void grabPointer(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        if (client.xServer.isRelativeMouseMovement()) {
            client.skipRequest();
            try (XStreamLock lock = outputStream.lock()) {
                outputStream.writeByte(RESPONSE_CODE_SUCCESS);
                outputStream.writeByte((byte)Status.ALREADY_GRABBED.ordinal());
                outputStream.writeShort(client.getSequenceNumber());
                outputStream.writeInt(0);
                outputStream.writePad(24);
            }
            return;
        }

        boolean ownerEvents = client.getRequestData() == 1;
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);

        Bitmask eventMask = new Bitmask(inputStream.readShort());
        inputStream.skip(14);

        Status status;
        if (client.xServer.grabManager.getWindow() != null && client.xServer.grabManager.getClient() != client) {
            status = Status.ALREADY_GRABBED;
        }
        else if (window.getMapState() != Window.MapState.VIEWABLE) {
            status = Status.NOT_VIEWABLE;
        }
        else {
            status = Status.SUCCESS;
            client.xServer.grabManager.activatePointerGrab(window, ownerEvents, eventMask, client);
        }

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)status.ordinal());
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writePad(24);
        }
    }

    public static void ungrabPointer(XClient client, XInputStream inputStream, XOutputStream outputStream) {
        inputStream.skip(4);
        client.xServer.grabManager.deactivatePointerGrab();
    }

    public static void grabKeyboard(XClient client, XInputStream inputStream,
                                    XOutputStream outputStream)
            throws IOException, XRequestError {
        boolean ownerEvents = client.getRequestData() == 1;
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        int timestamp = inputStream.readInt();
        int pointerMode = inputStream.readByte() & 0xff;
        int keyboardMode = inputStream.readByte() & 0xff;
        inputStream.skip(2);
        // Synchronous modes require core AllowEvents freeze/thaw semantics.
        // Reject that valid-but-unimplemented variant instead of returning a
        // false GrabSuccess and delivering events asynchronously.
        if (timestamp != 0 || pointerMode != 1 || keyboardMode != 1) {
            throw new BadImplementation();
        }

        Status status;
        XClient grabbingClient = client.xServer.grabManager.getKeyboardClient();
        if (grabbingClient != null && grabbingClient != client) {
            status = Status.ALREADY_GRABBED;
        }
        else if (window.getMapState() != Window.MapState.VIEWABLE) {
            status = Status.NOT_VIEWABLE;
        }
        else {
            status = Status.SUCCESS;
            client.xServer.grabManager.activateKeyboardGrab(window, ownerEvents,
                    client);
        }

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)status.ordinal());
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writePad(24);
        }
    }

    public static void ungrabKeyboard(XClient client, XInputStream inputStream,
                                      XOutputStream outputStream)
            throws XRequestError {
        int timestamp = inputStream.readInt();
        if (timestamp != 0) throw new BadImplementation();
        if (client.xServer.grabManager.getKeyboardClient() == client) {
            client.xServer.grabManager.deactivateKeyboardGrab();
        }
    }

    public static void grabKey(XClient client, XInputStream inputStream,
                               XOutputStream outputStream)
            throws XRequestError {
        boolean ownerEvents = client.getRequestData() == 1;
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        int modifiers = inputStream.readShort() & 0xffff;
        int keycode = inputStream.readByte() & 0xff;
        int pointerMode = inputStream.readByte() & 0xff;
        int keyboardMode = inputStream.readByte() & 0xff;
        inputStream.skip(3);
        if ((keycode != 0 && keycode < 8) || (modifiers & ~0x80ff) != 0)
            throw new BadValue(keycode != 0 && keycode < 8
                    ? keycode : modifiers);
        if (pointerMode != 1 || keyboardMode != 1)
            throw new BadImplementation();
        if (!client.xServer.grabManager.addPassiveKeyGrab(window, keycode,
                modifiers, ownerEvents, client)) throw new BadAccess();
    }

    public static void ungrabKey(XClient client, XInputStream inputStream,
                                 XOutputStream outputStream)
            throws XRequestError {
        int keycode = client.getRequestData() & 0xff;
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        int modifiers = inputStream.readShort() & 0xffff;
        inputStream.skip(2);
        if ((modifiers & ~0x80ff) != 0) throw new BadValue(modifiers);
        client.xServer.grabManager.removePassiveKeyGrabs(window, keycode,
                modifiers, client);
    }
}
