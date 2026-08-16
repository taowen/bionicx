package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Keyboard;
import com.winlator.xserver.Atom;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadAtom;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** Minimal XKB 1.0 map for the core keyboard. */
public class XKeyboardExtension extends Extension {
    public static final int MAJOR_VERSION = 1;
    public static final int MINOR_VERSION = 0;
    private static final byte FIRST_EVENT = 90;
    private static final byte FIRST_ERROR = -102;
    private static final int CORE_KEYBOARD_ID = 3;
    private static final int XKB_KEY_TYPES_MASK = 1;
    private static final int XKB_KEY_SYMS_MASK = 2;
    private static final int XKB_MODIFIER_MAP_MASK = 1 << 2;
    private static final int XKB_EXPLICIT_COMPONENTS_MASK = 1 << 3;
    private static final int XKB_KEY_ACTIONS_MASK = 1 << 4;
    private static final int XKB_VIRTUAL_MODS_MASK = 1 << 6;
    private static final int XKB_VIRTUAL_MOD_MAP_MASK = 1 << 7;
    private static final int XKB_XKBCOMMON_MAP_MASK = XKB_KEY_TYPES_MASK
            | XKB_KEY_SYMS_MASK | XKB_KEY_ACTIONS_MASK | XKB_VIRTUAL_MODS_MASK
            | XKB_EXPLICIT_COMPONENTS_MASK | XKB_MODIFIER_MAP_MASK
            | XKB_VIRTUAL_MOD_MAP_MASK;
    private static final int REQUIRED_KEY_TYPES = 4;
    private static final int ESCAPE_KEYCODE = 9;
    private static final int XK_ESCAPE = 0xff1b;
    private static final int XKB_COMPONENT_NAMES_MASK = 0x3f;
    private static final int XKB_KEY_TYPE_NAMES_MASK = 1 << 6;
    private static final int XKB_KT_LEVEL_NAMES_MASK = 1 << 7;
    private static final int XKB_KEY_NAMES_MASK = 1 << 9;
    private static final int XKB_VIRTUAL_MOD_NAMES_MASK = 1 << 11;
    private static final int XKB_SUPPORTED_NAMES_MASK = XKB_COMPONENT_NAMES_MASK
            | XKB_KEY_TYPE_NAMES_MASK | XKB_KT_LEVEL_NAMES_MASK
            | XKB_KEY_NAMES_MASK | XKB_VIRTUAL_MOD_NAMES_MASK;
    private static final int XKB_GBN_TYPES_MASK = 1;
    private static final int XKB_GBN_SYMBOLS_MASK = (1 << 2) | (1 << 3);
    private static final int XKB_GBN_SUPPORTED_MASK = XKB_GBN_TYPES_MASK
            | XKB_GBN_SYMBOLS_MASK;
    private static final String[] COMPONENT_NAMES = {
        "bionicx", "bionicx", "bionicx", "bionicx", "BIONICX", "bionicx"
    };
    private static final String[] KEY_TYPE_NAMES = {
        "ONE_LEVEL", "TWO_LEVEL", "ALPHABETIC", "KEYPAD"
    };
    private static final int[] KEY_TYPE_LEVEL_COUNTS = {1, 2, 2, 2};
    private static final int XKB_REPEAT_KEYS_MASK = 1;
    private int repeatDelay = 660;
    private int repeatInterval = 40;
    private int lockedMods = 0;
    private int latchedMods = 0;
    private boolean scrollLockOn = false;
    private static final int LOCK_MOD_MASK = 2;
    private static final int NUM_LOCK_MOD_MASK = 16;

    private static abstract class ClientOpcodes {
        private static final byte USE_EXTENSION = 0;
        private static final byte SELECT_EVENTS = 1;
        private static final byte BELL = 3;
        private static final byte GET_STATE = 4;
        private static final byte LATCH_LOCK_STATE = 5;
        private static final byte GET_CONTROLS = 6;
        private static final byte SET_CONTROLS = 7;
        private static final byte GET_MAP = 8;
        private static final byte GET_COMPAT_MAP = 10;
        private static final byte GET_INDICATOR_STATE = 12;
        private static final byte GET_INDICATOR_MAP = 13;
        private static final byte SET_INDICATOR_MAP = 14;
        private static final byte GET_NAMED_INDICATOR = 15;
        private static final byte SET_NAMED_INDICATOR = 16;
        private static final byte GET_NAMES = 17;
        private static final byte PER_CLIENT_FLAGS = 21;
        private static final byte GET_KBD_BY_NAME = 23;
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

