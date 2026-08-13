package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Pointer;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;

/**
 * XTEST 2.2. KeePassXC auto-type calls XQueryExtension("XTEST") from
 * isAvailable() and later XTestFakeKeyEvent. Advertise the extension and
 * map FakeInput onto the existing inject helpers.
 */
public class XTestExtension extends Extension {
    public static final int MAJOR_VERSION = 2;
    public static final int MINOR_VERSION = 2;

    private static final byte GET_VERSION = 0;
    private static final byte COMPARE_CURSOR = 1;
    private static final byte FAKE_INPUT = 2;
    private static final byte GRAB_CONTROL = 3;

    private static final int KEY_PRESS = 2;
    private static final int KEY_RELEASE = 3;
    private static final int BUTTON_PRESS = 4;
    private static final int BUTTON_RELEASE = 5;
    private static final int MOTION_NOTIFY = 6;

    public XTestExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
    }

    @Override
    public String getName() {
        return "XTEST";
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        switch (client.getRequestData()) {
            case GET_VERSION:
                getVersion(client, inputStream, outputStream);
                break;
            case COMPARE_CURSOR:
                compareCursor(client, inputStream, outputStream);
                break;
            case FAKE_INPUT:
                fakeInput(inputStream);
                break;
            case GRAB_CONTROL:
                grabControl(inputStream);
                break;
            default:
                throw new BadValue(client.getRequestData() & 0xff);
        }
    }

    private void getVersion(XClient client, XInputStream inputStream,
                            XOutputStream outputStream) throws IOException {
        inputStream.skip(4);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)MAJOR_VERSION);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)MINOR_VERSION);
            outputStream.writePad(22);
        }
    }

    private void compareCursor(XClient client, XInputStream inputStream,
                               XOutputStream outputStream) throws IOException {
        inputStream.skip(8);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)1);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writePad(24);
        }
    }

    private void fakeInput(XInputStream inputStream) throws IOException {
        int type = inputStream.readUnsignedByte();
        int detail = inputStream.readUnsignedByte();
        inputStream.skip(2);
        inputStream.skip(4);
        inputStream.skip(4);
        inputStream.skip(8);
        short rootX = inputStream.readShort();
        short rootY = inputStream.readShort();
        inputStream.skip(8);

        switch (type) {
            case KEY_PRESS:
                xServer.injectRawKey((byte)detail, true);
                break;
            case KEY_RELEASE:
                xServer.injectRawKey((byte)detail, false);
                break;
            case BUTTON_PRESS:
                Pointer.Button button = buttonFromDetail(detail);
                if (button != null) xServer.injectPointerButtonPress(button);
                break;
            case BUTTON_RELEASE:
                Pointer.Button release = buttonFromDetail(detail);
                if (release != null) xServer.injectPointerButtonRelease(release);
                break;
            case MOTION_NOTIFY:
                if (detail != 0) {
                    xServer.injectPointerMoveDelta(rootX, rootY);
                } else {
                    xServer.injectPointerMove(rootX, rootY);
                }
                break;
            default:
                break;
        }
    }

    private void grabControl(XInputStream inputStream) {
        inputStream.skip(4);
    }

    private static Pointer.Button buttonFromDetail(int detail) {
        switch (detail) {
            case 1:
                return Pointer.Button.BUTTON_LEFT;
            case 2:
                return Pointer.Button.BUTTON_MIDDLE;
            case 3:
                return Pointer.Button.BUTTON_RIGHT;
            case 4:
                return Pointer.Button.BUTTON_SCROLL_UP;
            case 5:
                return Pointer.Button.BUTTON_SCROLL_DOWN;
            default:
                return null;
        }
    }
}
