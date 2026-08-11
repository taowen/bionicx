package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Keyboard;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;

/** Minimal read-only XKB 1.0 map for the core keyboard. */
public class XKeyboardExtension extends Extension {
    public static final int MAJOR_VERSION = 1;
    public static final int MINOR_VERSION = 0;
    private static final byte FIRST_EVENT = 90;
    private static final byte FIRST_ERROR = -102;
    private static final int CORE_KEYBOARD_ID = 3;
    private static final int XKB_KEY_TYPES_MASK = 1;
    private static final int XKB_KEY_SYMS_MASK = 2;
    private static final int REQUIRED_KEY_TYPES = 4;
    private static final int ESCAPE_KEYCODE = 9;
    private static final int XK_ESCAPE = 0xff1b;

    private static abstract class ClientOpcodes {
        private static final byte USE_EXTENSION = 0;
        private static final byte GET_MAP = 8;
    }

    public XKeyboardExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
    }

    @Override
    public String getName() {
        return "XKEYBOARD";
    }

    @Override
    public byte getFirstEventId() {
        return FIRST_EVENT;
    }

    @Override
    public byte getFirstErrorId() {
        return FIRST_ERROR;
    }

    private void useExtension(XClient client, XInputStream inputStream,
                              XOutputStream outputStream) throws IOException {
        inputStream.skip(4);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)1); // supported
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)MAJOR_VERSION);
            outputStream.writeShort((short)MINOR_VERSION);
            outputStream.writePad(20);
        }
    }

    private void getMap(XClient client, XInputStream inputStream,
                        XOutputStream outputStream) throws IOException {
        inputStream.skip(client.getRemainingRequestLength());
        int present = XKB_KEY_TYPES_MASK | XKB_KEY_SYMS_MASK;
        int variableBytes = REQUIRED_KEY_TYPES * 8 + 8 + 4;
        int replyLength = (8 + variableBytes) / 4;

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(replyLength);
            outputStream.writeShort((short)0);
            outputStream.writeByte((byte)Keyboard.MIN_KEYCODE);
            outputStream.writeByte((byte)Keyboard.MAX_KEYCODE);
            outputStream.writeShort((short)present);
            outputStream.writeByte((byte)0); // first type
            outputStream.writeByte((byte)REQUIRED_KEY_TYPES); // types in reply
            outputStream.writeByte((byte)REQUIRED_KEY_TYPES); // total types
            outputStream.writeByte((byte)ESCAPE_KEYCODE);
            outputStream.writeShort((short)1); // total keysyms
            outputStream.writeByte((byte)1); // keys with symbols
            outputStream.writeByte((byte)0); // first action key
            outputStream.writeShort((short)0); // total actions
            outputStream.writeByte((byte)0); // action keys
            outputStream.writeByte((byte)0); // first behavior key
            outputStream.writeByte((byte)0); // behavior keys
            outputStream.writeByte((byte)0); // total behaviors
            outputStream.writeByte((byte)0); // first explicit key
            outputStream.writeByte((byte)0); // explicit keys
            outputStream.writeByte((byte)0); // total explicit
            outputStream.writeByte((byte)0); // first modifier-map key
            outputStream.writeByte((byte)0); // modifier-map keys
            outputStream.writeByte((byte)0); // total modifier-map entries
            outputStream.writeByte((byte)0); // first virtual-mod-map key
            outputStream.writeByte((byte)0); // virtual-mod-map keys
            outputStream.writeByte((byte)0); // total virtual-mod-map entries
            outputStream.writeByte((byte)0);
            outputStream.writeShort((short)0); // virtual modifiers

            // Four required one-level types with no modifier map entries.
            for (int i = 0; i < REQUIRED_KEY_TYPES; i++) {
                outputStream.writeByte((byte)0); // mask
                outputStream.writeByte((byte)0); // real modifiers
                outputStream.writeShort((short)0); // virtual modifiers
                outputStream.writeByte((byte)1); // levels
                outputStream.writeByte((byte)0); // map entries
                outputStream.writeByte((byte)0); // preserve
                outputStream.writeByte((byte)0);
            }

            // One symbol-map record for keycode 9, followed by XK_Escape.
            outputStream.writePad(4); // key type indices for four groups
            outputStream.writeByte((byte)1); // one keyboard group
            outputStream.writeByte((byte)1); // width
            outputStream.writeShort((short)1); // symbols
            outputStream.writeInt(XK_ESCAPE);
        }
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        switch (client.getRequestData()) {
            case ClientOpcodes.USE_EXTENSION:
                useExtension(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_MAP:
                getMap(client, inputStream, outputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