    private int getTotalKeysyms() {
        int totalKeysyms = 0;
        for (int keycode = Keyboard.MIN_KEYCODE;
             keycode <= Keyboard.MAX_KEYCODE; keycode++) {
            int lower = xServer.keyboard.getKeysym(keycode, 0);
            int upper = xServer.keyboard.getKeysym(keycode, 1);
            if (lower != 0) totalKeysyms += upper != 0 && upper != lower ? 2 : 1;
        }
        return totalKeysyms;
    }

    private int countModifierActions() {
        return countModifierMapKeys();
    }

    private int countModifierMapKeys() {
        int count = 0;
        for (int keycode = Keyboard.MIN_KEYCODE;
             keycode <= Keyboard.MAX_KEYCODE; keycode++) {
            if (getModifierActionMask(keycode) != 0) count++;
        }
        return count;
    }

    private int getMapVariableBytes(int present) {
        int bytes = 0;
        if ((present & XKB_KEY_TYPES_MASK) != 0)
            bytes += 8 + 16 + 32 + 16;
        if ((present & XKB_KEY_SYMS_MASK) != 0)
            bytes += Keyboard.KEYS_COUNT * 8 + getTotalKeysyms() * 4;
        if ((present & XKB_KEY_ACTIONS_MASK) != 0)
            bytes += Keyboard.KEYS_COUNT + countModifierActions() * 8;
        if ((present & XKB_MODIFIER_MAP_MASK) != 0)
            bytes += (countModifierMapKeys() * 2 + 3) & ~3;
        return bytes;
    }

    private int getModifierActionMask(int keycode) {
        return Keyboard.getModifierFlag((byte)keycode);
    }

    private boolean isLockingModifier(int keycode) {
        return keycode == 66 || keycode == 77;
    }

