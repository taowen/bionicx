package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.IDGenerator;
import com.winlator.xserver.Window;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadWindow;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;

/** Read-only RandR 1.3 model of BionicX's single Android display. */
public class XRandRExtension extends Extension {
    public static final int MAJOR_VERSION = 1;
    public static final int MINOR_VERSION = 3;
    private static final byte FIRST_EVENT = 72;
    private static final byte FIRST_ERROR = -116;
    private static final String OUTPUT_NAME = "BionicX-0";

    private final int crtcId = IDGenerator.generate();
    private final int outputId = IDGenerator.generate();
    private final int modeId = IDGenerator.generate();

    private static abstract class ClientOpcodes {
        private static final byte QUERY_VERSION = 0;
        private static final byte GET_SCREEN_RESOURCES = 8;
        private static final byte GET_SCREEN_RESOURCES_CURRENT = 25;
        private static final byte GET_OUTPUT_PRIMARY = 31;
    }

    public XRandRExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
    }

    @Override
    public String getName() {
        return "RANDR";
    }

    @Override
    public byte getFirstEventId() {
        return FIRST_EVENT;
    }

    @Override
    public byte getFirstErrorId() {
        return FIRST_ERROR;
    }

    private void queryVersion(XClient client, XInputStream inputStream,
                              XOutputStream outputStream) throws IOException {
        inputStream.skip(8);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(MAJOR_VERSION);
            outputStream.writeInt(MINOR_VERSION);
            outputStream.writePad(16);
        }
    }

    private void getScreenResources(XClient client, XInputStream inputStream,
                                    XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);

        int width = Short.toUnsignedInt(xServer.screenInfo.width);
        int height = Short.toUnsignedInt(xServer.screenInfo.height);
        byte[] name = OUTPUT_NAME.getBytes(xServer.LATIN1_CHARSET);
        int namePadding = -name.length & 3;
        int payloadBytes = 4 + 4 + 32 + name.length + namePadding;
        long dotClock = Math.min(Integer.MAX_VALUE, (long)width * height * 60);

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(payloadBytes / 4);
            outputStream.writeInt(1); // topology timestamp
            outputStream.writeInt(1); // configuration timestamp
            outputStream.writeShort((short)1); // CRTCs
            outputStream.writeShort((short)1); // outputs
            outputStream.writeShort((short)1); // modes
            outputStream.writeShort((short)name.length);
            outputStream.writePad(8);

            outputStream.writeInt(crtcId);
            outputStream.writeInt(outputId);

            outputStream.writeInt(modeId);
            outputStream.writeShort((short)width);
            outputStream.writeShort((short)height);
            outputStream.writeInt((int)dotClock);
            outputStream.writeShort((short)width); // hSyncStart
            outputStream.writeShort((short)width); // hSyncEnd
            outputStream.writeShort((short)width); // hTotal
            outputStream.writeShort((short)0);     // hSkew
            outputStream.writeShort((short)height); // vSyncStart
            outputStream.writeShort((short)height); // vSyncEnd
            outputStream.writeShort((short)height); // vTotal
            outputStream.writeShort((short)name.length);
            outputStream.writeInt(0); // mode flags

            outputStream.write(name);
            if (namePadding > 0) outputStream.writePad(namePadding);
        }
    }

    private void getOutputPrimary(XClient client, XInputStream inputStream,
                                  XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        if (xServer.windowManager.getWindow(windowId) == null)
            throw new BadWindow(windowId);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(outputId);
            outputStream.writePad(20);
        }
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        switch (client.getRequestData()) {
            case ClientOpcodes.QUERY_VERSION:
                queryVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_SCREEN_RESOURCES:
            case ClientOpcodes.GET_SCREEN_RESOURCES_CURRENT:
                getScreenResources(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_OUTPUT_PRIMARY:
                getOutputPrimary(client, inputStream, outputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
