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

/** RandR 1.5: GetMonitors plus the GTK init_randr15 follow-up requests. */
public class XRandRExtension extends Extension {
    public static final int MAJOR_VERSION = 1;
    public static final int MINOR_VERSION = 5;
    private static final byte FIRST_EVENT = 72;
    private static final byte FIRST_ERROR = -116;
    private static final String OUTPUT_NAME = "BionicX-0";

    private static final int GAMMA_SIZE = 256;
    private final int crtcId = IDGenerator.generate();
    private final int outputId = IDGenerator.generate();
    private final int modeId = IDGenerator.generate();
    private int primaryOutput;
    private final int[] gammaRed = new int[GAMMA_SIZE];
    private final int[] gammaGreen = new int[GAMMA_SIZE];
    private final int[] gammaBlue = new int[GAMMA_SIZE];

    private static abstract class ClientOpcodes {
        private static final byte QUERY_VERSION = 0;
        private static final byte SELECT_INPUT = 4;
        private static final byte GET_SCREEN_SIZE_RANGE = 6;
        private static final byte SET_SCREEN_SIZE = 7;
        private static final byte GET_SCREEN_RESOURCES = 8;
        private static final byte GET_OUTPUT_INFO = 9;
        private static final byte LIST_OUTPUT_PROPERTIES = 10;
        private static final byte GET_OUTPUT_PROPERTY = 15;
        private static final byte GET_CRTC_INFO = 20;
        private static final byte SET_CRTC_CONFIG = 21;
        private static final byte GET_CRTC_GAMMA_SIZE = 22;
        private static final byte GET_CRTC_GAMMA = 23;
        private static final byte SET_CRTC_GAMMA = 24;
        private static final byte GET_SCREEN_RESOURCES_CURRENT = 25;
        private static final byte SET_OUTPUT_PRIMARY = 30;
        private static final byte GET_OUTPUT_PRIMARY = 31;
        private static final byte GET_PROVIDERS = 32;
        private static final byte GET_MONITORS = 42;
    }