    private void writeMapReply(XClient client, XOutputStream outputStream,
                               int requested) {
        int present = requested & XKB_XKBCOMMON_MAP_MASK;
        int totalKeysyms = getTotalKeysyms();
        int modifierActions = countModifierActions();
        int modifierMapKeys = countModifierMapKeys();
        int variableBytes = getMapVariableBytes(present);
        int replyLength = (8 + variableBytes) / 4;
        boolean hasTypes = (present & XKB_KEY_TYPES_MASK) != 0;
        boolean hasKeySyms = (present & XKB_KEY_SYMS_MASK) != 0;
        boolean hasActions = (present & XKB_KEY_ACTIONS_MASK) != 0;
        boolean hasModMap = (present & XKB_MODIFIER_MAP_MASK) != 0;

        outputStream.writeByte(RESPONSE_CODE_SUCCESS);
        outputStream.writeByte((byte)CORE_KEYBOARD_ID);
        outputStream.writeShort(client.getSequenceNumber());
        outputStream.writeInt(replyLength);
        outputStream.writeShort((short)0);
        outputStream.writeByte((byte)Keyboard.MIN_KEYCODE);
        outputStream.writeByte((byte)Keyboard.MAX_KEYCODE);
        outputStream.writeShort((short)present);
        outputStream.writeByte((byte)0); // first type
        outputStream.writeByte((byte)(hasTypes ? REQUIRED_KEY_TYPES : 0));
        outputStream.writeByte((byte)REQUIRED_KEY_TYPES); // total types
        outputStream.writeByte((byte)Keyboard.MIN_KEYCODE);
        outputStream.writeShort((short)(hasKeySyms ? totalKeysyms : 0));
        outputStream.writeByte((byte)(hasKeySyms ? Keyboard.KEYS_COUNT : 0));
        outputStream.writeByte((byte)Keyboard.MIN_KEYCODE); // first action key
        outputStream.writeShort((short)(hasActions ? modifierActions : 0));
        outputStream.writeByte((byte)(hasActions ? Keyboard.KEYS_COUNT : 0));
        outputStream.writeByte((byte)0); // first behavior key
        outputStream.writeByte((byte)0); // behavior keys
        outputStream.writeByte((byte)0); // total behaviors
        outputStream.writeByte((byte)0); // first explicit key
        outputStream.writeByte((byte)0); // explicit keys
        outputStream.writeByte((byte)0); // total explicit
        outputStream.writeByte((byte)(hasModMap ? Keyboard.MIN_KEYCODE : 0));
        outputStream.writeByte((byte)(hasModMap ? Keyboard.KEYS_COUNT : 0));
        outputStream.writeByte((byte)(hasModMap ? modifierMapKeys : 0));
        outputStream.writeByte((byte)0); // first virtual-mod-map key
        outputStream.writeByte((byte)0); // virtual-mod-map keys
        outputStream.writeByte((byte)0); // total virtual-mod-map entries
        outputStream.writeByte((byte)0);
        outputStream.writeShort((short)0); // virtual modifiers

        // ONE_LEVEL, TWO_LEVEL, ALPHABETIC and KEYPAD. ALPHABETIC maps
        // Shift, Lock and Shift+Lock to the conventional two levels.
        if (hasTypes) for (int i = 0; i < REQUIRED_KEY_TYPES; i++) {
            boolean twoLevels = i != 0;
            boolean alphabetic = i == 2;
            int modifierMask = alphabetic ? 3 : twoLevels ? 1 : 0;
            int entryCount = alphabetic ? 3 : twoLevels ? 1 : 0;
            outputStream.writeByte((byte)modifierMask);
            outputStream.writeByte((byte)modifierMask);
            outputStream.writeShort((short)0); // virtual modifiers
            outputStream.writeByte((byte)(twoLevels ? 2 : 1)); // levels
            outputStream.writeByte((byte)entryCount);
            outputStream.writeByte((byte)0); // preserve
            outputStream.writeByte((byte)0);
            if (twoLevels) {
                outputStream.writeByte((byte)1); // active
                outputStream.writeByte((byte)1); // Shift mask
                outputStream.writeByte((byte)1); // level 2
                outputStream.writeByte((byte)1); // real Shift modifier
                outputStream.writeShort((short)0);
                outputStream.writeShort((short)0);
            }
            if (alphabetic) {
                outputStream.writeByte((byte)1); // active
                outputStream.writeByte((byte)2); // Lock mask
                outputStream.writeByte((byte)1); // level 2
                outputStream.writeByte((byte)2); // real Lock modifier
                outputStream.writeShort((short)0);
                outputStream.writeShort((short)0);
                outputStream.writeByte((byte)1); // active
                outputStream.writeByte((byte)3); // Shift + Lock mask
                outputStream.writeByte((byte)0); // level 1
                outputStream.writeByte((byte)3);
                outputStream.writeShort((short)0);
                outputStream.writeShort((short)0);
            }
        }

        if (hasKeySyms) for (int keycode = Keyboard.MIN_KEYCODE;
             keycode <= Keyboard.MAX_KEYCODE; keycode++) {
            int lower = xServer.keyboard.getKeysym(keycode, 0);
            int upper = xServer.keyboard.getKeysym(keycode, 1);
            int symbolCount = lower == 0 ? 0
                    : upper != 0 && upper != lower ? 2 : 1;
            int typeIndex = symbolCount != 2 ? 0
                    : lower >= 'a' && lower <= 'z' ? 2 : 1;
            outputStream.writeByte((byte)typeIndex);
            outputStream.writePad(3); // unused group key-type indices
            outputStream.writeByte((byte)(symbolCount > 0 ? 1 : 0));
            outputStream.writeByte((byte)symbolCount);
            outputStream.writeShort((short)symbolCount);
            if (symbolCount > 0) outputStream.writeInt(lower);
            if (symbolCount > 1) outputStream.writeInt(upper);
        }
        if (hasActions) for (int keycode = Keyboard.MIN_KEYCODE;
             keycode <= Keyboard.MAX_KEYCODE; keycode++) {
            outputStream.writeByte((byte)(getModifierActionMask(keycode) != 0
                    ? 1 : 0));
        }
        if (hasActions) for (int keycode = Keyboard.MIN_KEYCODE;
             keycode <= Keyboard.MAX_KEYCODE; keycode++) {
            int mask = getModifierActionMask(keycode);
            if (mask == 0) continue;
            outputStream.writeByte((byte)(isLockingModifier(keycode) ? 3 : 1));
            outputStream.writeByte((byte)0); // action flags
            outputStream.writeByte((byte)mask);
            outputStream.writeByte((byte)mask);
            outputStream.writePad(4);
        }
        if (hasModMap) {
            int written = 0;
            for (int keycode = Keyboard.MIN_KEYCODE;
                 keycode <= Keyboard.MAX_KEYCODE; keycode++) {
                int mask = getModifierActionMask(keycode);
                if (mask == 0) continue;
                outputStream.writeByte((byte)keycode);
                outputStream.writeByte((byte)mask);
                written += 2;
            }
            if ((written & 3) != 0) outputStream.writePad(4 - (written & 3));
        }
    }

