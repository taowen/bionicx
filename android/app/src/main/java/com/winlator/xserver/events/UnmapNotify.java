package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Window;

import java.io.IOException;

public class UnmapNotify extends Event {
    private final Window event;
    private final Window window;
    private final boolean fromConfigure;

    public UnmapNotify(Window event, Window window) {
        this(event, window, false);
    }

    public UnmapNotify(Window event, Window window, boolean fromConfigure) {
        super(18);
        this.event = event;
        this.window = window;
        this.fromConfigure = fromConfigure;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(event.id);
            outputStream.writeInt(window.id);
            outputStream.writeByte((byte)(fromConfigure ? 1 : 0));
            outputStream.writePad(19);
        }
    }
}
