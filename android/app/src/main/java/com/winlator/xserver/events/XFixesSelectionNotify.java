package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;

import java.io.IOException;

public class XFixesSelectionNotify extends Event {
    private final byte subtype;
    private final int window;
    private final int owner;
    private final int selection;
    private final int selectionTimestamp;
    private final int timestamp = (int)System.currentTimeMillis();

    public XFixesSelectionNotify(int eventCode, int subtype, int window, int owner,
                                 int selection, int selectionTimestamp) {
        super(eventCode);
        this.subtype = (byte)subtype;
        this.window = window;
        this.owner = owner;
        this.selection = selection;
        this.selectionTimestamp = selectionTimestamp;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte(subtype);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(window);
            outputStream.writeInt(owner);
            outputStream.writeInt(selection);
            outputStream.writeInt(timestamp);
            outputStream.writeInt(selectionTimestamp);
            outputStream.writePad(8);
        }
    }
}
