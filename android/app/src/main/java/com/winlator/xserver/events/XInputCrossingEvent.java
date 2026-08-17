package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Window;

import java.io.IOException;

/** XI2 Enter/Leave event encoded as an X GenericEvent. */
public class XInputCrossingEvent extends Event {
    private static final int GENERIC_EVENT = 35;
    private final byte extensionOpcode;
    private final short deviceId;
    private final int eventType;
    private final int detail;
    private final int mode;
    private final int timestamp;
    private final Window root;
    private final Window event;
    private final Window child;
    private final short rootX;
    private final short rootY;
    private final short eventX;
    private final short eventY;
    private final boolean focus;
    private final int baseModifiers;
    private final int lockedModifiers;
    private final int effectiveModifiers;
    private final int buttonState;

    public XInputCrossingEvent(byte extensionOpcode, int deviceId,
            int eventType, int detail, int mode, Window root, Window event,
            Window child, short rootX, short rootY, short eventX, short eventY,
            boolean focus, int baseModifiers, int lockedModifiers,
            int effectiveModifiers, int buttonState) {
        super(GENERIC_EVENT);
        this.extensionOpcode = extensionOpcode;
        this.deviceId = (short)deviceId;
        this.eventType = eventType;
        this.detail = detail;
        this.mode = mode;
        this.timestamp = (int)System.currentTimeMillis();
        this.root = root;
        this.event = event;
        this.child = child;
        this.rootX = rootX;
        this.rootY = rootY;
        this.eventX = eventX;
        this.eventY = eventY;
        this.focus = focus;
        this.baseModifiers = baseModifiers;
        this.lockedModifiers = lockedModifiers;
        this.effectiveModifiers = effectiveModifiers;
        this.buttonState = buttonState;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream)
            throws IOException {
        // xXIEnterEvent is 72 bytes plus button words. xXIGroupInfo is
        // 4 CARD8s. An 88-byte encoding put the button mask 12 bytes
        // late, so XGetEventData still returned a non-NULL mask of zeros.
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte(extensionOpcode);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(11); // (76 - 32) / 4
            outputStream.writeShort((short)eventType);
            outputStream.writeShort(deviceId);
            outputStream.writeInt(timestamp);
            outputStream.writeShort(deviceId); // sourceid
            outputStream.writeByte((byte)mode);
            outputStream.writeByte((byte)detail);
            outputStream.writeInt(root.id);
            outputStream.writeInt(event.id);
            outputStream.writeInt(child != null ? child.id : 0);
            outputStream.writeInt(rootX << 16);
            outputStream.writeInt(rootY << 16);
            outputStream.writeInt(eventX << 16);
            outputStream.writeInt(eventY << 16);
            outputStream.writeByte((byte)1); // same_screen
            outputStream.writeByte((byte)(focus ? 1 : 0));
            outputStream.writeShort((short)1); // buttons_len
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
