package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;

import java.io.IOException;

public class DamageNotify extends Event {
    private final int level;
    private final int drawable;
    private final int damage;
    private final short areaX;
    private final short areaY;
    private final int areaWidth;
    private final int areaHeight;
    private final short geometryX;
    private final short geometryY;
    private final int geometryWidth;
    private final int geometryHeight;
    private final int timestamp = (int)System.currentTimeMillis();

    public DamageNotify(int eventCode, int level, int drawable, int damage,
                        short areaX, short areaY, int areaWidth, int areaHeight,
                        short geometryX, short geometryY,
                        int geometryWidth, int geometryHeight) {
        super(eventCode);
        this.level = level;
        this.drawable = drawable;
        this.damage = damage;
        this.areaX = areaX;
        this.areaY = areaY;
        this.areaWidth = areaWidth;
        this.areaHeight = areaHeight;
        this.geometryX = geometryX;
        this.geometryY = geometryY;
        this.geometryWidth = geometryWidth;
        this.geometryHeight = geometryHeight;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream)
            throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)level);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(drawable);
            outputStream.writeInt(damage);
            outputStream.writeInt(timestamp);
            outputStream.writeShort(areaX);
            outputStream.writeShort(areaY);
            outputStream.writeShort((short)areaWidth);
            outputStream.writeShort((short)areaHeight);
            outputStream.writeShort(geometryX);
            outputStream.writeShort(geometryY);
            outputStream.writeShort((short)geometryWidth);
            outputStream.writeShort((short)geometryHeight);
        }
    }
}
