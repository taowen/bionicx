package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Keyboard;
import com.winlator.xserver.Atom;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;
import java.nio.charset.StandardCharsets;

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
    private static final int XKB_COMPONENT_NAMES_MASK = 0x3f;
    private static final int XKB_KEY_TYPE_NAMES_MASK = 1 << 6;
    private static final int XKB_KEY_NAMES_MASK = 1 << 9;
    private static final int XKB_SUPPORTED_NAMES_MASK = XKB_COMPONENT_NAMES_MASK
            | XKB_KEY_TYPE_NAMES_MASK | XKB_KEY_NAMES_MASK;
    private static final String[] COMPONENT_NAMES = {
        "bionicx", "bionicx", "bionicx", "bionicx", "BIONICX", "bionicx"
    };
    private static final String[] KEY_TYPE_NAMES = {
        "ONE_LEVEL", "TWO_LEVEL", "ALPHABETIC", "KEYPAD"
    };

    private static abstract class ClientOpcodes {
        private static final byte USE_EXTENSION = 0;
        private static final byte SELECT_EVENTS = 1;
        private static final byte GET_MAP = 8;
        private static final byte GET_NAMES = 17;
        private static final byte GET_DEVICE_INFO = 24;
    }

    private void selectEvents(XClient client, XInputStream inputStream)
            throws IOException, XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        int affect = inputStream.readUnsignedShort();
        int clear = inputStream.readUnsignedShort();
        int selectAll = inputStream.readUnsignedShort();
        int affectMap = inputStream.readUnsignedShort();
        int map = inputStream.readUnsignedShort();
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
        client.updateXkbEventSelection(affect, clear, selectAll, affectMap, map);
        inputStream.skip(client.getRemainingRequestLength());
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

    private void getDeviceInfo(XClient client, XInputStream inputStream,
                               XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        int wanted = inputStream.readUnsignedShort();
        inputStream.skip(6); // allBtns, button range, pad, LED class and ID
        inputStream.skip(2);
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }

        byte[] name = "BionicX keyboard".getBytes(StandardCharsets.ISO_8859_1);
        int payloadBytes = (name.length + 2 + 3) & ~3;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(payloadBytes / 4);
            outputStream.writeShort((short)0); // no optional features present
            outputStream.writeShort((short)0); // no mutable device features
            outputStream.writeShort((short)wanted); // requested unsupported features
            outputStream.writeShort((short)0); // LED feedbacks
            outputStream.writeByte((byte)0); // first button wanted
            outputStream.writeByte((byte)0); // buttons wanted
            outputStream.writeByte((byte)0); // first button returned
            outputStream.writeByte((byte)0); // buttons returned
            outputStream.writeByte((byte)0); // total buttons
            outputStream.writeByte((byte)1); // core keyboard owns keyboard state
            outputStream.writeShort((short)0); // default keyboard feedback ID
            outputStream.writeShort((short)0); // default LED feedback ID
            outputStream.writeShort((short)0);
            outputStream.writeInt(0); // no device-type atom
            outputStream.writeShort((short)name.length);
            outputStream.write(name);
            outputStream.writePad(payloadBytes - name.length - 2);
        }
    }

    private void getNames(XClient client, XInputStream inputStream,
                          XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.skip(2);
        int requested = inputStream.readInt();
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }

        int present = requested & XKB_SUPPORTED_NAMES_MASK;
        int atomCount = 0;
        for (int bit = 0; bit < COMPONENT_NAMES.length; bit++) {
            if ((present & (1 << bit)) != 0) atomCount++;
        }
        if ((present & XKB_KEY_TYPE_NAMES_MASK) != 0) {
            atomCount += REQUIRED_KEY_TYPES;
        }
        int keyCount = (present & XKB_KEY_NAMES_MASK) != 0
                ? Keyboard.MAX_KEYCODE - Keyboard.MIN_KEYCODE + 1 : 0;
        int payloadBytes = atomCount * 4 + keyCount * 4;

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(payloadBytes / 4);
            outputStream.writeInt(present);
            outputStream.writeByte((byte)Keyboard.MIN_KEYCODE);
            outputStream.writeByte((byte)Keyboard.MAX_KEYCODE);
            outputStream.writeByte((byte)((present & XKB_KEY_TYPE_NAMES_MASK) != 0
                    ? REQUIRED_KEY_TYPES : 0));
            outputStream.writeByte((byte)0); // group-name mask
            outputStream.writeShort((short)0); // virtual-modifier mask
            outputStream.writeByte((byte)(keyCount > 0 ? Keyboard.MIN_KEYCODE : 0));
            outputStream.writeByte((byte)keyCount);
            outputStream.writeInt(0); // indicator-name mask
            outputStream.writeByte((byte)0); // radio groups
            outputStream.writeByte((byte)0); // key aliases
            outputStream.writeShort((short)0); // types with level names
            outputStream.writeInt(0);

            for (int bit = 0; bit < COMPONENT_NAMES.length; bit++) {
                if ((present & (1 << bit)) != 0) {
                    outputStream.writeInt(Atom.internAtom(COMPONENT_NAMES[bit]));
                }
            }
            if ((present & XKB_KEY_TYPE_NAMES_MASK) != 0) {
                for (String name : KEY_TYPE_NAMES) {
                    outputStream.writeInt(Atom.internAtom(name));
                }
            }
            if (keyCount > 0) {
                for (int keycode = Keyboard.MIN_KEYCODE;
                     keycode <= Keyboard.MAX_KEYCODE; keycode++) {
                    if (keycode == ESCAPE_KEYCODE) {
                        outputStream.write(new byte[]{'E', 'S', 'C', 0});
                    }
                    else {
                        outputStream.write(String.format("K%03d", keycode)
                                .getBytes(StandardCharsets.ISO_8859_1));
                    }
                }
            }
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
            case ClientOpcodes.SELECT_EVENTS:
                selectEvents(client, inputStream);
                break;
            case ClientOpcodes.GET_MAP:
                getMap(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_NAMES:
                getNames(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_DEVICE_INFO:
                getDeviceInfo(client, inputStream, outputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
