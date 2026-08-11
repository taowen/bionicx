package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;

/** Minimal XI 2.0 master-device model backed by BionicX pointer and keyboard. */
public class XInputExtension extends Extension {
    public static final int MAJOR_VERSION = 2;
    public static final int MINOR_VERSION = 0;
    private static final byte FIRST_EVENT = 80;
    private static final byte FIRST_ERROR = -106;

    private static final int ALL_DEVICES = 0;
    private static final int ALL_MASTER_DEVICES = 1;
    private static final int MASTER_POINTER_ID = 2;
    private static final int MASTER_KEYBOARD_ID = 3;
    private static final int USE_MASTER_POINTER = 1;
    private static final int USE_MASTER_KEYBOARD = 2;

    private static abstract class ClientOpcodes {
        private static final byte GET_EXTENSION_VERSION = 1;
        private static final byte XI_QUERY_VERSION = 47;
        private static final byte XI_QUERY_DEVICE = 48;
    }

    public XInputExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
    }

    @Override
    public String getName() {
        return "XInputExtension";
    }

    @Override
    public byte getFirstEventId() {
        return FIRST_EVENT;
    }

    @Override
    public byte getFirstErrorId() {
        return FIRST_ERROR;
    }

    private void getExtensionVersion(XClient client, XInputStream inputStream,
                                     XOutputStream outputStream) throws IOException {
        int nameLength = inputStream.readUnsignedShort();
        inputStream.skip(2);
        inputStream.readString8(nameLength);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.GET_EXTENSION_VERSION);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)MAJOR_VERSION);
            outputStream.writeShort((short)MINOR_VERSION);
            outputStream.writeByte((byte)1);
            outputStream.writePad(19);
        }
    }

    private void queryVersion(XClient client, XInputStream inputStream,
                              XOutputStream outputStream) throws IOException {
        inputStream.skip(4);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.XI_QUERY_VERSION);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)MAJOR_VERSION);
            outputStream.writeShort((short)MINOR_VERSION);
            outputStream.writePad(20);
        }
    }

    private static int paddedLength(String name) {
        return (name.length() + 3) & ~3;
    }

    private void writeDevice(XOutputStream outputStream, int id, int use,
                             int attachment, String name) {
        outputStream.writeShort((short)id);
        outputStream.writeShort((short)use);
        outputStream.writeShort((short)attachment);
        outputStream.writeShort((short)0); // no XI classes yet
        outputStream.writeShort((short)name.length());
        outputStream.writeByte((byte)1); // enabled
        outputStream.writeByte((byte)0);
        outputStream.write(name.getBytes(xServer.LATIN1_CHARSET));
        int padding = -name.length() & 3;
        if (padding > 0) outputStream.writePad(padding);
    }

    private void queryDevice(XClient client, XInputStream inputStream,
                             XOutputStream outputStream)
            throws IOException, XRequestError {
        int selector = inputStream.readUnsignedShort();
        inputStream.skip(2);
        boolean includePointer = selector == ALL_DEVICES
                || selector == ALL_MASTER_DEVICES || selector == MASTER_POINTER_ID;
        boolean includeKeyboard = selector == ALL_DEVICES
                || selector == ALL_MASTER_DEVICES || selector == MASTER_KEYBOARD_ID;
        if (!includePointer && !includeKeyboard) throw new BadValue(selector);

        String pointerName = "BionicX pointer";
        String keyboardName = "BionicX keyboard";
        int deviceCount = (includePointer ? 1 : 0) + (includeKeyboard ? 1 : 0);
        int payloadBytes = 0;
        if (includePointer) payloadBytes += 12 + paddedLength(pointerName);
        if (includeKeyboard) payloadBytes += 12 + paddedLength(keyboardName);

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(ClientOpcodes.XI_QUERY_DEVICE);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(payloadBytes / 4);
            outputStream.writeShort((short)deviceCount);
            outputStream.writePad(22);
            if (includePointer) {
                writeDevice(outputStream, MASTER_POINTER_ID, USE_MASTER_POINTER,
                        MASTER_KEYBOARD_ID, pointerName);
            }
            if (includeKeyboard) {
                writeDevice(outputStream, MASTER_KEYBOARD_ID, USE_MASTER_KEYBOARD,
                        MASTER_POINTER_ID, keyboardName);
            }
        }
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        switch (client.getRequestData()) {
            case ClientOpcodes.GET_EXTENSION_VERSION:
                getExtensionVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.XI_QUERY_VERSION:
                queryVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.XI_QUERY_DEVICE:
                queryDevice(client, inputStream, outputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
