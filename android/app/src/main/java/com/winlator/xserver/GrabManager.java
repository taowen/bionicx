package com.winlator.xserver;

import com.winlator.core.Bitmask;
import com.winlator.xserver.events.Event;
import com.winlator.xserver.events.PointerWindowEvent;

import java.util.ArrayList;

public class GrabManager implements WindowManager.OnWindowModificationListener,
        XResourceManager.OnResourceLifecycleListener {
    private Window window;
    private boolean ownerEvents;
    private boolean releaseWithButtons;
    private EventListener eventListener;
    private Window keyboardWindow;
    private XClient keyboardClient;
    private boolean keyboardOwnerEvents;
    private int passiveActivationKey = -1;
    private final ArrayList<PassiveKeyGrab> passiveKeyGrabs = new ArrayList<>();
    private final ArrayList<PassiveButtonGrab> passiveButtonGrabs =
            new ArrayList<>();
    private final XServer xServer;

    public GrabManager(XServer xServer) {
        this.xServer = xServer;
        xServer.windowManager.addOnWindowModificationListener(this);
        xServer.windowManager.addOnResourceLifecycleListener(this);
    }

    @Override
    public void onUnmapWindow(Window window) {
        if (window != null && window.getMapState() != Window.MapState.VIEWABLE) {
            deactivatePointerGrab();
            if (window == keyboardWindow) deactivateKeyboardGrab();
        }
    }

    @Override
    public void onFreeResource(XResource resource) {
        if (!(resource instanceof Window)) return;
        Window freedWindow = (Window)resource;
        passiveKeyGrabs.removeIf(grab -> grab.window == freedWindow);
        passiveButtonGrabs.removeIf(grab -> grab.window == freedWindow);
        if (window == freedWindow) deactivatePointerGrab();
        if (keyboardWindow == freedWindow) deactivateKeyboardGrab();
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

    public void deactivatePointerGrab(XClient client) {
        if (getClient() == client) deactivatePointerGrab();
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

    private static class PassiveButtonGrab {
        final Window window;
        final int button;
        final int modifiers;
        final boolean ownerEvents;
        final Bitmask eventMask;
        final XClient client;

        PassiveButtonGrab(Window window, int button, int modifiers,
                          boolean ownerEvents, Bitmask eventMask,
                          XClient client) {
            this.window = window;
            this.button = button;
            this.modifiers = modifiers;
            this.ownerEvents = ownerEvents;
            this.eventMask = eventMask;
            this.client = client;
        }
    }

    public boolean addPassiveButtonGrab(Window window, int button,
                                        int modifiers, boolean ownerEvents,
                                        Bitmask eventMask, XClient client) {
        for (PassiveButtonGrab grab : passiveButtonGrabs) {
            if (grab.window == window && grab.client != client
                    && overlaps(grab.button, button, 0)
                    && overlaps(grab.modifiers, modifiers, 0x8000)) return false;
        }
        removePassiveButtonGrabs(window, button, modifiers, client);
        passiveButtonGrabs.add(new PassiveButtonGrab(window, button, modifiers,
                ownerEvents, eventMask, client));
        return true;
    }

    public void removePassiveButtonGrabs(Window window, int button,
                                         int modifiers, XClient client) {
        passiveButtonGrabs.removeIf(grab -> grab.client == client
                && grab.window == window
                && (button == 0 || grab.button == button)
                && (modifiers == 0x8000 || grab.modifiers == modifiers));
    }

    public void removePassiveButtonGrabs(XClient client) {
        passiveButtonGrabs.removeIf(grab -> grab.client == client);
    }

    public boolean activatePassiveButtonGrab(Window pointWindow, byte button,
                                             int modifiers) {
        if (window != null) return false;
        Window candidate = pointWindow;
        while (candidate != null) {
            for (PassiveButtonGrab grab : passiveButtonGrabs) {
                if (grab.window == candidate
                        && (grab.button == 0 || grab.button == (button & 0xff))
                        && (grab.modifiers == 0x8000
                        || grab.modifiers == (modifiers & 0xff))) {
                    activatePointerGrab(grab.window,
                            new EventListener(grab.client, grab.eventMask),
                            grab.ownerEvents, true);
                    return true;
                }
            }
            candidate = candidate.getParent();
        }
        return false;
    }

    public void activateKeyboardGrab(Window window, boolean ownerEvents,
                                     XClient client) {
        keyboardWindow = window;
        keyboardOwnerEvents = ownerEvents;
        keyboardClient = client;
        passiveActivationKey = -1;
    }

    public void deactivateKeyboardGrab() {
        keyboardWindow = null;
        keyboardClient = null;
        keyboardOwnerEvents = false;
        passiveActivationKey = -1;
    }

    public void deactivateKeyboardGrab(XClient client) {
        if (keyboardClient == client) deactivateKeyboardGrab();
    }

    private static class PassiveKeyGrab {
        final Window window;
        final int keycode;
        final int modifiers;
        final boolean ownerEvents;
        final XClient client;

        PassiveKeyGrab(Window window, int keycode, int modifiers,
                       boolean ownerEvents, XClient client) {
            this.window = window;
            this.keycode = keycode;
            this.modifiers = modifiers;
            this.ownerEvents = ownerEvents;
            this.client = client;
        }
    }

    private static boolean overlaps(int left, int right, int wildcard) {
        return left == wildcard || right == wildcard || left == right;
    }

    public boolean addPassiveKeyGrab(Window window, int keycode, int modifiers,
                                     boolean ownerEvents, XClient client) {
        for (PassiveKeyGrab grab : passiveKeyGrabs) {
            if (grab.window == window && grab.client != client
                    && overlaps(grab.keycode, keycode, 0)
                    && overlaps(grab.modifiers, modifiers, 0x8000)) return false;
        }
        removePassiveKeyGrabs(window, keycode, modifiers, client);
        passiveKeyGrabs.add(new PassiveKeyGrab(window, keycode, modifiers,
                ownerEvents, client));
        return true;
    }

    public void removePassiveKeyGrabs(Window window, int keycode,
                                      int modifiers, XClient client) {
        passiveKeyGrabs.removeIf(grab -> grab.client == client
                && grab.window == window
                && (keycode == 0 || grab.keycode == keycode)
                && (modifiers == 0x8000 || grab.modifiers == modifiers));
    }

    public void removePassiveKeyGrabs(XClient client) {
        passiveKeyGrabs.removeIf(grab -> grab.client == client);
    }

    public boolean activatePassiveKeyGrab(Window focusedWindow, byte keycode,
                                          int modifiers) {
        if (keyboardClient != null) return false;
        Window candidate = focusedWindow;
        while (candidate != null) {
            for (PassiveKeyGrab grab : passiveKeyGrabs) {
                if (grab.window == candidate
                        && (grab.keycode == 0 || grab.keycode == (keycode & 0xff))
                        && (grab.modifiers == 0x8000
                        || grab.modifiers == (modifiers & 0xff))) {
                    activateKeyboardGrab(grab.window, grab.ownerEvents,
                            grab.client);
                    passiveActivationKey = keycode & 0xff;
                    return true;
                }
            }
            candidate = candidate.getParent();
        }
        return false;
    }

    public void releasePassiveKeyGrab(boolean allKeysReleased) {
        if (passiveActivationKey >= 0 && allKeysReleased)
            deactivateKeyboardGrab();
    }
}