    private void getMap(XClient client, XInputStream inputStream,
                        XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        int full = inputStream.readUnsignedShort();
        int partial = inputStream.readUnsignedShort();
        inputStream.skip(client.getRemainingRequestLength());
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
        try (XStreamLock lock = outputStream.lock()) {
            writeMapReply(client, outputStream, full | partial);
        }
    }

    private void setControls(XClient client, XInputStream inputStream)
            throws XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.skip(26);
        int changeCtrls = inputStream.readInt();
        int delay = inputStream.readUnsignedShort();
        int interval = inputStream.readUnsignedShort();
        inputStream.skip(client.getRemainingRequestLength());
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
        // xfsettingsd applies keyboard repeat via XkbSetAutoRepeatRate.
        // Other control bits are accepted as no-ops.
        if ((changeCtrls & XKB_REPEAT_KEYS_MASK) != 0) {
            if (delay == 0 || interval == 0) throw new BadValue(0);
            repeatDelay = delay;
            repeatInterval = interval;
        }
    }

    private void getControls(XClient client, XInputStream inputStream,
                             XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.skip(client.getRemainingRequestLength());
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(15); // 92-byte reply minus the fixed 32 bytes
            outputStream.writeByte((byte)1); // default mouse-keys button
            outputStream.writeByte((byte)1); // one keyboard group
            outputStream.writeByte((byte)0); // group wrap
            outputStream.writePad(5); // internal and ignored modifiers
            outputStream.writeShort((short)0); // internal virtual modifiers
            outputStream.writeShort((short)0); // ignored virtual modifiers
            outputStream.writeShort((short)repeatDelay);
            outputStream.writeShort((short)repeatInterval);
            outputStream.writeShort((short)300); // slow-keys delay
            outputStream.writeShort((short)300); // debounce delay
            outputStream.writeShort((short)160); // mouse-keys delay
            outputStream.writeShort((short)40); // mouse-keys interval
            outputStream.writeShort((short)30); // time to maximum speed
            outputStream.writeShort((short)10); // maximum speed
            outputStream.writeShort((short)0); // acceleration curve
            outputStream.writeShort((short)0); // AccessX options
            outputStream.writeShort((short)0); // AccessX timeout
            outputStream.writeShort((short)0); // timeout option mask
            outputStream.writeShort((short)0); // timeout option values
            outputStream.writePad(2);
            outputStream.writeInt(0); // timeout controls mask
            outputStream.writeInt(0); // timeout controls values
            outputStream.writeInt(0); // enabled controls
            for (int i = 0; i < 32; i++) outputStream.writeByte((byte)0xff);
        }
    }

    private void ringBell(XClient client, XInputStream inputStream)
            throws XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.skip(client.getRemainingRequestLength());
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
    }

    private void latchLockState(XClient client, XInputStream inputStream)
            throws XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        int affectModLocks = inputStream.readUnsignedByte();
        int modLocks = inputStream.readUnsignedByte();
        inputStream.skip(2); // lockGroup, groupLock
        int affectModLatches = inputStream.readUnsignedByte();
        int modLatches = inputStream.readUnsignedByte();
        inputStream.skip(client.getRemainingRequestLength());
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
        lockedMods = (lockedMods & ~affectModLocks) | (modLocks & affectModLocks);
        latchedMods = (latchedMods & ~affectModLatches)
                | (modLatches & affectModLatches);
    }

    private void getState(XClient client, XInputStream inputStream,
                          XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.skip(client.getRemainingRequestLength());
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            int baseMods = xServer.keyboard.getBaseModifiers();
            int effectiveMods = baseMods | latchedMods | lockedMods
                    | xServer.keyboard.getLockedModifiers();
            outputStream.writeByte((byte)effectiveMods);
            outputStream.writeByte((byte)baseMods);
            outputStream.writeByte((byte)latchedMods);
            outputStream.writeByte((byte)(lockedMods
                    | xServer.keyboard.getLockedModifiers()));
            outputStream.writeByte((byte)0); // effective group
            outputStream.writeByte((byte)0); // locked group
            outputStream.writeShort((short)0); // base group
            outputStream.writeShort((short)0); // latched group
            outputStream.writePad(6); // compatibility/grab/lookup modifier state
            outputStream.writeShort((short)0); // pointer button state
            outputStream.writePad(6);
        }
    }

    private void getCompatMap(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.skip(client.getRemainingRequestLength());
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeByte((byte)0); // no compatibility groups
            outputStream.writeByte((byte)0);
            outputStream.writeShort((short)0); // first sym interpretation
            outputStream.writeShort((short)0); // returned interpretations
            outputStream.writeShort((short)0); // total interpretations
            outputStream.writePad(16);
        }
    }

    private int effectiveLockedMods() {
        return lockedMods | xServer.keyboard.getLockedModifiers();
    }

    private int namedIndicatorIndex(int atom) {
        String name = Atom.getName(atom);
        if ("Caps Lock".equals(name)) return 0;
        if ("Num Lock".equals(name)) return 1;
        if ("Scroll Lock".equals(name)) return 2;
        return -1;
    }

    private boolean namedIndicatorOn(int index) {
        int locked = effectiveLockedMods();
        switch (index) {
            case 0:
                return (locked & LOCK_MOD_MASK) != 0;
            case 1:
                return (locked & NUM_LOCK_MOD_MASK) != 0;
            case 2:
                return scrollLockOn;
            default:
                return false;
        }
    }

    private void setNamedIndicatorOn(int index, boolean on) {
        switch (index) {
            case 0:
                lockedMods = on ? (lockedMods | LOCK_MOD_MASK)
                        : (lockedMods & ~LOCK_MOD_MASK);
                break;
            case 1:
                lockedMods = on ? (lockedMods | NUM_LOCK_MOD_MASK)
                        : (lockedMods & ~NUM_LOCK_MOD_MASK);
                break;
            case 2:
                scrollLockOn = on;
                break;
            default:
                break;
        }
    }

    private int indicatorStateBits() {
        int bits = 0;
        if (namedIndicatorOn(0)) bits |= 1;
        if (namedIndicatorOn(1)) bits |= 2;
        if (namedIndicatorOn(2)) bits |= 4;
        return bits;
    }

    private void getNamedIndicator(XClient client, XInputStream inputStream,
                                   XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.skip(6); // ledClass, ledID, pad
        int indicator = inputStream.readInt();
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
        if (!Atom.isValid(indicator)) throw new BadAtom(indicator);
        int index = namedIndicatorIndex(indicator);
        boolean found = index >= 0;
        boolean on = found && namedIndicatorOn(index);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(indicator);
            outputStream.writeByte((byte)(found ? 1 : 0));
            outputStream.writeByte((byte)(on ? 1 : 0));
            outputStream.writeByte((byte)(found ? 1 : 0));
            outputStream.writeByte((byte)(found ? index : 0xff));
            outputStream.writePad(8);
            outputStream.writeInt(0);
            outputStream.writeByte((byte)1);
            outputStream.writePad(3);
        }
    }

    private void setNamedIndicator(XClient client, XInputStream inputStream)
            throws XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.skip(6); // ledClass, ledID, pad
        int indicator = inputStream.readInt();
        boolean setState = inputStream.readUnsignedByte() != 0;
        boolean on = inputStream.readUnsignedByte() != 0;
        inputStream.skip(client.getRemainingRequestLength());
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
        if (!Atom.isValid(indicator)) throw new BadAtom(indicator);
        if (setState) setNamedIndicatorOn(namedIndicatorIndex(indicator), on);
    }

    private void setIndicatorMap(XClient client, XInputStream inputStream)
            throws XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.skip(client.getRemainingRequestLength());
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
    }

    private void getIndicatorMap(XClient client, XInputStream inputStream,
                                 XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.skip(client.getRemainingRequestLength());
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(0); // no indicator maps
            outputStream.writeInt(0); // no physical indicators
            outputStream.writeByte((byte)0);
            outputStream.writePad(15);
        }
    }

    private void getIndicatorState(XClient client, XInputStream inputStream,
                                   XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.skip(2);
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(indicatorStateBits());
            outputStream.writePad(20);
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
        int levelNameCount = 0;
        if ((present & XKB_KT_LEVEL_NAMES_MASK) != 0) {
            for (int count : KEY_TYPE_LEVEL_COUNTS) levelNameCount += count;
        }
        int keyCount = (present & XKB_KEY_NAMES_MASK) != 0
                ? Keyboard.MAX_KEYCODE - Keyboard.MIN_KEYCODE + 1 : 0;
        int payloadBytes = atomCount * 4 + keyCount * 4;
        if ((present & XKB_KT_LEVEL_NAMES_MASK) != 0) {
            payloadBytes += 4 + levelNameCount * 4;
        }

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(payloadBytes / 4);
            outputStream.writeInt(present);
            outputStream.writeByte((byte)Keyboard.MIN_KEYCODE);
            outputStream.writeByte((byte)Keyboard.MAX_KEYCODE);
            outputStream.writeByte((byte)((present & (XKB_KEY_TYPE_NAMES_MASK
                    | XKB_KT_LEVEL_NAMES_MASK)) != 0
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
            if ((present & XKB_KT_LEVEL_NAMES_MASK) != 0) {
                for (int count : KEY_TYPE_LEVEL_COUNTS) {
                    outputStream.writeByte((byte)count);
                }
                for (int count : KEY_TYPE_LEVEL_COUNTS) {
                    outputStream.writeInt(Atom.internAtom("Base"));
                    if (count == 2) outputStream.writeInt(Atom.internAtom("Shift"));
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

    private void getKeyboardByName(XClient client, XInputStream inputStream,
                                   XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.readUnsignedShort(); // required components
        inputStream.readUnsignedShort(); // wanted components
        boolean load = inputStream.readUnsignedByte() != 0;
        inputStream.skip(1);
        inputStream.skip(client.getRemainingRequestLength());
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100) {
            throw new BadValue(deviceSpec);
        }

        int nestedMapBytes = 40 + getMapVariableBytes(XKB_XKBCOMMON_MAP_MASK);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(nestedMapBytes / 4);
            outputStream.writeByte((byte)Keyboard.MIN_KEYCODE);
            outputStream.writeByte((byte)Keyboard.MAX_KEYCODE);
            outputStream.writeByte((byte)(load ? 1 : 0));
            outputStream.writeByte((byte)0); // existing keyboard
            outputStream.writeShort((short)XKB_GBN_SUPPORTED_MASK);
            outputStream.writeShort((short)XKB_GBN_SUPPORTED_MASK);
            outputStream.writePad(16);
            writeMapReply(client, outputStream, XKB_XKBCOMMON_MAP_MASK);
        }
    }

    private void perClientFlags(XClient client, XInputStream inputStream,
                                XOutputStream outputStream)
            throws IOException, XRequestError {
        int deviceSpec = inputStream.readUnsignedShort();
        inputStream.skip(2);
        int change = inputStream.readInt();
        int value = inputStream.readInt();
        int controlsToChange = inputStream.readInt();
        int autoControls = inputStream.readInt();
        int autoControlValues = inputStream.readInt();
        if (deviceSpec != CORE_KEYBOARD_ID && deviceSpec != 0x100)
            throw new BadValue(deviceSpec);
        // There are no mutable per-client server features yet. Report the
        // requested flag values as disabled, while retaining the standard
        // reply shape expected by libxkbcommon/GTK.
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)CORE_KEYBOARD_ID);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(0); // supported flags
            outputStream.writeInt(0); // current flags
            outputStream.writeInt(0); // automatic controls
            outputStream.writeInt(0); // automatic control values
            outputStream.writePad(8);
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
            case ClientOpcodes.BELL:
                ringBell(client, inputStream);
                break;
            case ClientOpcodes.GET_STATE:
                getState(client, inputStream, outputStream);
                break;
            case ClientOpcodes.LATCH_LOCK_STATE:
                latchLockState(client, inputStream);
                break;
            case ClientOpcodes.GET_CONTROLS:
                getControls(client, inputStream, outputStream);
                break;
            case ClientOpcodes.SET_CONTROLS:
                setControls(client, inputStream);
                break;
            case ClientOpcodes.GET_MAP:
                getMap(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_COMPAT_MAP:
                getCompatMap(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_INDICATOR_STATE:
                getIndicatorState(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_INDICATOR_MAP:
                getIndicatorMap(client, inputStream, outputStream);
                break;
            case ClientOpcodes.SET_INDICATOR_MAP:
                setIndicatorMap(client, inputStream);
                break;
            case ClientOpcodes.GET_NAMED_INDICATOR:
                getNamedIndicator(client, inputStream, outputStream);
                break;
            case ClientOpcodes.SET_NAMED_INDICATOR:
                setNamedIndicator(client, inputStream);
                break;
            case ClientOpcodes.GET_NAMES:
                getNames(client, inputStream, outputStream);
                break;
            case ClientOpcodes.PER_CLIENT_FLAGS:
                perClientFlags(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_KBD_BY_NAME:
                getKeyboardByName(client, inputStream, outputStream);
                break;
            case ClientOpcodes.GET_DEVICE_INFO:
                getDeviceInfo(client, inputStream, outputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
