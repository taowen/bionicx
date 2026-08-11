package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.IDGenerator;
import com.winlator.xserver.Atom;
import com.winlator.xserver.Window;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadAtom;
import com.winlator.xserver.errors.BadValue;
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
        private static final byte SELECT_INPUT = 4;
        private static final byte GET_SCREEN_RESOURCES = 8;
        private static final byte GET_OUTPUT_INFO = 9;
        private static final byte GET_OUTPUT_PROPERTY = 15;
        private static final byte GET_CRTC_INFO = 20;
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

    private XRequestError badOutput(int id) {
        return new XRequestError(Byte.toUnsignedInt(FIRST_ERROR), id);
    }

    private XRequestError badCrtc(int id) {
        return new XRequestError(Byte.toUnsignedInt(FIRST_ERROR) + 1, id);
    }

    private void selectInput(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        int eventMask = inputStream.readUnsignedShort();
        inputStream.skip(2);
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        // RandR 1.3 defines screen, CRTC, output and output-property masks.
        if ((eventMask & ~0x0f) != 0) throw new BadValue(eventMask);
        client.setRandrEventMask(window, eventMask);
    }

    private void getOutputInfo(XClient client, XInputStream inputStream,
                               XOutputStream outputStream)
            throws IOException, XRequestError {
        int requestedOutput = inputStream.readInt();
        inputStream.readInt(); // configuration timestamp
        if (requestedOutput != outputId) throw badOutput(requestedOutput);

        int mmWidth = Short.toUnsignedInt(xServer.screenInfo.getWidthInMillimeters());
        int mmHeight = Short.toUnsignedInt(xServer.screenInfo.getHeightInMillimeters());
        byte[] name = OUTPUT_NAME.getBytes(xServer.LATIN1_CHARSET);
        int namePadding = -name.length & 3;
        int bytesAfterHeader = 4 + 4 + 4 + name.length + namePadding;

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0); // RRSetConfigSuccess
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(bytesAfterHeader / 4);
            outputStream.writeInt(1); // topology timestamp
            outputStream.writeInt(crtcId);
            outputStream.writeInt(mmWidth);
            outputStream.writeInt(mmHeight);
            outputStream.writeByte((byte)0); // RR_Connected
            outputStream.writeByte((byte)0); // SubPixelUnknown
            outputStream.writeShort((short)1); // CRTCs
            outputStream.writeShort((short)1); // modes
            outputStream.writeShort((short)1); // preferred modes
            outputStream.writeShort((short)0); // clones
            outputStream.writeShort((short)name.length);
            outputStream.writeInt(crtcId);
            outputStream.writeInt(modeId);
            outputStream.write(name);
            if (namePadding > 0) outputStream.writePad(namePadding);
        }
    }

    private void getCrtcInfo(XClient client, XInputStream inputStream,
                             XOutputStream outputStream)
            throws IOException, XRequestError {
        int requestedCrtc = inputStream.readInt();
        inputStream.readInt(); // configuration timestamp
        if (requestedCrtc != crtcId) throw badCrtc(requestedCrtc);

        int width = Short.toUnsignedInt(xServer.screenInfo.width);
        int height = Short.toUnsignedInt(xServer.screenInfo.height);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0); // RRSetConfigSuccess
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(2); // current + possible output IDs
            outputStream.writeInt(1); // topology timestamp
            outputStream.writeShort((short)0); // x
            outputStream.writeShort((short)0); // y
            outputStream.writeShort((short)width);
            outputStream.writeShort((short)height);
            outputStream.writeInt(modeId);
            outputStream.writeShort((short)1); // RR_Rotate_0
            outputStream.writeShort((short)1); // supported rotations
            outputStream.writeShort((short)1); // outputs
            outputStream.writeShort((short)1); // possible outputs
            outputStream.writeInt(outputId);
            outputStream.writeInt(outputId);
        }
    }

    private void getOutputProperty(XClient client, XInputStream inputStream,
                                   XOutputStream outputStream)
            throws IOException, XRequestError {
        int requestedOutput = inputStream.readInt();
        int property = inputStream.readInt();
        int requestedType = inputStream.readInt();
        inputStream.skip(12); // offset, length, delete, pending, padding
        if (requestedOutput != outputId) throw badOutput(requestedOutput);
        if (!Atom.isValid(property)) throw new BadAtom(property);
        if (requestedType != 0 && !Atom.isValid(requestedType))
            throw new BadAtom(requestedType);

        // The single Android output currently publishes no RandR properties.
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0); // absent property has format 0
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(0); // property type None
            outputStream.writeInt(0); // bytes after
            outputStream.writeInt(0); // items
            outputStream.writePad(12);
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
            case ClientOpcodes.SELECT_INPUT:
                selectInput(client, inputStream);
                break;
            case ClientOpcodes.GET_SCREEN_RESOURCES:
            case ClientOpcodes.GET_SCREEN_RESOURCES_CURRENT:
                getScreenResources(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_OUTPUT_PRIMARY:
                getOutputPrimary(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_OUTPUT_INFO:
                getOutputInfo(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_CRTC_INFO:
                getCrtcInfo(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_OUTPUT_PROPERTY:
                getOutputProperty(client, inputStream, outputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