    public XRandRExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
        primaryOutput = outputId;
        for (int i = 0; i < GAMMA_SIZE; i++) {
            int value = i * 257;
            gammaRed[i] = value;
            gammaGreen[i] = value;
            gammaBlue[i] = value;
        }
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
            outputStream.writeInt(primaryOutput);
            outputStream.writePad(20);
        }
    }

    private void setOutputPrimary(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        int output = inputStream.readInt();
        if (xServer.windowManager.getWindow(windowId) == null)
            throw new BadWindow(windowId);
        if (output != 0 && output != outputId) throw badOutput(output);
        primaryOutput = output;
    }

    private void getScreenSizeRange(XClient client, XInputStream inputStream,
                                    XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        if (xServer.windowManager.getWindow(windowId) == null)
            throw new BadWindow(windowId);
        int width = Short.toUnsignedInt(xServer.screenInfo.width);
        int height = Short.toUnsignedInt(xServer.screenInfo.height);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)width);
            outputStream.writeShort((short)height);
            outputStream.writeShort((short)width);
            outputStream.writeShort((short)height);
            outputStream.writePad(16);
        }
    }

    private void setScreenSize(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        inputStream.skip(12);
        if (xServer.windowManager.getWindow(windowId) == null)
            throw new BadWindow(windowId);
    }

    private void setCrtcConfig(XClient client, XInputStream inputStream,
                               XOutputStream outputStream)
            throws IOException, XRequestError {
        int crtc = inputStream.readInt();
        inputStream.skip(20);
        inputStream.skip(client.getRemainingRequestLength());
        if (crtc != crtcId) throw badCrtc(crtc);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(1);
            outputStream.writePad(20);
        }
    }

    private void getCrtcGammaSize(XClient client, XInputStream inputStream,
                                  XOutputStream outputStream)
            throws IOException, XRequestError {
        int crtc = inputStream.readInt();
        if (crtc != crtcId) throw badCrtc(crtc);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)GAMMA_SIZE);
            outputStream.writePad(22);
        }
    }

    private void writeGamma(XOutputStream outputStream, int[] ramp) {
        for (int value : ramp) outputStream.writeShort((short)value);
    }

    private void getCrtcGamma(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        int crtc = inputStream.readInt();
        if (crtc != crtcId) throw badCrtc(crtc);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(GAMMA_SIZE * 6 / 4);
            outputStream.writeShort((short)GAMMA_SIZE);
            outputStream.writePad(22);
            writeGamma(outputStream, gammaRed);
            writeGamma(outputStream, gammaGreen);
            writeGamma(outputStream, gammaBlue);
        }
    }

    private void readGamma(XInputStream inputStream, int[] ramp, int size) {
        for (int i = 0; i < size && i < ramp.length; i++)
            ramp[i] = inputStream.readUnsignedShort();
    }

    private void setCrtcGamma(XClient client, XInputStream inputStream)
            throws XRequestError {
        int crtc = inputStream.readInt();
        int size = inputStream.readUnsignedShort();
        inputStream.skip(2);
        if (crtc != crtcId) throw badCrtc(crtc);
        if (size <= 0 || size > GAMMA_SIZE) throw new BadValue(size);
        readGamma(inputStream, gammaRed, size);
        readGamma(inputStream, gammaGreen, size);
        readGamma(inputStream, gammaBlue, size);
        inputStream.skip(client.getRemainingRequestLength());
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
        // RandR 1.5 adds provider, resource, and lease notify bits.
        if ((eventMask & ~0xff) != 0) throw new BadValue(eventMask);
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

    private void listOutputProperties(XClient client, XInputStream inputStream,
                                      XOutputStream outputStream)
            throws IOException, XRequestError {
        int requestedOutput = inputStream.readInt();
        if (requestedOutput != outputId) throw badOutput(requestedOutput);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)0); // nAtoms
            outputStream.writePad(22);
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

    private void getProviders(XClient client, XInputStream inputStream,
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
            outputStream.writeInt(1); // timestamp
            outputStream.writeShort((short)0); // nProviders
            outputStream.writePad(18);
        }
    }

    private void getMonitors(XClient client, XInputStream inputStream,
                             XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        inputStream.skip(4); // get_active + pad
        if (xServer.windowManager.getWindow(windowId) == null)
            throw new BadWindow(windowId);

        int width = Short.toUnsignedInt(xServer.screenInfo.width);
        int height = Short.toUnsignedInt(xServer.screenInfo.height);
        int mmWidth = Short.toUnsignedInt(xServer.screenInfo.getWidthInMillimeters());
        int mmHeight = Short.toUnsignedInt(xServer.screenInfo.getHeightInMillimeters());
        int nameAtom = Atom.internAtom(OUTPUT_NAME);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(7);
            outputStream.writeInt(1); // timestamp
            outputStream.writeInt(1); // nmonitors
            outputStream.writeInt(1); // noutputs
            outputStream.writePad(12);
            outputStream.writeInt(nameAtom);
            outputStream.writeByte((byte)1); // primary
            outputStream.writeByte((byte)1); // automatic
            outputStream.writeShort((short)1); // noutput
            outputStream.writeShort((short)0); // x
            outputStream.writeShort((short)0); // y
            outputStream.writeShort((short)width);
            outputStream.writeShort((short)height);
            outputStream.writeInt(mmWidth);
            outputStream.writeInt(mmHeight);
            outputStream.writeInt(outputId);
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
            case ClientOpcodes.GET_SCREEN_SIZE_RANGE:
                getScreenSizeRange(client, inputStream, outputStream);
                break;
            case ClientOpcodes.SET_SCREEN_SIZE:
                setScreenSize(client, inputStream);
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
            case ClientOpcodes.SET_CRTC_CONFIG:
                setCrtcConfig(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_CRTC_GAMMA_SIZE:
                getCrtcGammaSize(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_CRTC_GAMMA:
                getCrtcGamma(client, inputStream, outputStream);
                break;
            case ClientOpcodes.SET_CRTC_GAMMA:
                setCrtcGamma(client, inputStream);
                break;
            case ClientOpcodes.SET_OUTPUT_PRIMARY:
                setOutputPrimary(client, inputStream);
                break;
            case ClientOpcodes.GET_OUTPUT_PROPERTY:
                getOutputProperty(client, inputStream, outputStream);
                break;
            case ClientOpcodes.LIST_OUTPUT_PROPERTIES:
                listOutputProperties(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_PROVIDERS:
                getProviders(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_MONITORS:
                getMonitors(client, inputStream, outputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
