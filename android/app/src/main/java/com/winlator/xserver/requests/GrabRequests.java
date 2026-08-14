package com.winlator.xserver.requests;

import android.util.Log;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.core.Bitmask;
import com.winlator.xserver.Window;
import com.winlator.xserver.XClient;
import com.winlator.xserver.Cursor;
import com.winlator.xserver.Pointer;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadAccess;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.BadCursor;
import com.winlator.xserver.errors.BadWindow;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;

public abstract class GrabRequests {
    private static final String TAG = "WinlatorXGrab";
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
        int pointerMode = inputStream.readByte() & 0xff;
        int keyboardMode = inputStream.readByte() & 0xff;
        int confineTo = inputStream.readInt();
        int cursorId = inputStream.readInt();
        int timestamp = inputStream.readInt();
        Cursor cursor = cursorId == 0 ? null
                : client.xServer.cursorManager.getCursor(cursorId);
        if (cursorId != 0 && cursor == null) throw new BadCursor(cursorId);
        if (pointerMode != 1 || keyboardMode != 1 || confineTo != 0
                || timestamp != 0) throw new BadImplementation();

        Status status;
        if (client.xServer.grabManager.getWindow() != null && client.xServer.grabManager.getClient() != client) {
            status = Status.ALREADY_GRABBED;
        }
        else if (window.getMapState() != Window.MapState.VIEWABLE) {
            status = Status.NOT_VIEWABLE;
        }
        else {
            status = Status.SUCCESS;
            client.xServer.grabManager.activatePointerGrab(window, ownerEvents,
                    eventMask, client, cursor);
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

    public static void grabButton(XClient client, XInputStream inputStream,
                                  XOutputStream outputStream)
            throws XRequestError {
        boolean ownerEvents = client.getRequestData() == 1;
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        Bitmask eventMask = new Bitmask(inputStream.readShort() & 0xffff);
        int pointerMode = inputStream.readByte() & 0xff;
        int keyboardMode = inputStream.readByte() & 0xff;
        int confineTo = inputStream.readInt();
        int cursorId = inputStream.readInt();
        int button = inputStream.readByte() & 0xff;
        inputStream.skip(1);
        int modifiers = inputStream.readShort() & 0xffff;
        if ((eventMask.getBits() & ~0x7ffc) != 0)
            throw new BadValue(eventMask.getBits());
        if ((modifiers & ~0x80ff) != 0) throw new BadValue(modifiers);
        Cursor cursor = cursorId == 0 ? null
                : client.xServer.cursorManager.getCursor(cursorId);
        if (cursorId != 0 && cursor == null) throw new BadCursor(cursorId);
        if ((pointerMode != 0 && pointerMode != 1)
                || keyboardMode != 1 || confineTo != 0) {
            Log.w(TAG, "unsupported GrabButton modes pointer=" + pointerMode
                    + " keyboard=" + keyboardMode
                    + " confineTo=" + Integer.toUnsignedString(confineTo)
                    + " cursor=" + Integer.toUnsignedString(cursorId));
            throw new BadImplementation();
        }
        if (!client.xServer.grabManager.addPassiveButtonGrab(window, button,
                modifiers, ownerEvents, eventMask, client, cursor,
                pointerMode == 0))
            throw new BadAccess();
    }

    public static void allowEvents(XClient client, XInputStream inputStream,
                                   XOutputStream outputStream)
            throws XRequestError {
        int mode = client.getRequestData() & 0xff;
        int timestamp = inputStream.readInt();
        if (timestamp != 0) throw new BadImplementation();
        if (client.xServer.grabManager.getClient() != client) return;

        if (mode == 2) { // ReplayPointer
            Pointer.Button button =
                    client.xServer.grabManager.getPassiveActivationButton();
            if (!client.xServer.grabManager.isPointerSynchronous()
                    || button == null) return;
            client.xServer.grabManager.deactivatePointerGrabForReplay();
            client.xServer.inputDeviceManager.replayPointerButtonPress(button);
        }
        else throw new BadImplementation();
    }

    public static void ungrabButton(XClient client, XInputStream inputStream,
                                    XOutputStream outputStream)
            throws XRequestError {
        int button = client.getRequestData() & 0xff;
        int windowId = inputStream.readInt();
        Window window = client.xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        int modifiers = inputStream.readShort() & 0xffff;
        inputStream.skip(2);
        if ((modifiers & ~0x80ff) != 0) throw new BadValue(modifiers);
        client.xServer.grabManager.removePassiveButtonGrabs(window, button,
                modifiers, client);
    }

    public static void changeActivePointerGrab(XClient client,
                                                XInputStream inputStream,
                                                XOutputStream outputStream)
            throws XRequestError {
        int cursorId = inputStream.readInt();
        // X11 timestamps are advisory for this request. The server currently
        // has no wrap-aware last-grab clock, so consume the value and apply
        // the change whenever this client owns the active pointer grab.
        inputStream.readInt();
        Bitmask eventMask = new Bitmask(inputStream.readShort() & 0xffff);
        inputStream.skip(2);
        if ((eventMask.getBits() & ~0x7ffc) != 0)
            throw new BadValue(eventMask.getBits());
        Cursor cursor = cursorId == 0 ? null
                : client.xServer.cursorManager.getCursor(cursorId);
        if (cursorId != 0 && cursor == null) throw new BadCursor(cursorId);
        client.xServer.grabManager.changeActivePointerGrab(eventMask, cursor,
                client);
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
        // GTK/xfsettingsd install many passive grabs with GrabModeSync
        // (keyboardMode 0). Freeze/AllowEvents is not implemented; accept
        // the request and deliver keys asynchronously so startup can finish.
        if ((pointerMode != 0 && pointerMode != 1)
                || (keyboardMode != 0 && keyboardMode != 1))
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
