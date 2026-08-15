package com.winlator.xserver.requests;

import static com.winlator.xserver.Keyboard.KEYSYMS_PER_KEYCODE;
import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Keyboard;
import com.winlator.xserver.XClient;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;

public abstract class KeyboardRequests {
    public static void getKeyboardMapping(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        byte firstKeycode = inputStream.readByte();
        int count = inputStream.readUnsignedByte();
        inputStream.skip(2);

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(KEYSYMS_PER_KEYCODE);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(count * KEYSYMS_PER_KEYCODE);
            outputStream.writePad(24);

            int first = (firstKeycode & 0xff) - Keyboard.MIN_KEYCODE;
            for (int key = 0; key < count; ++key) {
                int base = (first + key) * KEYSYMS_PER_KEYCODE;
                for (int level = 0; level < KEYSYMS_PER_KEYCODE; ++level)
                    outputStream.writeInt(
                            client.xServer.keyboard.keysyms[base + level]);
            }
        }
    }

    public static void changeKeyboardControl(XClient client,
                                             XInputStream inputStream)
            throws XRequestError {
        int mask = inputStream.readInt();
        Keyboard keyboard = client.xServer.keyboard;
        int keycode = -1;
        int autoRepeatMode = -1;
        if ((mask & 1) != 0) keyboard.setKeyClickPercent(inputStream.readInt());
        if ((mask & 2) != 0) keyboard.setBellPercent(inputStream.readInt());
        if ((mask & 4) != 0) keyboard.setBellPitch(inputStream.readInt());
        if ((mask & 8) != 0) keyboard.setBellDuration(inputStream.readInt());
        int led = 0;
        if ((mask & 16) != 0) led = inputStream.readInt();
        int ledMode = 0;
        if ((mask & 32) != 0) ledMode = inputStream.readInt();
        if ((mask & 16) != 0 && (mask & 32) != 0)
            keyboard.setLed(led, ledMode == 1);
        if ((mask & 64) != 0) keycode = inputStream.readInt() & 0xff;
        if ((mask & 128) != 0) autoRepeatMode = inputStream.readInt();
        if (autoRepeatMode == 0 || autoRepeatMode == 1) {
            if (keycode >= 0) keyboard.setKeyAutoRepeat(keycode, autoRepeatMode);
            else keyboard.setGlobalAutoRepeat(autoRepeatMode);
        }
        inputStream.skip(client.getRemainingRequestLength());
    }

    public static void getKeyboardControl(XClient client,
                                          XOutputStream outputStream)
            throws IOException, XRequestError {
        Keyboard keyboard = client.xServer.keyboard;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)keyboard.getGlobalAutoRepeat());
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(5);
            outputStream.writeInt(keyboard.getLedMask());
            outputStream.writeByte((byte)keyboard.getKeyClickPercent());
            outputStream.writeByte((byte)keyboard.getBellPercent());
            outputStream.writeShort((short)keyboard.getBellPitch());
            outputStream.writeShort((short)keyboard.getBellDuration());
            outputStream.writePad(2);
            outputStream.write(keyboard.getAutoRepeats());
        }
    }

    public static void getModifierMapping(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)1);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(2);
            outputStream.writePad(24);
            outputStream.writePad(8);
        }
    }
}
