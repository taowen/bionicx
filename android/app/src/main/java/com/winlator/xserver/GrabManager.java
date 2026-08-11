package com.winlator.xserver;

import com.winlator.core.Bitmask;
import com.winlator.xserver.events.Event;
import com.winlator.xserver.events.PointerWindowEvent;

public class GrabManager implements WindowManager.OnWindowModificationListener {
    private Window window;
    private boolean ownerEvents;
    private boolean releaseWithButtons;
    private EventListener eventListener;
    private Window keyboardWindow;
    private XClient keyboardClient;
    private boolean keyboardOwnerEvents;
    private final XServer xServer;

    public GrabManager(XServer xServer) {
        this.xServer = xServer;
        xServer.windowManager.addOnWindowModificationListener(this);
    }

    @Override
    public void onUnmapWindow(Window window) {
        if (window != null && window.getMapState() != Window.MapState.VIEWABLE) {
            deactivatePointerGrab();
            if (window == keyboardWindow) deactivateKeyboardGrab();
        }
    }

    public Window getWindow() {
        return window;
    }

    public boolean isOwnerEvents() {
        return ownerEvents;
    }

    public boolean isReleaseWithButtons() {
        return releaseWithButtons;
    }

    public EventListener getEventListener() {
        return eventListener;
    }

    public XClient getClient() {
        return eventListener != null ? eventListener.client : null;
    }

    public Window getKeyboardWindow() {
        return keyboardWindow;
    }

    public XClient getKeyboardClient() {
        return keyboardClient;
    }

    public boolean isKeyboardOwnerEvents() {
        return keyboardOwnerEvents;
    }

    public void deactivatePointerGrab() {
        if (window != null) {
            xServer.inputDeviceManager.sendEnterLeaveNotify(window, xServer.inputDeviceManager.getPointWindow(), PointerWindowEvent.Mode.UNGRAB);
            window = null;
            eventListener = null;
        }
    }

    private void activatePointerGrab(Window window, EventListener eventListener, boolean ownerEvents, boolean releaseWithButtons) {
        if (this.window == null) {
            xServer.inputDeviceManager.sendEnterLeaveNotify(xServer.inputDeviceManager.getPointWindow(), window, PointerWindowEvent.Mode.GRAB);
        }
        this.window = window;
        this.releaseWithButtons = releaseWithButtons;
        this.ownerEvents = ownerEvents;
        this.eventListener = eventListener;
    }

    public void activatePointerGrab(Window window, boolean ownerEvents, Bitmask eventMask, XClient client) {
        activatePointerGrab(window, new EventListener(client, eventMask), ownerEvents, false);
    }

    public void activatePointerGrab(Window window) {
        EventListener eventListener = window.getButtonPressListener();
        activatePointerGrab(window, eventListener, eventListener.isInterestedIn(Event.OWNER_GRAB_BUTTON), true);
    }

    public void activateKeyboardGrab(Window window, boolean ownerEvents,
                                     XClient client) {
        keyboardWindow = window;
        keyboardOwnerEvents = ownerEvents;
        keyboardClient = client;
    }

    public void deactivateKeyboardGrab() {
        keyboardWindow = null;
        keyboardClient = null;
        keyboardOwnerEvents = false;
    }

    public void deactivateKeyboardGrab(XClient client) {
        if (keyboardClient == client) deactivateKeyboardGrab();
    }
}
