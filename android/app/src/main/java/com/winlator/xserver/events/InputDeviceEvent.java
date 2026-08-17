package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.core.Bitmask;
import com.winlator.xserver.Window;

import java.io.IOException;

public class InputDeviceEvent extends Event {
    private final byte detail;
    private final int timestamp;
    private final Window root;
    private final Window event;
    private final Window child;
    private final short eventX;
    private final short eventY;
    private final short rootX;
    private final short rootY;
    private final Bitmask state;
    private boolean sendEvent;

    public InputDeviceEvent(int code, byte detail, Window root, Window event, Window child, short rootX, short rootY, short eventX, short eventY, Bitmask state) {
        super(code);
        this.detail = detail;
        this.timestamp = (int)System.currentTimeMillis();
        this.root = root;
        this.event = event;
        this.child = child;
        this.rootX = rootX;
        this.rootY = rootY;
        this.eventX = eventX;
        this.eventY = eventY;
        this.state = state;
    }

    public void setSendEvent(boolean sendEvent) {
        this.sendEvent = sendEvent;
    }

    public boolean isSendEvent() {
        return sendEvent;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            // X protocol: bit 7 of the event code marks SendEvent.
            // GDK's XI2 translator ignores real core buttons and only
            // accepts send_event ones; a sync-grab filter that missed
            // the XI2 cookie can still thaw on that core press.
            outputStream.writeByte((byte)(code | (sendEvent ? 0x80 : 0)));
            outputStream.writeByte(detail);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(timestamp);
            outputStream.writeInt(root.id);
            outputStream.writeInt(event.id);
            outputStream.writeInt(child != null ? child.id : 0);
            outputStream.writeShort(rootX);
            outputStream.writeShort(rootY);
            outputStream.writeShort(eventX);
            outputStream.writeShort(eventY);
            outputStream.writeShort((short)state.getBits());
            outputStream.writeByte((byte)1);
            outputStream.writeByte((byte)0);
        }
    }
}
