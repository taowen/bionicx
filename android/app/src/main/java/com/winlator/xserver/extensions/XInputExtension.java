package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.core.Bitmask;
import com.winlator.xserver.Cursor;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.Window;
import com.winlator.xserver.events.Event;
import com.winlator.xserver.events.XInputCrossingEvent;
import com.winlator.xserver.events.XInputDeviceEvent;
import com.winlator.xserver.errors.BadCursor;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.BadWindow;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;

import androidx.collection.ArrayMap;

/** Minimal XI 2.0 master-device model backed by BionicX pointer and keyboard. */
public class XInputExtension extends Extension {
    public static final int MAJOR_VERSION = 2;
    public static final int MINOR_VERSION = 0;
    private static final byte FIRST_EVENT = 80;
    private static final byte FIRST_ERROR = -106;

    private static final int ALL_DEVICES = 0;
    private static final int ALL_MASTER_DEVICES = 1;
    private static final int MASTER_POINTER_ID = 2;
    private static final int MASTER_KEYBOARD_ID = 3;
    private static final int USE_MASTER_POINTER = 1;
    private static final int USE_MASTER_KEYBOARD = 2;
    private static final int BUTTON_CLASS = 1;
    private static final int POINTER_BUTTONS = 7;
    public static final int XI_KEY_PRESS = 2;
    public static final int XI_KEY_RELEASE = 3;
    public static final int XI_BUTTON_PRESS = 4;
    public static final int XI_BUTTON_RELEASE = 5;
    public static final int XI_MOTION = 6;
    public static final int XI_ENTER = 7;
    public static final int XI_LEAVE = 8;
    private byte[] pointerGrabMask;
    private byte[] keyboardGrabMask;

    private static abstract class ClientOpcodes {
        private static final byte GET_EXTENSION_VERSION = 1;
        private static final byte XI_QUERY_POINTER = 40;
        private static final byte XI_CHANGE_CURSOR = 42;
        private static final byte XI_SELECT_EVENTS = 46;
        private static final byte XI_QUERY_VERSION = 47;
        private static final byte XI_QUERY_DEVICE = 48;
        private static final byte XI_GRAB_DEVICE = 51;
        private static final byte XI_UNGRAB_DEVICE = 52;
        private static final byte XI_GET_SELECTED_EVENTS = 60;
    }

