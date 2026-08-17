package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.core.Bitmask;
import com.winlator.xserver.Atom;
import com.winlator.xserver.Cursor;
import com.winlator.xserver.GrabManager;
import com.winlator.xserver.Keyboard;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.Window;
import com.winlator.xserver.events.Event;
import com.winlator.xserver.events.XInputCrossingEvent;
import com.winlator.xserver.events.XInputDeviceEvent;
import com.winlator.xserver.errors.BadAtom;
import com.winlator.xserver.errors.BadCursor;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadMatch;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.BadWindow;
import com.winlator.xserver.errors.XRequestError;
import com.winlator.xserver.Pointer;

import android.util.Log;
import android.util.SparseArray;

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
    private int deviceMode = 0;
    private byte[] deviceButtonMap = {1, 2, 3, 4, 5, 6, 7};
    private final SparseArray<DeviceProperty> pointerProperties = new SparseArray<>();
    private final SparseArray<DeviceProperty> keyboardProperties = new SparseArray<>();

    private static final class DeviceProperty {
        private int type;
        private int format;
        private byte[] data;

        private DeviceProperty(int type, int format, byte[] data) {
            this.type = type;
            this.format = format;
            this.data = data;
        }
    }

    private static abstract class ClientOpcodes {
        private static final byte GET_EXTENSION_VERSION = 1;
        private static final byte LIST_INPUT_DEVICES = 2;
        private static final byte OPEN_DEVICE = 3;
        private static final byte CLOSE_DEVICE = 4;
        private static final byte SET_DEVICE_MODE = 5;
        private static final byte SELECT_EXTENSION_EVENT = 6;
        private static final byte GET_SELECTED_EXTENSION_EVENTS = 7;
        private static final byte GET_FEEDBACK_CONTROL = 22;
        private static final byte CHANGE_FEEDBACK_CONTROL = 23;
        private static final byte GET_DEVICE_BUTTON_MAPPING = 28;
        private static final byte SET_DEVICE_BUTTON_MAPPING = 29;
        private static final byte LIST_DEVICE_PROPERTIES = 36;
        private static final byte CHANGE_DEVICE_PROPERTY = 37;
        private static final byte DELETE_DEVICE_PROPERTY = 38;
        private static final byte GET_DEVICE_PROPERTY = 39;
        private static final byte XI_QUERY_POINTER = 40;
        private static final byte XI_WARP_POINTER = 41;
        private static final byte XI_CHANGE_CURSOR = 42;
        private static final byte XI_GET_CLIENT_POINTER = 45;
        private static final byte XI_SET_FOCUS = 49;
        private static final byte XI_GET_FOCUS = 50;
        private static final byte XI_SELECT_EVENTS = 46;
        private static final byte XI_QUERY_VERSION = 47;
        private static final byte XI_QUERY_DEVICE = 48;
        private static final byte XI_GRAB_DEVICE = 51;
        private static final byte XI_UNGRAB_DEVICE = 52;
        private static final byte XI_ALLOW_EVENTS = 53;
        private static final byte XI_PASSIVE_GRAB_DEVICE = 54;
        private static final byte XI_PASSIVE_UNGRAB_DEVICE = 55;
        private static final byte XI_GET_PROPERTY = 59;
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

    private void getClientPointer(XClient client, XInputStream inputStream,
                                  XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        if (windowId != 0 && xServer.windowManager.getWindow(windowId) == null)
            throw new BadWindow(windowId);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeByte((byte)1); // client pointer is set
            outputStream.writeByte((byte)0);
            outputStream.writeShort((short)MASTER_POINTER_ID);
            outputStream.writePad(20);
        }
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
        if (deviceId == MASTER_POINTER_ID
                && xServer.grabManager.isPassiveSynchronousPointerGrab()) {
            XClient grabClient = xServer.grabManager.getClient();
            Window grabWindow = xServer.grabManager.getWindow();
            if (grabClient == null || grabWindow == null
                    || !grabWindow.attributes.isEnabled()) return;
            Window target = grabWindow;
            if (xServer.grabManager.isOwnerEvents()) {
                Window selected = selectedAncestor(grabClient, sourceWindow,
                        deviceId, eventType);
                if (selected != null) target = selected;
            }
            sendDeviceEventToClient(grabClient, deviceId, eventType, detail,
                    sourceWindow, target, rootX, rootY);
            if (xServer.pointer.getY() < 40)
                Log.i("BionicX", "BXINFO grab-xi sent type=" + eventType
                        + " client=" + GrabManager.describeClient(grabClient)
                        + " target=0x" + Integer.toHexString(target.id)
                        + " cookie=" + getMajorOpcode());
            return;
        }
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

    private static final int IS_X_POINTER = 0;
    private static final int IS_X_KEYBOARD = 1;
    private static final int KEY_CLASS = 0;
    private static final int VALUATOR_CLASS = 2;
    private static final String POINTER_NAME = "Virtual core pointer";
    private static final String KEYBOARD_NAME = "Virtual core keyboard";

    private void listInputDevices(XClient client, XOutputStream outputStream)
            throws IOException {
        int mouseType = Atom.internAtom("MOUSE");
        int keyboardType = Atom.internAtom("KEYBOARD");
        int pointerClasses = 4 + 8 + 12 * 2;
        int keyboardClasses = 8;
        int names = 1 + POINTER_NAME.length() + 1 + KEYBOARD_NAME.length();
        int namesPadded = (names + 3) & ~3;
        int extra = 8 * 2 + pointerClasses + keyboardClasses + namesPadded;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.LIST_INPUT_DEVICES);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(extra / 4);
            outputStream.writeByte((byte)2);
            outputStream.writePad(23);

            outputStream.writeInt(mouseType);
            outputStream.writeByte((byte)MASTER_POINTER_ID);
            outputStream.writeByte((byte)2);
            outputStream.writeByte((byte)IS_X_POINTER);
            outputStream.writeByte((byte)0);

            outputStream.writeInt(keyboardType);
            outputStream.writeByte((byte)MASTER_KEYBOARD_ID);
            outputStream.writeByte((byte)1);
            outputStream.writeByte((byte)IS_X_KEYBOARD);
            outputStream.writeByte((byte)0);

            outputStream.writeByte((byte)BUTTON_CLASS);
            outputStream.writeByte((byte)4);
            outputStream.writeShort((short)POINTER_BUTTONS);

            outputStream.writeByte((byte)VALUATOR_CLASS);
            outputStream.writeByte((byte)(8 + 12 * 2));
            outputStream.writeByte((byte)2);
            outputStream.writeByte((byte)0);
            outputStream.writeInt(0);
            for (int axis = 0; axis < 2; axis++) {
                outputStream.writeInt(1);
                outputStream.writeInt(0);
                outputStream.writeInt(0);
            }

            outputStream.writeByte((byte)KEY_CLASS);
            outputStream.writeByte((byte)8);
            outputStream.writeByte((byte)Keyboard.MIN_KEYCODE);
            outputStream.writeByte((byte)Keyboard.MAX_KEYCODE);
            outputStream.writeShort(Keyboard.KEYS_COUNT);
            outputStream.writePad(2);

            outputStream.writeByte((byte)POINTER_NAME.length());
            outputStream.write(POINTER_NAME.getBytes());
            outputStream.writeByte((byte)KEYBOARD_NAME.length());
            outputStream.write(KEYBOARD_NAME.getBytes());
            if ((namesPadded - names) > 0) outputStream.writePad(namesPadded - names);
        }
    }

    private void openDevice(XClient client, XInputStream inputStream,
                            XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceId = inputStream.readUnsignedByte();
        inputStream.skip(3);
        if (deviceId != MASTER_POINTER_ID && deviceId != MASTER_KEYBOARD_ID)
            throw new BadValue(deviceId);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.OPEN_DEVICE);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeByte((byte)0);
            outputStream.writePad(23);
        }
    }

    private void closeDevice(XClient client, XInputStream inputStream)
            throws XRequestError {
        int deviceId = inputStream.readUnsignedByte();
        inputStream.skip(3);
        if (deviceId != MASTER_POINTER_ID && deviceId != MASTER_KEYBOARD_ID)
            throw new BadValue(deviceId);
    }

    private void requireMasterDevice(int deviceId) throws XRequestError {
        if (deviceId != MASTER_POINTER_ID && deviceId != MASTER_KEYBOARD_ID)
            throw new BadValue(deviceId);
    }

    private SparseArray<DeviceProperty> propertiesFor(int deviceId) {
        return deviceId == MASTER_KEYBOARD_ID ? keyboardProperties : pointerProperties;
    }

    private void setDeviceMode(XClient client, XInputStream inputStream,
                               XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceId = inputStream.readUnsignedByte();
        int mode = inputStream.readUnsignedByte();
        inputStream.skip(2);
        requireMasterDevice(deviceId);
        if (mode != 0 && mode != 1) throw new BadValue(mode);
        deviceMode = mode;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.SET_DEVICE_MODE);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeByte((byte)0);
            outputStream.writePad(23);
        }
    }

    private void getFeedbackControl(XClient client, XInputStream inputStream,
                                    XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceId = inputStream.readUnsignedByte();
        inputStream.skip(3);
        requireMasterDevice(deviceId);
        boolean pointer = deviceId == MASTER_POINTER_ID;
        int extra = pointer ? 12 : 0;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.GET_FEEDBACK_CONTROL);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(extra / 4);
            outputStream.writeShort((short)(pointer ? 1 : 0));
            outputStream.writePad(22);
            if (pointer) {
                outputStream.writeByte((byte)1); // PtrFeedbackClass
                outputStream.writeByte((byte)0);
                outputStream.writeShort((short)12);
                outputStream.writePad(2);
                outputStream.writeShort((short)xServer.pointer.getAccelNumerator());
                outputStream.writeShort((short)xServer.pointer.getAccelDenominator());
                outputStream.writeShort((short)xServer.pointer.getAccelThreshold());
            }
        }
    }

    private void changeFeedbackControl(XClient client, XInputStream inputStream)
            throws XRequestError {
        int mask = inputStream.readInt();
        int deviceId = inputStream.readUnsignedByte();
        inputStream.skip(3); // feedback id + pad
        requireMasterDevice(deviceId);
        if (client.getRemainingRequestLength() >= 12) {
            inputStream.skip(4); // class, id, length
            inputStream.skip(2);
            int threshold = inputStream.readShort();
            int accelNum = inputStream.readShort();
            int accelDenom = inputStream.readShort();
            if (deviceId == MASTER_POINTER_ID) {
                int num = (mask & 1) != 0 ? accelNum : xServer.pointer.getAccelNumerator();
                int denom = (mask & 2) != 0 ? accelDenom : xServer.pointer.getAccelDenominator();
                int thr = (mask & 4) != 0 ? threshold : xServer.pointer.getAccelThreshold();
                if ((mask & 2) != 0 && denom == 0) throw new BadValue(0);
                xServer.pointer.setAccel(num, denom, thr);
            }
        }
        inputStream.skip(client.getRemainingRequestLength());
    }

    private void selectExtensionEvent(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        int count = inputStream.readUnsignedShort();
        inputStream.skip(2);
        if (xServer.windowManager.getWindow(windowId) == null)
            throw new BadWindow(windowId);
        for (int i = 0; i < count; i++) inputStream.readInt();
    }

    private void getSelectedExtensionEvents(XClient client,
                                            XInputStream inputStream,
                                            XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        if (xServer.windowManager.getWindow(windowId) == null)
            throw new BadWindow(windowId);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.GET_SELECTED_EXTENSION_EVENTS);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)0);
            outputStream.writeShort((short)0);
            outputStream.writePad(20);
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

    private void getDeviceButtonMapping(XClient client, XInputStream inputStream,
                                        XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceId = inputStream.readUnsignedByte();
        inputStream.skip(3);
        requireMasterDevice(deviceId);
        byte[] map = deviceButtonMap;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.GET_DEVICE_BUTTON_MAPPING);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt((map.length + 3) / 4);
            outputStream.writeByte((byte)map.length);
            outputStream.writePad(23);
            outputStream.write(map);
            outputStream.writePad(-map.length & 3);
        }
    }

    private void setDeviceButtonMapping(XClient client, XInputStream inputStream,
                                        XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceId = inputStream.readUnsignedByte();
        int mapLength = inputStream.readUnsignedByte();
        inputStream.skip(2);
        requireMasterDevice(deviceId);
        byte[] map = new byte[mapLength];
        inputStream.read(map);
        inputStream.skip((-mapLength) & 3);
        boolean ok = mapLength == deviceButtonMap.length;
        if (ok) deviceButtonMap = map;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.SET_DEVICE_BUTTON_MAPPING);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeByte((byte)(ok ? 0 : 2));
            outputStream.writePad(23);
        }
    }

    private void listDeviceProperties(XClient client, XInputStream inputStream,
                                      XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceId = inputStream.readUnsignedByte();
        inputStream.skip(3);
        requireMasterDevice(deviceId);
        SparseArray<DeviceProperty> properties = propertiesFor(deviceId);
        int count;
        int[] atoms;
        synchronized (properties) {
            count = properties.size();
            atoms = new int[count];
            for (int i = 0; i < count; i++) atoms[i] = properties.keyAt(i);
        }
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.LIST_DEVICE_PROPERTIES);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(count);
            outputStream.writeShort((short)count);
            outputStream.writePad(22);
            for (int atom : atoms) outputStream.writeInt(atom);
        }
    }

    private void changeDeviceProperty(XClient client, XInputStream inputStream)
            throws XRequestError {
        int property = inputStream.readInt();
        int type = inputStream.readInt();
        int deviceId = inputStream.readUnsignedByte();
        int format = inputStream.readUnsignedByte();
        int mode = inputStream.readUnsignedByte();
        inputStream.skip(1);
        int nUnits = inputStream.readInt();
        requireMasterDevice(deviceId);
        if (!Atom.isValid(property) || !Atom.isValid(type))
            throw new BadAtom(property);
        if (format != 8 && format != 16 && format != 32) throw new BadValue(format);
        if (mode > 2) throw new BadValue(mode);
        int dataBytes = nUnits * (format / 8);
        byte[] incoming = new byte[dataBytes];
        inputStream.read(incoming);
        inputStream.skip((-dataBytes) & 3);
        inputStream.skip(client.getRemainingRequestLength());
        SparseArray<DeviceProperty> properties = propertiesFor(deviceId);
        synchronized (properties) {
            DeviceProperty existing = properties.get(property);
            if (mode == 0 || existing == null) {
                properties.put(property, new DeviceProperty(type, format, incoming));
                return;
            }
            if (existing.format != format || existing.type != type)
                throw new BadMatch();
            byte[] merged = new byte[existing.data.length + incoming.length];
            if (mode == 1) {
                System.arraycopy(incoming, 0, merged, 0, incoming.length);
                System.arraycopy(existing.data, 0, merged, incoming.length,
                        existing.data.length);
            }
            else {
                System.arraycopy(existing.data, 0, merged, 0, existing.data.length);
                System.arraycopy(incoming, 0, merged, existing.data.length,
                        incoming.length);
            }
            existing.data = merged;
        }
    }

    private void deleteDeviceProperty(XInputStream inputStream) throws XRequestError {
        int property = inputStream.readInt();
        int deviceId = inputStream.readUnsignedByte();
        inputStream.skip(3);
        requireMasterDevice(deviceId);
        if (!Atom.isValid(property)) throw new BadAtom(property);
        SparseArray<DeviceProperty> properties = propertiesFor(deviceId);
        synchronized (properties) {
            properties.remove(property);
        }
    }

    private void getDeviceProperty(XClient client, XInputStream inputStream,
                                   XOutputStream outputStream)
            throws IOException, XRequestError {
        int property = inputStream.readInt();
        inputStream.readInt(); // requested type
        int longOffset = inputStream.readInt();
        int longLength = inputStream.readInt();
        int deviceId = inputStream.readUnsignedByte();
        boolean delete = inputStream.readUnsignedByte() != 0;
        inputStream.skip(2);
        requireMasterDevice(deviceId);
        if (!Atom.isValid(property)) throw new BadAtom(property);
        DeviceProperty stored;
        SparseArray<DeviceProperty> properties = propertiesFor(deviceId);
        synchronized (properties) {
            stored = properties.get(property);
            if (delete && stored != null) properties.remove(property);
        }
        int offsetBytes = longOffset * 4;
        byte[] slice = new byte[0];
        int bytesAfter = 0;
        int format = 0;
        int type = 0;
        int nItems = 0;
        if (stored != null) {
            type = stored.type;
            format = stored.format;
            if (offsetBytes > stored.data.length) throw new BadValue(longOffset);
            int available = stored.data.length - offsetBytes;
            int wanted = longLength * 4;
            int take = Math.min(available, wanted);
            slice = new byte[take];
            System.arraycopy(stored.data, offsetBytes, slice, 0, take);
            bytesAfter = available - take;
            int unit = Math.max(1, format / 8);
            nItems = take / unit;
        }
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.GET_DEVICE_PROPERTY);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt((slice.length + 3) / 4);
            outputStream.writeInt(type);
            outputStream.writeInt(bytesAfter);
            outputStream.writeInt(nItems);
            outputStream.writeByte((byte)format);
            outputStream.writeByte((byte)deviceId);
            outputStream.writePad(10);
            outputStream.write(slice);
            outputStream.writePad(-slice.length & 3);
        }
    }

    private void warpPointer(XInputStream inputStream) throws XRequestError {
        int srcWindowId = inputStream.readInt();
        int dstWindowId = inputStream.readInt();
        int srcX = inputStream.readInt() >> 16;
        int srcY = inputStream.readInt() >> 16;
        int srcWidth = inputStream.readUnsignedShort();
        int srcHeight = inputStream.readUnsignedShort();
        int dstX = inputStream.readInt() >> 16;
        int dstY = inputStream.readInt() >> 16;
        int deviceId = inputStream.readUnsignedShort();
        inputStream.skip(2);
        if (deviceId != MASTER_POINTER_ID && deviceId != ALL_MASTER_DEVICES)
            throw new BadValue(deviceId);
        Window srcWindow = srcWindowId == 0 ? null
                : xServer.windowManager.getWindow(srcWindowId);
        if (srcWindowId != 0 && srcWindow == null) throw new BadWindow(srcWindowId);
        Window dstWindow = dstWindowId == 0 ? null
                : xServer.windowManager.getWindow(dstWindowId);
        if (dstWindowId != 0 && dstWindow == null) throw new BadWindow(dstWindowId);
        if (srcWindow != null) {
            if (srcWidth == 0) srcWidth = srcWindow.getWidth() - srcX;
            if (srcHeight == 0) srcHeight = srcWindow.getHeight() - srcY;
            short[] local = srcWindow.rootPointToLocal(
                    xServer.pointer.getX(), xServer.pointer.getY());
            if (local[0] < srcX || local[1] < srcY
                    || local[0] >= srcX + srcWidth
                    || local[1] >= srcY + srcHeight) return;
        }
        if (dstWindow == null) {
            xServer.pointer.setPosition(xServer.pointer.getX() + dstX,
                    xServer.pointer.getY() + dstY);
        }
        else {
            short[] root = dstWindow.localPointToRoot((short)dstX, (short)dstY);
            xServer.pointer.setPosition(root[0], root[1]);
        }
    }

    private void setFocus(XInputStream inputStream) throws XRequestError {
        int windowId = inputStream.readInt();
        inputStream.skip(4); // timestamp
        int deviceId = inputStream.readUnsignedShort();
        inputStream.skip(2);
        if (deviceId != MASTER_POINTER_ID && deviceId != MASTER_KEYBOARD_ID
                && deviceId != ALL_MASTER_DEVICES)
            throw new BadValue(deviceId);
        if (windowId == 0) {
            xServer.windowManager.setFocus(null,
                    xServer.windowManager.getFocusRevertTo());
            return;
        }
        if (windowId == 1) {
            xServer.windowManager.setPointerRootFocus(
                    xServer.windowManager.getFocusRevertTo());
            return;
        }
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        if (!window.attributes.isViewable()) throw new BadMatch();
        xServer.windowManager.setFocus(window,
                xServer.windowManager.getFocusRevertTo());
    }

    private void getFocus(XClient client, XInputStream inputStream,
                          XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceId = inputStream.readUnsignedShort();
        inputStream.skip(2);
        if (deviceId != MASTER_POINTER_ID && deviceId != MASTER_KEYBOARD_ID
                && deviceId != ALL_MASTER_DEVICES)
            throw new BadValue(deviceId);
        Window focused = xServer.windowManager.getFocusedWindow();
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.XI_GET_FOCUS);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(focused == null ? 0 : focused.id);
            outputStream.writePad(20);
        }
    }

    private void getProperty(XClient client, XInputStream inputStream,
                             XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceId = inputStream.readUnsignedShort();
        inputStream.skip(2); // delete flag and padding
        inputStream.readInt(); // property atom
        inputStream.readInt(); // requested type atom
        inputStream.readInt(); // long offset
        inputStream.readInt(); // long length
        if (deviceId != MASTER_POINTER_ID && deviceId != MASTER_KEYBOARD_ID)
            throw new BadValue(deviceId);
        // The synthetic master devices expose no XI properties. None/format 0
        // is the protocol's successful empty-property result.
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.XI_GET_PROPERTY);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(0); // property type: None
            outputStream.writeInt(0); // bytes after
            outputStream.writeInt(0); // item count
            outputStream.writeByte((byte)0); // format
            outputStream.writePad(11);
        }
    }

    private void grabDevice(XClient client, XInputStream inputStream,
                            XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        inputStream.readInt(); // timestamp is advisory
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
        // GDK passes the last event time and often XIGrabModeSync.
        // Freeze is not implemented; accept the grab and deliver
        // asynchronously.
        if ((mode != 0 && mode != 1)
                || (pairedDeviceMode != 0 && pairedDeviceMode != 1)
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
        inputStream.readInt(); // timestamp is advisory
        int deviceId = inputStream.readUnsignedShort();
        inputStream.skip(2);
        if (deviceId != MASTER_POINTER_ID && deviceId != MASTER_KEYBOARD_ID)
            throw new BadValue(deviceId);
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

    private void allowEvents(XClient client, XInputStream inputStream)
            throws XRequestError {
        inputStream.readInt(); // timestamp is advisory
        int deviceId = inputStream.readUnsignedShort();
        int mode = inputStream.readUnsignedByte() & 0xff;
        inputStream.skip(1);
        // XI 2.2 appends touchid + grab_window. Skip any tail.
        inputStream.skip(client.getRemainingRequestLength());
        if (deviceId != MASTER_POINTER_ID && deviceId != MASTER_KEYBOARD_ID
                && deviceId != ALL_DEVICES && deviceId != ALL_MASTER_DEVICES)
            throw new BadValue(deviceId);
        if (mode > 7) throw new BadValue(mode);
        if (deviceId == MASTER_KEYBOARD_ID) return;
        if (xServer.grabManager.getClient() != client) {
            Log.i("BionicX", "BXINFO allow-events XI ignored mode=" + mode
                    + " client=" + GrabManager.describeClient(client)
                    + " grabClient=" + GrabManager.describeClient(
                            xServer.grabManager.getClient()));
            return;
        }
        Window grab = xServer.grabManager.getWindow();
        Log.i("BionicX", "BXINFO allow-events XI mode=" + mode
                + " grab=0x" + Integer.toHexString(grab != null ? grab.id : 0)
                + " client=" + GrabManager.describeClient(client)
                + " sync=" + xServer.grabManager.isPointerSynchronous());
        // XIAsyncDevice=0, XISyncDevice=1, XIReplayDevice=2. Passive
        // sync grabs freeze the pointer; GDK/xfwm thaw through XI.
        if (mode == 2) {
            Pointer.Button button =
                    xServer.grabManager.getPassiveActivationButton();
            if (!xServer.grabManager.isPointerSynchronous() || button == null)
                return;
            xServer.grabManager.deactivatePointerGrabForReplay();
            xServer.inputDeviceManager.replayPointerButtonPress(button);
            return;
        }
        if (mode == 0 || mode == 1)
            xServer.grabManager.thawSynchronousPointer();
    }

    private static int coreModifiers(int xiModifiers) {
        return xiModifiers == 0x80000000 ? 0x8000 : (xiModifiers & 0xffff);
    }

    private void passiveGrabDevice(XClient client, XInputStream inputStream,
                                   XOutputStream outputStream)
            throws IOException, XRequestError {
        inputStream.readInt(); // timestamp is advisory
        int windowId = inputStream.readInt();
        int cursorId = inputStream.readInt();
        int detail = inputStream.readInt();
        int deviceId = inputStream.readUnsignedShort();
        int numModifiers = inputStream.readUnsignedShort();
        int maskWords = inputStream.readUnsignedShort();
        int grabType = inputStream.readUnsignedByte();
        int grabMode = inputStream.readUnsignedByte();
        int pairedDeviceMode = inputStream.readUnsignedByte();
        boolean ownerEvents = inputStream.readUnsignedByte() != 0;
        inputStream.skip(2);
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        Cursor cursor = cursorId == 0 ? null
                : xServer.cursorManager.getCursor(cursorId);
        if (cursorId != 0 && cursor == null) throw new BadCursor(cursorId);
        if (deviceId != MASTER_POINTER_ID && deviceId != MASTER_KEYBOARD_ID
                && deviceId != ALL_DEVICES && deviceId != ALL_MASTER_DEVICES)
            throw new BadValue(deviceId);
        // GDK installs XIGrabButton / XIGrabKeycode with XIGrabModeSync.
        // Freeze is not implemented; accept and deliver asynchronously.
        if ((grabMode != 0 && grabMode != 1)
                || (pairedDeviceMode != 0 && pairedDeviceMode != 1)
                || grabType > 6 || numModifiers > 32 || maskWords > 8
                || maskWords * 4 + numModifiers * 4
                    > client.getRemainingRequestLength()) {
            throw new BadImplementation();
        }
        byte[] xiMask = new byte[maskWords * 4];
        inputStream.read(xiMask);
        int[] failedModifiers = new int[numModifiers];
        byte[] failedStatuses = new byte[numModifiers];
        int failed = 0;
        int coreMask = 0;
        if (maskBit(xiMask, XI_BUTTON_PRESS)) coreMask |= Event.BUTTON_PRESS;
        if (maskBit(xiMask, XI_BUTTON_RELEASE)) coreMask |= Event.BUTTON_RELEASE;
        if (maskBit(xiMask, XI_MOTION)) coreMask |= Event.POINTER_MOTION;
        if (grabType == 0) coreMask |= Event.BUTTON_PRESS | Event.BUTTON_RELEASE;
        for (int i = 0; i < numModifiers; i++) {
            int modifier = inputStream.readInt();
            int coreMods = coreModifiers(modifier);
            boolean ok = true;
            if (grabType == 0) {
                ok = xServer.grabManager.addPassiveButtonGrab(window,
                        detail & 0xff, coreMods, ownerEvents,
                        new Bitmask(coreMask), client, cursor, grabMode == 0);
            }
            else if (grabType == 1) {
                ok = xServer.grabManager.addPassiveKeyGrab(window,
                        detail & 0xff, coreMods, ownerEvents, client);
            }
            if (!ok) {
                failedModifiers[failed] = modifier;
                failedStatuses[failed] = 1; // AlreadyGrabbed
                failed++;
            }
        }
        inputStream.skip(client.getRemainingRequestLength());
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.XI_PASSIVE_GRAB_DEVICE);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(failed * 2);
            outputStream.writeShort((short)failed);
            outputStream.writePad(22);
            for (int i = 0; i < failed; i++) {
                outputStream.writeInt(failedModifiers[i]);
                outputStream.writeByte(failedStatuses[i]);
                outputStream.writePad(3);
            }
        }
    }

    private void passiveUngrabDevice(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        int detail = inputStream.readInt();
        int deviceId = inputStream.readUnsignedShort();
        int numModifiers = inputStream.readUnsignedShort();
        int grabType = inputStream.readUnsignedByte();
        inputStream.skip(3);
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        if (deviceId != MASTER_POINTER_ID && deviceId != MASTER_KEYBOARD_ID
                && deviceId != ALL_DEVICES && deviceId != ALL_MASTER_DEVICES)
            throw new BadValue(deviceId);
        if (grabType > 6 || numModifiers > 32
                || numModifiers * 4 > client.getRemainingRequestLength()) {
            throw new BadImplementation();
        }
        for (int i = 0; i < numModifiers; i++) {
            int coreMods = coreModifiers(inputStream.readInt());
            if (grabType == 0) {
                xServer.grabManager.removePassiveButtonGrabs(window,
                        detail & 0xff, coreMods, client);
            }
            else if (grabType == 1) {
                xServer.grabManager.removePassiveKeyGrabs(window,
                        detail & 0xff, coreMods, client);
            }
        }
        inputStream.skip(client.getRemainingRequestLength());
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        switch (client.getRequestData()) {
            case ClientOpcodes.GET_EXTENSION_VERSION:
                getExtensionVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.LIST_INPUT_DEVICES:
                listInputDevices(client, outputStream);
                break;
            case ClientOpcodes.OPEN_DEVICE:
                openDevice(client, inputStream, outputStream);
                break;
            case ClientOpcodes.CLOSE_DEVICE:
                closeDevice(client, inputStream);
                break;
            case ClientOpcodes.SET_DEVICE_MODE:
                setDeviceMode(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_FEEDBACK_CONTROL:
                getFeedbackControl(client, inputStream, outputStream);
                break;
            case ClientOpcodes.CHANGE_FEEDBACK_CONTROL:
                changeFeedbackControl(client, inputStream);
                break;
            case ClientOpcodes.GET_DEVICE_BUTTON_MAPPING:
                getDeviceButtonMapping(client, inputStream, outputStream);
                break;
            case ClientOpcodes.SET_DEVICE_BUTTON_MAPPING:
                setDeviceButtonMapping(client, inputStream, outputStream);
                break;
            case ClientOpcodes.LIST_DEVICE_PROPERTIES:
                listDeviceProperties(client, inputStream, outputStream);
                break;
            case ClientOpcodes.CHANGE_DEVICE_PROPERTY:
                changeDeviceProperty(client, inputStream);
                break;
            case ClientOpcodes.DELETE_DEVICE_PROPERTY:
                deleteDeviceProperty(inputStream);
                break;
            case ClientOpcodes.GET_DEVICE_PROPERTY:
                getDeviceProperty(client, inputStream, outputStream);
                break;
            case ClientOpcodes.XI_WARP_POINTER:
                warpPointer(inputStream);
                break;
            case ClientOpcodes.XI_SET_FOCUS:
                setFocus(inputStream);
                break;
            case ClientOpcodes.XI_GET_FOCUS:
                getFocus(client, inputStream, outputStream);
                break;
            case ClientOpcodes.SELECT_EXTENSION_EVENT:
                selectExtensionEvent(client, inputStream);
                break;
            case ClientOpcodes.GET_SELECTED_EXTENSION_EVENTS:
                getSelectedExtensionEvents(client, inputStream, outputStream);
                break;
            case ClientOpcodes.XI_QUERY_POINTER:
                queryPointer(client, inputStream, outputStream);
                break;
            case ClientOpcodes.XI_CHANGE_CURSOR:
                changeCursor(client, inputStream);
                break;
            case ClientOpcodes.XI_GET_CLIENT_POINTER:
                getClientPointer(client, inputStream, outputStream);
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
            case ClientOpcodes.XI_ALLOW_EVENTS:
                allowEvents(client, inputStream);
                break;
            case ClientOpcodes.XI_PASSIVE_GRAB_DEVICE:
                passiveGrabDevice(client, inputStream, outputStream);
                break;
            case ClientOpcodes.XI_PASSIVE_UNGRAB_DEVICE:
                passiveUngrabDevice(client, inputStream);
                break;
            case ClientOpcodes.XI_GET_PROPERTY:
                getProperty(client, inputStream, outputStream);
                break;
            case ClientOpcodes.XI_GET_SELECTED_EVENTS:
                getSelectedEvents(client, inputStream, outputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
