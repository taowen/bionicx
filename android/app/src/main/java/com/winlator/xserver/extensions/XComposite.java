package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.renderer.Texture;
import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.core.Callback;
import com.winlator.xserver.Drawable;
import com.winlator.xserver.IDGenerator;
import com.winlator.xserver.Pixmap;
import com.winlator.xserver.Window;
import com.winlator.xserver.WindowAttributes;
import com.winlator.xserver.WindowManager;
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
import java.util.ArrayList;

public class XComposite extends Extension {
    public static final byte MAJOR_VERSION = 0;
    public static final byte MINOR_VERSION = 3;

    public enum UpdateMode {REDIRECT_AUTOMATIC, REDIRECT_MANUAL}

    private static abstract class ClientOpcodes {
        private static final byte QUERY_VERSION = 0;
        private static final byte REDIRECT_WINDOW = 1;
        private static final byte REDIRECT_SUBWINDOWS = 2;
        private static final byte UNREDIRECT_WINDOW = 3;
        private static final byte UNREDIRECT_SUBWINDOWS = 4;
        private static final byte NAME_WINDOW_PIXMAP = 6;
        private static final byte GET_OVERLAY_WINDOW = 7;
        private static final byte RELEASE_OVERLAY_WINDOW = 8;
    }

    private Window overlayWindow;
    private final ArrayList<XClient> overlayClients = new ArrayList<>();
    private final Callback<XClient> onOverlayClientGone = this::dropOverlayClient;

    public XComposite(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
        xServer.windowManager.addOnWindowModificationListener(
                new WindowManager.OnWindowModificationListener() {
                    @Override
                    public void onMapWindow(Window window) {
                        applyRedirectStorage(window);
                        raiseOverlay();
                    }

                    @Override
                    public void onChangeWindowZOrder(Window window) {
                        raiseOverlay();
                    }
                });
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

    public boolean isOverlayWindow(Window window) {
        return overlayWindow != null && window == overlayWindow;
    }

    public boolean hasOverlay() {
        return overlayWindow != null
                && xServer.windowManager.getWindow(overlayWindow.id) != null;
    }

    public boolean isOverlayOutput(Window window) {
        return isOverlayTree(window) && window != overlayWindow;
    }

    private boolean isOverlayTree(Window window) {
        while (window != null) {
            if (window == overlayWindow) return true;
            window = window.getParent();
        }
        return false;
    }

    // Overlay and its output children stay on screen even when root has
    // RedirectSubwindows. Other redirected children keep a backing pixmap
    // and are skipped by the GL renderer via offscreenStorage.
    private void applyRedirectStorage(Window window) {
        if (window == null) return;
        if (isOverlayTree(window)) {
            if (window.isInputOutput() && window.getContent() != null)
                window.getContent().setOffscreenStorage(false);
            for (Window child : window.getChildren()) applyRedirectStorage(child);
            return;
        }
        if (window.isInputOutput() && window.getContent() != null) {
            window.getContent().setOffscreenStorage(isRedirected(window));
        }
        for (Window child : window.getChildren()) applyRedirectStorage(child);
    }

    private void raiseOverlay() {
        if (overlayWindow == null) return;
        Window root = xServer.windowManager.rootWindow;
        if (overlayWindow.getParent() != root) return;
        // moveChildAbove does not fire onChangeWindowZOrder, so raising from
        // that callback cannot loop — including when the overlay itself was
        // stacked below.
        root.moveChildAbove(overlayWindow, null);
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
        window.setTag("compositeRedirectSubwindows", Byte.valueOf(updateMode));
        applyRedirectStorage(window);
        xServer.windowManager.triggerOnChangeWindowZOrder(window);
    }

    private boolean isRedirected(Window window) {
        if (isOverlayTree(window)) return false;
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

    private Window createOverlayWindow() throws XRequestError {
        Window root = xServer.windowManager.rootWindow;
        Window overlay = xServer.windowManager.createWindow(
                IDGenerator.generate(), root, (short)0, (short)0,
                root.getWidth(), root.getHeight(),
                WindowAttributes.WindowClass.INPUT_OUTPUT,
                root.getContent().visual, root.getContent().visual.depth,
                null);
        overlay.attributes.setOverrideRedirect(true);
        root.moveChildAbove(overlay, null);
        if (overlay.getContent() != null) overlay.getContent().setOffscreenStorage(false);
        return overlay;
    }

    private void getOverlayWindow(XClient client, XInputStream inputStream,
                                  XOutputStream outputStream)
            throws IOException, XRequestError {
        int windowId = inputStream.readInt();
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        if (overlayWindow != null
                && xServer.windowManager.getWindow(overlayWindow.id) == null) {
            overlayWindow = null;
            overlayClients.clear();
        }
        if (overlayWindow == null) overlayWindow = createOverlayWindow();
        if (!overlayClients.contains(client)) {
            overlayClients.add(client);
            client.addOnDestroyListener(onOverlayClientGone);
        }
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(overlayWindow.id);
            outputStream.writePad(20);
        }
    }

    private void dropOverlayClient(XClient client) {
        if (!overlayClients.remove(client)) return;
        if (!overlayClients.isEmpty() || overlayWindow == null) return;
        int id = overlayWindow.id;
        overlayWindow = null;
        xServer.windowManager.destroyWindow(id);
    }

    private void releaseOverlayWindow(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        dropOverlayClient(client);
    }

    private void unredirectSubwindows(XClient client, XInputStream inputStream)
            throws XRequestError {
        int windowId = inputStream.readInt();
        inputStream.skip(4);
        Window window = xServer.windowManager.getWindow(windowId);
        if (window == null) throw new BadWindow(windowId);
        window.removeTag("compositeRedirectSubwindows");
        applyRedirectStorage(window);
        xServer.windowManager.triggerOnChangeWindowZOrder(window);
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
                try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER,
                        XServer.Lockable.DRAWABLE_MANAGER)) {
                    redirectSubwindows(client, inputStream);
                }
                break;
            case ClientOpcodes.UNREDIRECT_WINDOW:
                try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER, XServer.Lockable.DRAWABLE_MANAGER)) {
                    unredirectWindow(client, inputStream, outputStream);
                }
                break;
            case ClientOpcodes.UNREDIRECT_SUBWINDOWS:
                try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER,
                        XServer.Lockable.DRAWABLE_MANAGER)) {
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
            case ClientOpcodes.GET_OVERLAY_WINDOW:
                try (XLock lock = xServer.lock(
                        XServer.Lockable.WINDOW_MANAGER,
                        XServer.Lockable.DRAWABLE_MANAGER)) {
                    getOverlayWindow(client, inputStream, outputStream);
                }
                break;
            case ClientOpcodes.RELEASE_OVERLAY_WINDOW:
                try (XLock lock = xServer.lock(
                        XServer.Lockable.WINDOW_MANAGER,
                        XServer.Lockable.DRAWABLE_MANAGER)) {
                    releaseOverlayWindow(client, inputStream);
                }
                break;
            default:
                throw new BadImplementation();
        }
    }
}
