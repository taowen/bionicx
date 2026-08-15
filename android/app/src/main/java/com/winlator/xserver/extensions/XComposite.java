package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.renderer.Texture;
import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Drawable;
import com.winlator.xserver.Pixmap;
import com.winlator.xserver.Window;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XLock;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadAccess;
import com.winlator.xserver.errors.BadIdChoice;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadMatch;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.BadWindow;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;

public class XComposite extends Extension {
    public static final byte MAJOR_VERSION = 0;
    public static final byte MINOR_VERSION = 2;

    public enum UpdateMode {REDIRECT_AUTOMATIC, REDIRECT_MANUAL}

    private static abstract class ClientOpcodes {
        private static final byte QUERY_VERSION = 0;
        private static final byte REDIRECT_WINDOW = 1;
        private static final byte REDIRECT_SUBWINDOWS = 2;
        private static final byte UNREDIRECT_WINDOW = 3;
        private static final byte UNREDIRECT_SUBWINDOWS = 4;
        private static final byte NAME_WINDOW_PIXMAP = 6;
    }

    public XComposite(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
    }

    @Override
    public String getName() {
        return "Composite";
    }

    private void setWindowsToOffscreenStorage(Window window, boolean offscreenStorage) {
        if (!window.attributes.isMapped()) return;
        window.getContent().setOffscreenStorage(offscreenStorage);

        for (Window child : window.getChildren()) {
            setWindowsToOffscreenStorage(child, offscreenStorage);
        }
    }

    private void queryVersion(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
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

    private void redirectWindow(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        byte updateMode = inputStream.readByte();
        inputStream.skip(3);

        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);

        if (window == xServer.windowManager.rootWindow) throw new BadMatch();
        if (window.getTag("compositeRedirectParent") != null) throw new BadAccess();

        Window parent = window.getParent();
        boolean forceRedirectAutomatic = window.isSurface() && window.getWidth() == parent.getWidth() && window.getHeight() == parent.getHeight();
        if (forceRedirectAutomatic) updateMode = (byte)UpdateMode.REDIRECT_AUTOMATIC.ordinal();

        window.setTag("compositeRedirectParent", parent);
        setWindowsToOffscreenStorage(window, true);
        parent.attributes.setRenderSubwindows(false);
        xServer.windowManager.triggerOnChangeWindowZOrder(window);

        if (updateMode == UpdateMode.REDIRECT_AUTOMATIC.ordinal()) {
            Drawable parentContent = parent.getContent();
            final Texture texture = parentContent.getTexture();
            if (texture != null) xServer.getRenderer().xServerView.queueEvent(texture::destroy);
            parentContent.setTexture(window.getContent().getTexture());
        }
    }

    private void redirectSubwindows(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        byte updateMode = inputStream.readByte();
        inputStream.skip(3);
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        if (updateMode != UpdateMode.REDIRECT_AUTOMATIC.ordinal()
                && updateMode != UpdateMode.REDIRECT_MANUAL.ordinal()) {
            throw new BadValue(updateMode);
        }
        // Accept without taking children offscreen so a WM can keep
        // painting. NameWindowPixmap still treats those children as
        // redirected.
        window.setTag("compositeRedirectSubwindows", Byte.valueOf(updateMode));
    }

    private boolean isRedirected(Window window) {
        if (window.getTag("compositeRedirectParent") != null) return true;
        Window parent = window.getParent();
        while (parent != null) {
            if (parent.getTag("compositeRedirectSubwindows") != null)
                return true;
            parent = parent.getParent();
        }
        return false;
    }

    private void nameWindowPixmap(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        int pixmapId = inputStream.readInt();
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        if (!window.isInputOutput() || !isRedirected(window))
            throw new BadMatch();
        if (!client.isValidResourceId(pixmapId)) throw new BadIdChoice(pixmapId);
        Drawable content = window.getContent();
        Drawable backing = xServer.drawableManager.createDrawable(
                pixmapId, content.width, content.height, content.visual);
        if (backing == null) throw new BadIdChoice(pixmapId);
        backing.setData(content.getData());
        backing.setUseSharedData(true);
        Pixmap pixmap = xServer.pixmapManager.createPixmap(backing);
        if (pixmap == null) throw new BadIdChoice(pixmapId);
        client.registerAsOwnerOfResource(pixmap);
    }

    private void unredirectSubwindows(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        inputStream.skip(4);
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        window.removeTag("compositeRedirectSubwindows");
    }

    private void unredirectWindow(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        byte updateMode = inputStream.readByte();
        inputStream.skip(3);

        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);

        if (window == xServer.windowManager.rootWindow) throw new BadMatch();
        Window oldParent = (Window)window.getTag("compositeRedirectParent");
        if (oldParent == null) throw new BadValue(windowId);

        boolean forceRedirectAutomatic = window.isSurface() && window.getWidth() == oldParent.getWidth() && window.getHeight() == oldParent.getHeight();
        if (forceRedirectAutomatic) updateMode = (byte)UpdateMode.REDIRECT_AUTOMATIC.ordinal();

        window.removeTag("compositeRedirectParent");
        setWindowsToOffscreenStorage(window, false);
        oldParent.attributes.setRenderSubwindows(true);
        xServer.windowManager.triggerOnChangeWindowZOrder(window);

        if (updateMode == UpdateMode.REDIRECT_AUTOMATIC.ordinal()) {
            Drawable parentContent = oldParent.getContent();
            parentContent.setTexture(new Texture(parentContent));
        }
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        int opcode = client.getRequestData();

        switch (opcode) {
            case ClientOpcodes.QUERY_VERSION :
                queryVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.REDIRECT_WINDOW :
                try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER, XServer.Lockable.DRAWABLE_MANAGER)) {
                    redirectWindow(client, inputStream, outputStream);
                }
                break;
            case ClientOpcodes.REDIRECT_SUBWINDOWS:
                try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER)) {
                    redirectSubwindows(client, inputStream);
                }
                break;
            case ClientOpcodes.UNREDIRECT_WINDOW:
                try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER, XServer.Lockable.DRAWABLE_MANAGER)) {
                    unredirectWindow(client, inputStream, outputStream);
                }
                break;
            case ClientOpcodes.UNREDIRECT_SUBWINDOWS:
                try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER)) {
                    unredirectSubwindows(client, inputStream);
                }
                break;
            case ClientOpcodes.NAME_WINDOW_PIXMAP:
                try (XLock lock = xServer.lock(
                        XServer.Lockable.WINDOW_MANAGER,
                        XServer.Lockable.DRAWABLE_MANAGER,
                        XServer.Lockable.PIXMAP_MANAGER)) {
                    nameWindowPixmap(client, inputStream);
                }
                break;
            default:
                throw new BadImplementation();
        }
    }
}
