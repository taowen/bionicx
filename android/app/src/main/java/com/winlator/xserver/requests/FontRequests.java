package com.winlator.xserver.requests;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadFont;
import com.winlator.xserver.errors.BadIdChoice;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Core-font protocol backed by one deterministic virtual fixed-width face. */
public abstract class FontRequests {
    private static final short GLYPH_WIDTH = 8;
    private static final short FONT_ASCENT = 11;
    private static final short FONT_DESCENT = 3;
    private static final String[] FONT_NAMES = {
            "fixed", "6x13", "8x13", "9x15", "10x20", "nil2", "cursor"
    };

    public static void openFont(XClient client, XInputStream inputStream,
                                XOutputStream outputStream)
            throws XRequestError {
        int fontId = inputStream.readInt();
        int length = inputStream.readUnsignedShort();
        inputStream.skip(2);
        String name = inputStream.readString8(length);
        if (!client.isValidResourceId(fontId)
                || !client.openFont(fontId, name))
            throw new BadIdChoice(fontId);
    }

    public static void closeFont(XClient client, XInputStream inputStream,
                                 XOutputStream outputStream)
            throws XRequestError {
        int fontId = inputStream.readInt();
        if (!client.closeFont(fontId)) throw new BadFont(fontId);
    }

    private static void writeCharInfo(XOutputStream outputStream) {
        outputStream.writeShort((short)0);       // left side bearing
        outputStream.writeShort(GLYPH_WIDTH);    // right side bearing
        outputStream.writeShort(GLYPH_WIDTH);    // character width
        outputStream.writeShort(FONT_ASCENT);
        outputStream.writeShort(FONT_DESCENT);
        outputStream.writeShort((short)0);       // attributes
    }

    public static void queryFont(XClient client, XInputStream inputStream,
                                 XOutputStream outputStream)
            throws IOException, XRequestError {
        int fontId = inputStream.readInt();
        if (!client.hasOpenFont(fontId)) throw new BadFont(fontId);

        // A fixed-width font may omit per-character metrics when min/max
        // bounds apply to every character. The 60-byte reply therefore has
        // seven 4-byte units following the protocol's first 32 bytes.
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(7);
            writeCharInfo(outputStream);          // min bounds
            outputStream.writePad(4);
            writeCharInfo(outputStream);          // max bounds
            outputStream.writePad(4);
            outputStream.writeShort((short)0);    // min char/byte2
            outputStream.writeShort((short)255);  // max char/byte2
            outputStream.writeShort((short)32);   // default char
            outputStream.writeShort((short)0);    // properties
            outputStream.writeByte((byte)0);      // left-to-right
            outputStream.writeByte((byte)0);      // min byte1
            outputStream.writeByte((byte)0);      // max byte1
            outputStream.writeByte((byte)1);      // all chars exist
            outputStream.writeShort(FONT_ASCENT);
            outputStream.writeShort(FONT_DESCENT);
            outputStream.writeInt(0);             // char infos
        }
    }

    public static void queryTextExtents(XClient client,
                                        XInputStream inputStream,
                                        XOutputStream outputStream)
            throws IOException, XRequestError {
        int fontId = inputStream.readInt();
        if (!client.hasOpenFont(fontId)) throw new BadFont(fontId);
        int remaining = client.getRemainingRequestLength();
        int padding = client.getRequestData() != 0 ? 2 : 0;
        int characterCount = Math.max(0, remaining - padding) / 2;
        inputStream.skip(remaining);
        int width = characterCount * GLYPH_WIDTH;

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);      // left-to-right
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort(FONT_ASCENT);
            outputStream.writeShort(FONT_DESCENT);
            outputStream.writeShort(FONT_ASCENT);
            outputStream.writeShort(FONT_DESCENT);
            outputStream.writeInt(width);
            outputStream.writeInt(0);             // overall left
            outputStream.writeInt(width);         // overall right
            outputStream.writePad(4);
        }
    }

    private static boolean globMatches(String pattern, String value) {
        int patternIndex = 0;
        int valueIndex = 0;
        int starIndex = -1;
        int retryValue = -1;
        pattern = pattern.toLowerCase(Locale.ROOT);
        value = value.toLowerCase(Locale.ROOT);
        while (valueIndex < value.length()) {
            if (patternIndex < pattern.length()
                    && (pattern.charAt(patternIndex) == '?'
                    || pattern.charAt(patternIndex) == value.charAt(valueIndex))) {
                ++patternIndex;
                ++valueIndex;
            }
            else if (patternIndex < pattern.length()
                    && pattern.charAt(patternIndex) == '*') {
                starIndex = patternIndex++;
                retryValue = valueIndex;
            }
            else if (starIndex >= 0) {
                patternIndex = starIndex + 1;
                valueIndex = ++retryValue;
            }
            else return false;
        }
        while (patternIndex < pattern.length()
                && pattern.charAt(patternIndex) == '*') ++patternIndex;
        return patternIndex == pattern.length();
    }

    public static void listFonts(XClient client, XInputStream inputStream,
                                 XOutputStream outputStream)
            throws IOException {
        int maximumNames = inputStream.readUnsignedShort();
        int patternLength = inputStream.readUnsignedShort();
        String pattern = inputStream.readString8(patternLength);
        List<String> matches = new ArrayList<>();
        for (String name : FONT_NAMES) {
            if (matches.size() >= maximumNames) break;
            if (globMatches(pattern, name)) matches.add(name);
        }
        int dataLength = 0;
        for (String name : matches) dataLength += 1 + name.length();
        int padding = -dataLength & 3;

        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt((dataLength + padding) / 4);
            outputStream.writeShort((short)matches.size());
            outputStream.writePad(22);
            for (String name : matches) {
                byte[] encoded = name.getBytes(XServer.LATIN1_CHARSET);
                outputStream.writeByte((byte)encoded.length);
                outputStream.write(encoded);
            }
            outputStream.writePad(padding);
        }
    }
}
