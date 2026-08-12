package com.winlator.xserver.requests;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.XClient;

import java.io.IOException;

/** TrueColor requests for the root visual's immutable RGB colormap. */
public abstract class ColorRequests {
    private static int pixel(int red, int green, int blue) {
        return ((red >>> 8) << 16) | ((green >>> 8) << 8) | (blue >>> 8);
    }

    private static short expand(int component) {
        return (short)(component * 257);
    }

    public static void allocColor(XClient client, XInputStream inputStream,
                                  XOutputStream outputStream)
            throws IOException {
        inputStream.readInt(); // root TrueColor colormap (advertised as id 0)
        int requestedRed = inputStream.readUnsignedShort();
        int requestedGreen = inputStream.readUnsignedShort();
        int requestedBlue = inputStream.readUnsignedShort();
        inputStream.skip(2);
        int pixel = pixel(requestedRed, requestedGreen, requestedBlue);
        short red = expand((pixel >>> 16) & 0xff);
        short green = expand((pixel >>> 8) & 0xff);
        short blue = expand(pixel & 0xff);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort(red);
            outputStream.writeShort(green);
            outputStream.writeShort(blue);
            outputStream.writePad(2);
            outputStream.writeInt(pixel);
            outputStream.writePad(12);
        }
    }

    public static void queryColors(XClient client, XInputStream inputStream,
                                   XOutputStream outputStream)
            throws IOException {
        inputStream.readInt(); // root TrueColor colormap
        int count = client.getRemainingRequestLength() / 4;
        int[] pixels = new int[count];
        for (int index = 0; index < count; ++index)
            pixels[index] = inputStream.readInt();
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(count * 2);
            outputStream.writeShort((short)count);
            outputStream.writePad(22);
            for (int pixel : pixels) {
                outputStream.writeShort(expand((pixel >>> 16) & 0xff));
                outputStream.writeShort(expand((pixel >>> 8) & 0xff));
                outputStream.writeShort(expand(pixel & 0xff));
                outputStream.writePad(2);
            }
        }
    }
}