    private void queryPointer(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        int deviceId = inputStream.readUnsignedShort();
        inputStream.skip(2);
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        if (deviceId != MASTER_POINTER_ID && deviceId != ALL_MASTER_DEVICES)
            throw new BadValue(deviceId);

        short rootX = xServer.pointer.getClampedX();
        short rootY = xServer.pointer.getClampedY();
        short[] local = window.rootPointToLocal(rootX, rootY);
        Window child = window.getChildByCoords(rootX, rootY, true);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.XI_QUERY_POINTER);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(6); // 56-byte fixed reply, no button mask
            outputStream.writeInt(xServer.windowManager.rootWindow.id);
            outputStream.writeInt(child != null ? child.id : 0);
            outputStream.writeInt(rootX << 16);
            outputStream.writeInt(rootY << 16);
            outputStream.writeInt(local[0] << 16);
            outputStream.writeInt(local[1] << 16);
            outputStream.writeByte((byte)1); // same_screen
            outputStream.writeByte((byte)0);
            outputStream.writeShort((short)0); // buttons_len
            outputStream.writeInt(xServer.keyboard.getBaseModifiers());
            outputStream.writeInt(0); // latched modifiers
            outputStream.writeInt(xServer.keyboard.getLockedModifiers());
            outputStream.writeInt(xServer.keyboard.getModifiersMask().getBits());
            outputStream.writeInt(0); // group info
        }
    }

    private void changeCursor(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        int cursorId = inputStream.readInt();
        int deviceId = inputStream.readUnsignedShort();
        inputStream.skip(2);
        if (deviceId != MASTER_POINTER_ID && deviceId != ALL_MASTER_DEVICES)
            throw new BadValue(deviceId);
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        Cursor cursor = cursorId == 0 ? null
                : xServer.cursorManager.getCursor(cursorId);
        if (cursorId != 0 && cursor == null) throw new BadCursor(cursorId);
        window.attributes.setCursor(cursor);
    }

    public XInputExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
    }

    @Override
    public String getName() {
        return "XInputExtension";
    }

    @Override
    public byte getFirstEventId() {
        return FIRST_EVENT;
    }

    @Override
    public byte getFirstErrorId() {
        return FIRST_ERROR;
    }

    public void sendDeviceEvent(int deviceId, int eventType, int detail,
                                Window sourceWindow, short rootX, short rootY) {
        if (sourceWindow == null) return;
        XClient grabbedClient = null;
        boolean deliveredToGrabbedClient = false;
        Window grabWindow = deviceId == MASTER_POINTER_ID
                ? xServer.grabManager.getWindow()
                : xServer.grabManager.getKeyboardWindow();
        byte[] grabMask = deviceId == MASTER_POINTER_ID
                ? pointerGrabMask : keyboardGrabMask;
        boolean ownerEvents = deviceId == MASTER_POINTER_ID
                ? xServer.grabManager.isOwnerEvents()
                : xServer.grabManager.isKeyboardOwnerEvents();
        if (grabWindow != null) {
            grabbedClient = deviceId == MASTER_POINTER_ID
                    ? xServer.grabManager.getClient()
                    : xServer.grabManager.getKeyboardClient();
            Window target = null;
            if (grabbedClient != null && ownerEvents) {
                target = selectedAncestor(grabbedClient, sourceWindow,
                        deviceId, eventType);
            }
            if (target == null && grabbedClient != null && grabMask != null
                    && maskBit(grabMask, eventType)) {
                target = grabWindow;
            }
            if (target != null && target.attributes.isEnabled()) {
                sendDeviceEventToClient(grabbedClient, deviceId, eventType,
                        detail, sourceWindow, target, rootX, rootY);
                deliveredToGrabbedClient = true;
            }
        }
        for (XClient client : xServer.getClientsSnapshot()) {
            if (client == grabbedClient && deliveredToGrabbedClient) continue;
            Window eventWindow = selectedAncestor(client, sourceWindow,
                    deviceId, eventType);
            if (eventWindow == null || !eventWindow.attributes.isEnabled()) continue;
            sendDeviceEventToClient(client, deviceId, eventType, detail,
                    sourceWindow, eventWindow, rootX, rootY);
        }
    }

    private Window selectedAncestor(XClient client, Window sourceWindow,
                                    int deviceId, int eventType) {
        Window eventWindow = sourceWindow;
        while (eventWindow != null
                && !client.isXiEventSelected(eventWindow, deviceId, eventType)) {
            eventWindow = eventWindow.getParent();
        }
        return eventWindow;
    }

    private void sendDeviceEventToClient(XClient client, int deviceId,
            int eventType, int detail, Window sourceWindow, Window eventWindow,
            short rootX, short rootY) {
        short[] local = eventWindow.rootPointToLocal(rootX, rootY);
        Window child = sourceWindow == eventWindow ? null : sourceWindow;
        int buttonState = xiButtonState();
        if (deviceId == MASTER_POINTER_ID && detail > 0
                && detail <= POINTER_BUTTONS) {
            if (eventType == XI_BUTTON_PRESS) buttonState &= ~(1 << detail);
            else if (eventType == XI_BUTTON_RELEASE) buttonState |= 1 << detail;
        }
        client.sendEvent(new XInputDeviceEvent(getMajorOpcode(), deviceId,
                eventType, detail, xServer.windowManager.rootWindow,
                eventWindow, child, rootX, rootY, local[0], local[1],
                xServer.keyboard.getBaseModifiers(),
                xServer.keyboard.getLockedModifiers(),
                xServer.keyboard.getModifiersMask().getBits(), buttonState));
    }

    public void sendCrossingEvent(int eventType, int detail, int mode,
            Window sourceWindow, short rootX, short rootY, boolean focus) {
        if (sourceWindow == null) return;
        XClient grabbedClient = xServer.grabManager.getClient();
        Window grabWindow = xServer.grabManager.getWindow();
        boolean deliveredToGrabbedClient = false;
        if (grabWindow != null && grabbedClient != null) {
            Window target = null;
            if (xServer.grabManager.isOwnerEvents()) {
                target = selectedAncestor(grabbedClient, sourceWindow,
                        MASTER_POINTER_ID, eventType);
            }
            if (target == null && pointerGrabMask != null
                    && maskBit(pointerGrabMask, eventType)) target = grabWindow;
            if (target != null && target.attributes.isEnabled()) {
                sendCrossingEventToClient(grabbedClient, eventType, detail,
                        mode, sourceWindow, target, rootX, rootY, focus);
                deliveredToGrabbedClient = true;
            }
        }
        for (XClient client : xServer.getClientsSnapshot()) {
            if (client == grabbedClient && deliveredToGrabbedClient) continue;
            Window eventWindow = selectedAncestor(client, sourceWindow,
                    MASTER_POINTER_ID, eventType);
            if (eventWindow == null || !eventWindow.attributes.isEnabled()) continue;
            sendCrossingEventToClient(client, eventType, detail, mode,
                    sourceWindow, eventWindow, rootX, rootY, focus);
        }
    }

    private void sendCrossingEventToClient(XClient client, int eventType,
            int detail, int mode, Window sourceWindow, Window eventWindow,
            short rootX, short rootY, boolean focus) {
        short[] local = eventWindow.rootPointToLocal(rootX, rootY);
        int buttonState = xiButtonState();
        client.sendEvent(new XInputCrossingEvent(getMajorOpcode(),
                MASTER_POINTER_ID, eventType, detail, mode,
                xServer.windowManager.rootWindow, eventWindow,
                sourceWindow == eventWindow ? null : sourceWindow,
                rootX, rootY, local[0], local[1], focus,
                xServer.keyboard.getBaseModifiers(),
                xServer.keyboard.getLockedModifiers(),
                xServer.keyboard.getModifiersMask().getBits(), buttonState));
    }

    private int xiButtonState() {
        int buttonState = 0;
        int coreButtons = xServer.pointer.getButtonMask().getBits();
        for (int button = 1; button <= POINTER_BUTTONS; button++) {
            if ((coreButtons & (1 << (button + 7))) != 0)
                buttonState |= 1 << button;
        }
        return buttonState;
    }

    private void getExtensionVersion(XClient client, XInputStream inputStream,
                                     XOutputStream outputStream) throws IOException {
        int nameLength = inputStream.readUnsignedShort();
        inputStream.skip(2);
        inputStream.readString8(nameLength);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.GET_EXTENSION_VERSION);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)MAJOR_VERSION);
            outputStream.writeShort((short)MINOR_VERSION);
            outputStream.writeByte((byte)1);
            outputStream.writePad(19);
        }
    }

    private void queryVersion(XClient client, XInputStream inputStream,
                              XOutputStream outputStream) throws IOException {
        inputStream.skip(4);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.XI_QUERY_VERSION);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)MAJOR_VERSION);
            outputStream.writeShort((short)MINOR_VERSION);
            outputStream.writePad(20);
        }
    }

    private static int paddedLength(String name) {
        return (name.length() + 3) & ~3;
    }

    private void writeDevice(XOutputStream outputStream, int id, int use,
                             int attachment, String name, boolean pointer) {
        outputStream.writeShort((short)id);
        outputStream.writeShort((short)use);
        outputStream.writeShort((short)attachment);
        outputStream.writeShort((short)(pointer ? 1 : 0));
        outputStream.writeShort((short)name.length());
        outputStream.writeByte((byte)1); // enabled
        outputStream.writeByte((byte)0);
        outputStream.write(name.getBytes(xServer.LATIN1_CHARSET));
        int padding = -name.length() & 3;
        if (padding > 0) outputStream.writePad(padding);
        if (pointer) {
            // XIButtonClass: header, one 32-bit current-state word, then one
            // Atom label per button. None is a valid label.
            outputStream.writeShort((short)BUTTON_CLASS);
            outputStream.writeShort((short)10); // 40 bytes / 4
            outputStream.writeShort((short)id);
            outputStream.writeShort((short)POINTER_BUTTONS);
            outputStream.writeInt(0);
            outputStream.writePad(POINTER_BUTTONS * 4);
        }
    }

    private void queryDevice(XClient client, XInputStream inputStream,
                             XOutputStream outputStream)
            throws IOException, XRequestError {
        int selector = inputStream.readUnsignedShort();
        inputStream.skip(2);
        boolean includePointer = selector == ALL_DEVICES
                || selector == ALL_MASTER_DEVICES || selector == MASTER_POINTER_ID;
        boolean includeKeyboard = selector == ALL_DEVICES
                || selector == ALL_MASTER_DEVICES || selector == MASTER_KEYBOARD_ID;
        if (!includePointer && !includeKeyboard) throw new BadValue(selector);

        String pointerName = "BionicX pointer";
        String keyboardName = "BionicX keyboard";
        int deviceCount = (includePointer ? 1 : 0) + (includeKeyboard ? 1 : 0);
        int payloadBytes = 0;
        if (includePointer) payloadBytes += 12 + paddedLength(pointerName) + 40;
        if (includeKeyboard) payloadBytes += 12 + paddedLength(keyboardName);

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.XI_QUERY_DEVICE);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(payloadBytes / 4);
            outputStream.writeShort((short)deviceCount);
            outputStream.writePad(22);
            if (includePointer) {
                writeDevice(outputStream, MASTER_POINTER_ID, USE_MASTER_POINTER,
                        MASTER_KEYBOARD_ID, pointerName, true);
            }
            if (includeKeyboard) {
                writeDevice(outputStream, MASTER_KEYBOARD_ID, USE_MASTER_KEYBOARD,
                        MASTER_POINTER_ID, keyboardName, false);
            }
        }
    }

    private static boolean validDeviceSelector(int deviceId) {
        return deviceId == ALL_DEVICES || deviceId == ALL_MASTER_DEVICES
                || deviceId == MASTER_POINTER_ID || deviceId == MASTER_KEYBOARD_ID;
    }

    private void selectEvents(XClient client, XInputStream inputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        int count = inputStream.readUnsignedShort();
        inputStream.skip(2);
        for (int i = 0; i < count; i++) {
            int deviceId = inputStream.readUnsignedShort();
            int maskWords = inputStream.readUnsignedShort();
            if (!validDeviceSelector(deviceId) || maskWords > 8
                    || maskWords * 4 > client.getRemainingRequestLength()) {
                throw new BadValue(deviceId);
            }
            byte[] mask = new byte[maskWords * 4];
            inputStream.read(mask);
            client.setXiEventMask(window, deviceId, mask);
        }
    }

    private void getSelectedEvents(XClient client, XInputStream inputStream,
                                   XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        ArrayMap<Integer, byte[]> masks = client.getXiEventMasks(window);
        int payloadBytes = 0;
        for (int i = 0; i < masks.size(); i++) {
            payloadBytes += 4 + masks.valueAt(i).length;
        }
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.XI_GET_SELECTED_EVENTS);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(payloadBytes / 4);
            outputStream.writeShort((short)masks.size());
            outputStream.writePad(22);
            for (int i = 0; i < masks.size(); i++) {
                byte[] mask = masks.valueAt(i);
                outputStream.writeShort((short)(int)masks.keyAt(i));
                outputStream.writeShort((short)(mask.length / 4));
                outputStream.write(mask);
            }
        }
    }

    private void grabDevice(XClient client, XInputStream inputStream,
                            XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        int timestamp = inputStream.readInt();
        int cursorId = inputStream.readInt();
        int deviceId = inputStream.readUnsignedShort();
        int mode = inputStream.readUnsignedByte();
        int pairedDeviceMode = inputStream.readUnsignedByte();
        boolean ownerEvents = inputStream.readUnsignedByte() != 0;
        inputStream.skip(1);
        int maskWords = inputStream.readUnsignedShort();
        if (deviceId != MASTER_POINTER_ID && deviceId != MASTER_KEYBOARD_ID)
            throw new BadValue(deviceId);
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        Cursor cursor = cursorId == 0 ? null
                : xServer.cursorManager.getCursor(cursorId);
        if (cursorId != 0 && cursor == null) throw new BadCursor(cursorId);
        if (timestamp != 0 || mode != 1 || pairedDeviceMode != 1
                || maskWords > 8
                || maskWords * 4 > client.getRemainingRequestLength()) {
            throw new BadImplementation();
        }

        byte[] xiMask = new byte[maskWords * 4];
        inputStream.read(xiMask);
        int coreMask = 0;
        if (maskBit(xiMask, XI_BUTTON_PRESS)) coreMask |= Event.BUTTON_PRESS;
        if (maskBit(xiMask, XI_BUTTON_RELEASE)) coreMask |= Event.BUTTON_RELEASE;
        if (maskBit(xiMask, XI_MOTION)) coreMask |= Event.POINTER_MOTION;
        int status;
        boolean grabbedByOtherClient = deviceId == MASTER_POINTER_ID
                ? xServer.grabManager.getWindow() != null
                    && xServer.grabManager.getClient() != client
                : xServer.grabManager.getKeyboardClient() != null
                    && xServer.grabManager.getKeyboardClient() != client;
        if (grabbedByOtherClient) {
            status = 1; // AlreadyGrabbed
        }
        else if (window.getMapState() != Window.MapState.VIEWABLE) {
            status = 3; // GrabNotViewable
        }
        else {
            status = 0; // GrabSuccess
            if (deviceId == MASTER_POINTER_ID) {
                xServer.grabManager.activatePointerGrab(window, ownerEvents,
                        new Bitmask(coreMask), client, cursor);
                pointerGrabMask = xiMask.clone();
            }
            else {
                xServer.grabManager.activateKeyboardGrab(window, ownerEvents,
                        client);
                keyboardGrabMask = xiMask.clone();
            }
        }
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.XI_GRAB_DEVICE);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeByte((byte)status);
            outputStream.writePad(23);
        }
    }

    private static boolean maskBit(byte[] mask, int bit) {
        return bit / 8 < mask.length
                && (mask[bit / 8] & (1 << (bit & 7))) != 0;
    }

    private void ungrabDevice(XClient client, XInputStream inputStream)
            throws XRequestError {
        int timestamp = inputStream.readInt();
        int deviceId = inputStream.readUnsignedShort();
        inputStream.skip(2);
        if (deviceId != MASTER_POINTER_ID && deviceId != MASTER_KEYBOARD_ID)
            throw new BadValue(deviceId);
        if (timestamp != 0) throw new BadImplementation();
        if (deviceId == MASTER_POINTER_ID) {
            boolean owned = xServer.grabManager.getClient() == client;
            xServer.grabManager.deactivatePointerGrab(client);
            if (owned) pointerGrabMask = null;
        }
        else {
            boolean owned = xServer.grabManager.getKeyboardClient() == client;
            xServer.grabManager.deactivateKeyboardGrab(client);
            if (owned) keyboardGrabMask = null;
        }
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        switch (client.getRequestData()) {
            case ClientOpcodes.GET_EXTENSION_VERSION:
                getExtensionVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.XI_QUERY_POINTER:
                queryPointer(client, inputStream, outputStream);
                break;
            case ClientOpcodes.XI_CHANGE_CURSOR:
                changeCursor(client, inputStream);
                break;
            case ClientOpcodes.XI_SELECT_EVENTS:
                selectEvents(client, inputStream);
                break;
            case ClientOpcodes.XI_QUERY_VERSION:
                queryVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.XI_QUERY_DEVICE:
                queryDevice(client, inputStream, outputStream);
                break;
            case ClientOpcodes.XI_GRAB_DEVICE:
                grabDevice(client, inputStream, outputStream);
                break;
            case ClientOpcodes.XI_UNGRAB_DEVICE:
                ungrabDevice(client, inputStream);
                break;
            case ClientOpcodes.XI_GET_SELECTED_EVENTS:
                getSelectedEvents(client, inputStream, outputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
