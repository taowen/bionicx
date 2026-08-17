package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.extensions.MITSHMExtension;

import java.io.IOException;

public class ShmCompletion extends Event {
    private final int drawableId;
    private final byte majorOpcode;
    private final int shmseg;
    private final int offset;

    public ShmCompletion(MITSHMExtension extension, int drawableId, int shmseg,
                         int offset) {
        super(Byte.toUnsignedInt(extension.getFirstEventId()));
        this.drawableId = drawableId;
        this.majorOpcode = extension.getMajorOpcode();
        this.shmseg = shmseg;
        this.offset = offset;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(drawableId);
            outputStream.writeShort((short)MITSHMExtension.PUT_IMAGE_MINOR);
            outputStream.writeByte(majorOpcode);
            outputStream.writeByte((byte)0);
            outputStream.writeInt(shmseg);
            outputStream.writeInt(offset);
            // Core X events are 32 bytes. 20 used + 12 pad.
            outputStream.writePad(12);
        }
    }
}
