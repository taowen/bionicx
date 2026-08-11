package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;

import java.io.IOException;

/** XKB StateNotify for the core keyboard's real modifier state. */
public class XkbStateNotify extends Event {
    private static final int XKB_FIRST_EVENT = 90;
    private static final int XKB_STATE_NOTIFY = 2;
    private static final int CORE_KEYBOARD_ID = 3;
    private static final int CHANGED_MODIFIERS = 0x0b;

    private final int effectiveMods;
    private final int baseMods;
    private final int lockedMods;
    private final byte keycode;
    private final byte eventType;
    private final int timestamp = (int)System.currentTimeMillis();

    public XkbStateNotify(int effectiveMods, int baseMods, int lockedMods,
                          byte keycode, int eventType) {
        super(XKB_FIRST_EVENT);
        this.effectiveMods = effectiveMods;
        this.baseMods = baseMods;
        this.lockedMods = lockedMods;
        this.keycode = keycode;
        this.eventType = (byte)eventType;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream)
            throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)XKB_STATE_NOTIFY);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(timestamp);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeByte((byte)effectiveMods);
            outputStream.writeByte((byte)baseMods);
            outputStream.writeByte((byte)0); // latched modifiers
            outputStream.writeByte((byte)lockedMods);
            outputStream.writeByte((byte)0); // effective group
            outputStream.writeShort((short)0); // base group
            outputStream.writeShort((short)0); // latched group
            outputStream.writeByte((byte)0); // locked group
            outputStream.writePad(5); // compatibility/grab/lookup modifiers
            outputStream.writeShort((short)0); // pointer button state
            outputStream.writeShort((short)CHANGED_MODIFIERS);
            outputStream.writeByte(keycode);
            outputStream.writeByte(eventType);
            outputStream.writeByte((byte)0); // request major
            outputStream.writeByte((byte)0); // request minor
        }
    }
}
