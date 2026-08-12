package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Window;

import java.io.IOException;

public class VisibilityNotify extends Event {
    public enum State {UNOBSCURED, PARTIALLY_OBSCURED, FULLY_OBSCURED}

    private final Window window;
    private final State state;

    public VisibilityNotify(Window window, State state) {
        super(15);
        this.window = window;
        this.state = state;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream)
            throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(window.id);
            outputStream.writeByte((byte)state.ordinal());
            outputStream.writePad(23);
        }
    }
}
