package com.winlator.xserver.requests;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Pointer;
import com.winlator.xserver.XClient;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;

public abstract class PointerRequests {
    public static void changePointerControl(XClient client,
                                            XInputStream inputStream)
            throws XRequestError {
        int numerator = inputStream.readShort();
        int denominator = inputStream.readShort();
        int threshold = inputStream.readShort();
        boolean doAccel = inputStream.readByte() != 0;
        boolean doThreshold = inputStream.readByte() != 0;
        if (doAccel && denominator == 0) throw new BadValue(0);
        Pointer pointer = client.xServer.pointer;
        pointer.setAccel(
                doAccel ? numerator : pointer.getAccelNumerator(),
                doAccel ? denominator : pointer.getAccelDenominator(),
                doThreshold ? threshold : pointer.getAccelThreshold());
    }

    public static void getPointerControl(XClient client,
                                         XOutputStream outputStream)
            throws IOException, XRequestError {
        Pointer pointer = client.xServer.pointer;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)pointer.getAccelNumerator());
            outputStream.writeShort((short)pointer.getAccelDenominator());
            outputStream.writeShort((short)pointer.getAccelThreshold());
            outputStream.writePad(18);
        }
    }
}
