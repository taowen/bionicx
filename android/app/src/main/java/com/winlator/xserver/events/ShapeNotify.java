package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;

import java.io.IOException;

public class ShapeNotify extends Event {
    private final int kind;
    private final int window;
    private final int x;
    private final int y;
    private final int width;
    private final int height;
    private final int timestamp = (int)System.currentTimeMillis();
    private final boolean shaped;

    public ShapeNotify(int eventCode, int kind, int window, int x, int y,
                       int width, int height, boolean shaped) {
        super(eventCode);
        this.kind = kind;
        this.window = window;
        this.x = x;
        this.y = y;
        this.width = width;
        this.height = height;
        this.shaped = shaped;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream)
            throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)kind);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(window);
            outputStream.writeShort((short)x);
            outputStream.writeShort((short)y);
            outputStream.writeShort((short)width);
            outputStream.writeShort((short)height);
            outputStream.writeInt(timestamp);
            outputStream.writeByte((byte)(shaped ? 1 : 0));
            outputStream.writePad(11);
        }
    }
}
