package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Window;

import java.io.IOException;

/** XI2 DeviceEvent encoded as an X GenericEvent. */
public class XInputDeviceEvent extends Event {
    private static final int GENERIC_EVENT = 35;
    private final byte extensionOpcode;
    private final short deviceId;
    private final int eventType;
    private final int detail;
    private final int timestamp;
    private final Window root;
    private final Window event;
    private final Window child;
    private final short rootX;
    private final short rootY;
    private final short eventX;
    private final short eventY;
    private final int baseModifiers;
    private final int lockedModifiers;
    private final int effectiveModifiers;
    private final int buttonState;

    public XInputDeviceEvent(byte extensionOpcode, int deviceId, int eventType,
                             int detail, Window root, Window event, Window child,
                             short rootX, short rootY, short eventX, short eventY,
                             int baseModifiers,
                             int lockedModifiers, int effectiveModifiers,
                             int buttonState) {
        super(GENERIC_EVENT);
        this.extensionOpcode = extensionOpcode;
        this.deviceId = (short)deviceId;
        this.eventType = eventType;
        this.detail = detail;
        this.timestamp = (int)System.currentTimeMillis();
        this.root = root;
        this.event = event;
        this.child = child;
        this.rootX = rootX;
        this.rootY = rootY;
        this.eventX = eventX;
        this.eventY = eventY;
        this.baseModifiers = baseModifiers;
        this.lockedModifiers = lockedModifiers;
        this.effectiveModifiers = effectiveModifiers;
        this.buttonState = buttonState;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream)
            throws IOException {
        // xXIDeviceEvent is 80 bytes plus button/valuator words.
        // xXIGroupInfo is 4 CARD8s, not 4 CARD32s. A 96-byte encoding
        // put the button mask 12 bytes late, so libXi/GDK read zeros.
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte(extensionOpcode);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(13); // (84 - 32) / 4
            outputStream.writeShort((short)eventType);
            outputStream.writeShort(deviceId);
            outputStream.writeInt(timestamp);
            outputStream.writeInt(detail);
            outputStream.writeInt(root.id);
            outputStream.writeInt(event.id);
            outputStream.writeInt(child != null ? child.id : 0);
            outputStream.writeInt(rootX << 16);
            outputStream.writeInt(rootY << 16);
            outputStream.writeInt(eventX << 16);
            outputStream.writeInt(eventY << 16);
            outputStream.writeShort((short)1); // buttons_len, in 4-byte units
            outputStream.writeShort((short)0); // valuators_len
            outputStream.writeShort(deviceId);
            outputStream.writeShort((short)0);
            outputStream.writeInt(0); // flags
            outputStream.writeInt(baseModifiers);
            outputStream.writeInt(0); // latched modifiers
            outputStream.writeInt(lockedModifiers);
            outputStream.writeInt(effectiveModifiers);
            outputStream.writeByte((byte)0); // group.base
            outputStream.writeByte((byte)0); // group.latched
            outputStream.writeByte((byte)0); // group.locked
            outputStream.writeByte((byte)0); // group.effective
            outputStream.writeInt(buttonState);
        }
    }
}
