package com.winlator.xserver;

import com.winlator.core.Bitmask;
import com.winlator.renderer.FullscreenTransformation;
import com.winlator.winhandler.MouseEventFlags;
import com.winlator.winhandler.WinHandler;
import com.winlator.xserver.events.ButtonPress;
import com.winlator.xserver.events.ButtonRelease;
import com.winlator.xserver.events.EnterNotify;
import com.winlator.xserver.events.Event;
import com.winlator.xserver.events.KeyPress;
import com.winlator.xserver.events.KeyRelease;
import com.winlator.xserver.events.LeaveNotify;
import com.winlator.xserver.events.MappingNotify;
import com.winlator.xserver.events.MotionNotify;
import com.winlator.xserver.events.PointerWindowEvent;
import com.winlator.xserver.events.XkbStateNotify;
import com.winlator.xserver.extensions.XInputExtension;

import android.util.Log;

import java.util.ArrayList;

public class InputDeviceManager implements Pointer.OnPointerMotionListener, Keyboard.OnKeyboardListener, WindowManager.OnWindowModificationListener, XResourceManager.OnResourceLifecycleListener {
    private static final byte MOUSE_WHEEL_DELTA = 120;
    private Window pointWindow;
    private final XServer xServer;
    private final ArrayList<PendingPointerEvent> frozenPointerEvents =
            new ArrayList<>();

    private static class PendingPointerEvent {
        final Pointer.Button button;
        final Short x;
        final Short y;

        PendingPointerEvent(Pointer.Button button) {
            this.button = button;
            x = null;
            y = null;
        }

        PendingPointerEvent(short x, short y) {
            button = null;
            this.x = x;
            this.y = y;
        }
    }

    public InputDeviceManager(XServer xServer) {
        this.xServer = xServer;
        pointWindow = xServer.windowManager.rootWindow;
        xServer.windowManager.addOnWindowModificationListener(this);
        xServer.windowManager.addOnResourceLifecycleListener(this);
        xServer.pointer.addOnPointerMotionListener(this);
        xServer.keyboard.addOnKeyboardListener(this);
    }

    @Override
    public void onMapWindow(Window window) {
        updatePointWindow();
    }

    @Override
    public void onUnmapWindow(Window window) {
        updatePointWindow();
    }

    @Override
    public void onChangeWindowZOrder(Window window) {
        updatePointWindow();
    }

    @Override
    public void onUpdateWindowGeometry(Window window, boolean resized) {
        updatePointWindow();
    }

    @Override
    public void onCreateResource(XResource resource) {
        updatePointWindow();
    }

    @Override
    public void onFreeResource(XResource resource) {
        updatePointWindow();
    }

    public void updatePointWindow() {
        Window pointWindow = xServer.windowManager.findPointWindow(xServer.pointer.getClampedX(), xServer.pointer.getClampedY(), true);
        this.pointWindow = pointWindow != null ? pointWindow : xServer.windowManager.rootWindow;
    }

    public Window getPointWindow() {
        return pointWindow;
    }

    private void sendEvent(Window window, int eventId, Event event) {
        Window grabWindow = xServer.grabManager.getWindow();
        if (grabWindow != null && grabWindow.attributes.isEnabled()) {
            EventListener eventListener = xServer.grabManager.getEventListener();
            if (xServer.grabManager.isOwnerEvents() && window != null) {
                window.sendEvent(eventId, event, xServer.grabManager.getClient());
            }
            else if (eventListener.isInterestedIn(eventId)) {
                eventListener.sendEvent(event);
            }
        }
        else if (window != null && window.attributes.isEnabled()) {
            window.sendEvent(eventId, event);
        }
    }

    private void sendEvent(Window window, Bitmask eventMask, Event event) {
        Window grabWindow = xServer.grabManager.getWindow();
        if (grabWindow != null && grabWindow.attributes.isEnabled()) {
            EventListener eventListener = xServer.grabManager.getEventListener();
            if (xServer.grabManager.isOwnerEvents() && window != null) {
                window.sendEvent(eventMask, event, eventListener.client);
            }
            else if (eventListener.isInterestedIn(eventMask)) {
                eventListener.sendEvent(event);
            }
        }
        else if (window != null && window.attributes.isEnabled()) {
            window.sendEvent(eventMask, event);
        }
    }

    public void sendEnterLeaveNotify(Window windowA, Window windowB, PointerWindowEvent.Mode mode) {
        if (windowA == windowB) return;

        boolean sameScreenAndFocus = windowB.isAncestorOf(xServer.windowManager.getFocusedWindow());
        PointerWindowEvent.Detail detailA = PointerWindowEvent.Detail.NONLINEAR;
        PointerWindowEvent.Detail detailB = PointerWindowEvent.Detail.NONLINEAR;

        if (windowA.isAncestorOf(windowB)) {
            detailA = PointerWindowEvent.Detail.ANCESTOR;
            detailB = PointerWindowEvent.Detail.INFERIOR;
        }
        else if (windowB.isAncestorOf(windowA)) {
            detailB = PointerWindowEvent.Detail.ANCESTOR;
            detailA = PointerWindowEvent.Detail.INFERIOR;
        }

        Bitmask keyButMask = getKeyButMask();

        short xA = xServer.pointer.getX();
        short yA = xServer.pointer.getY();
        FullscreenTransformation fullscreenTransformationB = windowB.getFullscreenTransformation();
        if (fullscreenTransformationB != null) {
            short[] transformedPoint = fullscreenTransformationB.transformPointerCoords(xA, yA);
            xA = transformedPoint[0];
            yA = transformedPoint[1];
        }
        short[] localPointA = windowA.rootPointToLocal(xA, yA);

        short xB = xServer.pointer.getX();
        short yB = xServer.pointer.getY();
        FullscreenTransformation fullscreenTransformationA = windowA.getFullscreenTransformation();
        if (fullscreenTransformationA != null) {
            short[] transformedPoint = fullscreenTransformationA.transformPointerCoords(xB, yB);
            xB = transformedPoint[0];
            yB = transformedPoint[1];
        }
        short[] localPointB = windowB.rootPointToLocal(xB, yB);

        sendEvent(windowA, Event.LEAVE_WINDOW, new LeaveNotify(detailA, xServer.windowManager.rootWindow, windowA, null, xA, yA, localPointA[0], localPointA[1], keyButMask, mode, sameScreenAndFocus));
        sendEvent(windowB, Event.ENTER_WINDOW, new EnterNotify(detailB, xServer.windowManager.rootWindow, windowB, null, xB, yB, localPointB[0], localPointB[1], keyButMask, mode, sameScreenAndFocus));
        getXInputExtension().sendCrossingEvent(XInputExtension.XI_LEAVE,
                detailA.ordinal(), mode.ordinal(), windowA, xA, yA,
                sameScreenAndFocus);
        getXInputExtension().sendCrossingEvent(XInputExtension.XI_ENTER,
                detailB.ordinal(), mode.ordinal(), windowB, xB, yB,
                sameScreenAndFocus);
    }

    @Override
    public void onPointerButtonPress(Pointer.Button button) {
        if (xServer.isRelativeMouseMovement()) {
            WinHandler winHandler = xServer.getWinHandler();
            int wheelDelta = button == Pointer.Button.BUTTON_SCROLL_UP ? MOUSE_WHEEL_DELTA : (button == Pointer.Button.BUTTON_SCROLL_DOWN ? -MOUSE_WHEEL_DELTA : 0);
            winHandler.mouseEvent(MouseEventFlags.getFlagFor(button, true), 0, 0, wheelDelta);
            return;
        }
        // Activate a sync passive grab before XI2 so xfwm4 sees the press
        // on c->window. Implicit grabs wait until after XI2: GDK often
        // selects only XI2, and the first core ButtonPress ancestor is
        // the WM frame.
        deliverPointerButtonPress(button, true, false);
        sendXiPointerEvent(XInputExtension.XI_BUTTON_PRESS, button.code());
        if (xServer.grabManager.getWindow() == null)
            deliverPointerButtonPress(button, false, true);
        if (xServer.pointer.getY() < 40) {
            Window grab = xServer.grabManager.getWindow();
            XClient grabClient = xServer.grabManager.getClient();
            Log.i("BionicX", "BXINFO grab-trace btn=" + button.code()
                    + " point=0x" + Integer.toHexString(
                            pointWindow != null ? pointWindow.id : 0)
                    + " grab=0x" + Integer.toHexString(
                            grab != null ? grab.id : 0)
                    + " client=" + GrabManager.describeClient(grabClient)
                    + " origin=" + (pointWindow != null
                            ? GrabManager.describeClient(pointWindow.originClient)
                            : "none")
                    + " sync=" + xServer.grabManager.isPointerSynchronous()
                    + " ownerEvents=" + xServer.grabManager.isOwnerEvents()
                    + " enabled=" + (grab != null
                            && grab.attributes.isEnabled())
                    + " xy=" + xServer.pointer.getX() + ","
                    + xServer.pointer.getY()
                    + " grabs=" + xServer.grabManager.describeGrabsOn(
                            pointWindow)
                    + " last=" + (grabClient != null
                            ? grabClient.describeLastRequest() : "none"));
            xServer.grabManager.watchClientRequests(grabClient, 24);
        }
        // A core-only sync grabber (no XIGrabButton) never AllowEvents:
        // GDK's XFilterEvent eats core ButtonPress and the filter has no
        // XI2 button branch. Replay the activating press to the owner.
        if (xServer.grabManager.isPassiveSynchronousPointerGrab()) {
            XClient coreGrabber = xServer.grabManager.getClient();
            if (coreGrabber != null && !coreGrabber.usedXiPassivePointerGrab()) {
                if (xServer.pointer.getY() < 40)
                    Log.i("BionicX", "BXINFO grab-core-replay client="
                            + GrabManager.describeClient(coreGrabber));
                xServer.grabManager.deactivatePointerGrabForReplay();
                replayPointerButtonPress(button);
            }
        }
    }

    public void replayPointerButtonPress(Pointer.Button button) {
        sendXiPointerEvent(XInputExtension.XI_BUTTON_PRESS, button.code());
        deliverPointerButtonPress(button, false, true);
        ArrayList<PendingPointerEvent> pending =
                new ArrayList<>(frozenPointerEvents);
        frozenPointerEvents.clear();
        for (PendingPointerEvent event : pending) {
            if (event.button != null) {
                sendXiPointerEvent(XInputExtension.XI_BUTTON_RELEASE,
                        event.button.code());
                deliverPointerButtonRelease(event.button);
            }
            else {
                sendXiPointerEvent(XInputExtension.XI_MOTION, 0);
                deliverPointerMove(event.x, event.y);
            }
        }
    }

    public void discardFrozenPointerEvents() {
        frozenPointerEvents.clear();
    }

    public void flushFrozenPointerEvents() {
        ArrayList<PendingPointerEvent> pending =
                new ArrayList<>(frozenPointerEvents);
        frozenPointerEvents.clear();
        for (PendingPointerEvent event : pending) {
            if (event.button != null) deliverPointerButtonRelease(event.button);
            else deliverPointerMove(event.x, event.y);
        }
    }

    private void freezePointerMotion(short x, short y) {
        int lastIndex = frozenPointerEvents.size() - 1;
        PendingPointerEvent motion = new PendingPointerEvent(x, y);
        if (lastIndex >= 0 && frozenPointerEvents.get(lastIndex).x != null)
            frozenPointerEvents.set(lastIndex, motion);
        else frozenPointerEvents.add(motion);
    }

    private void deliverPointerButtonPress(Pointer.Button button,
                                           boolean allowPassiveGrab,
                                           boolean allowImplicitGrab) {
            Window normalWindow =
                    pointWindow.getAncestorWithEventId(Event.BUTTON_PRESS);
            Window grabWindow = xServer.grabManager.getWindow();
            if (grabWindow == null && allowPassiveGrab) {
                xServer.grabManager.activatePassiveButtonGrab(pointWindow,
                        button,
                        xServer.keyboard.getModifiersMask().getBits());
                grabWindow = xServer.grabManager.getWindow();
            }
            if (grabWindow == null && allowImplicitGrab
                    && normalWindow != null) {
                xServer.grabManager.activatePointerGrab(normalWindow);
                grabWindow = normalWindow;
            }

            if (grabWindow != null && grabWindow.attributes.isEnabled()) {
                Bitmask state = getKeyButMask();
                state.unset(button.flag());

                short x = xServer.pointer.getX();
                short y = xServer.pointer.getY();

                FullscreenTransformation fullscreenTransformation = grabWindow.getFullscreenTransformation();
                if (fullscreenTransformation != null) {
                    short[] transformedPoint = fullscreenTransformation.transformPointerCoords(x, y);
                    x = transformedPoint[0];
                    y = transformedPoint[1];
                }

                XClient grabClient = xServer.grabManager.getClient();
                boolean normalRoute = xServer.grabManager.isOwnerEvents()
                        && normalWindow != null && grabClient != null
                        && grabClient.isInterestedIn(Event.BUTTON_PRESS,
                        normalWindow);
                Window eventWindow = normalRoute ? normalWindow : grabWindow;
                short[] localPoint = eventWindow.rootPointToLocal(x, y);
                Window child = eventWindow.isAncestorOf(pointWindow)
                        ? pointWindow : null;
                ButtonPress event = new ButtonPress(button.code(),
                        xServer.windowManager.rootWindow, eventWindow, child,
                        x, y, localPoint[0], localPoint[1], state);
                if (xServer.grabManager.isPassiveSynchronousPointerGrab())
                    event.setSendEvent(true);
                if (normalRoute) {
                    eventWindow.sendEvent(Event.BUTTON_PRESS, event, grabClient);
                }
                else {
                    EventListener listener =
                            xServer.grabManager.getEventListener();
                    // The activating press must reach the grabber even when
                    // an XI2 mask failed to map onto ButtonPressMask.
                    if (listener != null) {
                        listener.sendEvent(event);
                        if (xServer.pointer.getY() < 40)
                            Log.i("BionicX", "BXINFO grab-press sent=0x"
                                    + Integer.toHexString(eventWindow.id)
                                    + " client=" + GrabManager.describeClient(
                                            grabClient)
                                    + " mask=" + listener.eventMask.getBits()
                                    + " send=" + (event.isSendEvent()
                                            ? 1 : 0));
                    }
                    else if (xServer.pointer.getY() < 40) {
                        Log.i("BionicX", "BXINFO grab-press skip=no-listener"
                                + " grab=0x"
                                + Integer.toHexString(grabWindow.id)
                                + " client=" + GrabManager.describeClient(
                                        grabClient));
                    }
                }
            }
            else if (xServer.pointer.getY() < 40) {
                Log.i("BionicX", "BXINFO grab-press skip=no-send"
                        + " grab=0x" + Integer.toHexString(
                                grabWindow != null ? grabWindow.id : 0)
                        + " enabled=" + (grabWindow != null
                                && grabWindow.attributes.isEnabled()));
            }
    }

    @Override
    public void onPointerButtonRelease(Pointer.Button button) {
        if (xServer.isRelativeMouseMovement()) {
            WinHandler winHandler = xServer.getWinHandler();
            winHandler.mouseEvent(MouseEventFlags.getFlagFor(button, false), 0, 0, 0);
            return;
        }
        if (xServer.grabManager.isPointerSynchronous()) {
            frozenPointerEvents.add(new PendingPointerEvent(button));
            return;
        }
        sendXiPointerEvent(XInputExtension.XI_BUTTON_RELEASE, button.code());
        deliverPointerButtonRelease(button);
    }

    private void deliverPointerButtonRelease(Pointer.Button button) {
            Window grabWindow = xServer.grabManager.getWindow();
            Window normalWindow = pointWindow.getAncestorWithEventId(
                    Event.BUTTON_RELEASE);
            XClient grabClient = xServer.grabManager.getClient();
            boolean normalRoute = grabWindow == null ||
                    (xServer.grabManager.isOwnerEvents()
                    && normalWindow != null && grabClient != null
                    && grabClient.isInterestedIn(Event.BUTTON_RELEASE,
                    normalWindow));
            Window window = normalRoute ? normalWindow : null;

            if (grabWindow != null || window != null) {
                Window eventWindow = window != null ? window : grabWindow;

                short x = xServer.pointer.getX();
                short y = xServer.pointer.getY();

                FullscreenTransformation fullscreenTransformation = eventWindow.getFullscreenTransformation();
                if (fullscreenTransformation != null) {
                    short[] transformedPoint = fullscreenTransformation.transformPointerCoords(x, y);
                    x = transformedPoint[0];
                    y = transformedPoint[1];
                }

                short[] localPoint = eventWindow.rootPointToLocal(x, y);
                Window child = eventWindow.isAncestorOf(pointWindow) ? pointWindow : null;
                Bitmask state = getKeyButMask();
                state.set(button.flag());
                ButtonRelease buttonRelease = new ButtonRelease(button.code(),
                        xServer.windowManager.rootWindow, eventWindow, child,
                        x, y, localPoint[0], localPoint[1], state);
                if (grabWindow == null) {
                    eventWindow.sendEvent(Event.BUTTON_RELEASE, buttonRelease);
                }
                else if (normalRoute) {
                    eventWindow.sendEvent(Event.BUTTON_RELEASE, buttonRelease,
                            grabClient);
                }
                else {
                    EventListener listener =
                            xServer.grabManager.getEventListener();
                    if (listener != null
                            && listener.isInterestedIn(Event.BUTTON_RELEASE))
                        listener.sendEvent(buttonRelease);
                }
            }

            if (xServer.pointer.getButtonMask().isEmpty() && xServer.grabManager.isReleaseWithButtons()) {
                xServer.grabManager.deactivatePointerGrab();
            }
    }

    @Override
    public void onPointerMove(short x, short y) {
        Window previousPointWindow = pointWindow;
        updatePointWindow();
        if (previousPointWindow != pointWindow) {
            sendEnterLeaveNotify(previousPointWindow, pointWindow,
                    PointerWindowEvent.Mode.NORMAL);
        }
        sendXiPointerEvent(XInputExtension.XI_MOTION, 0);
        if (xServer.grabManager.isPointerSynchronous()) {
            freezePointerMotion(x, y);
            return;
        }
        deliverPointerMove(x, y);
    }

    private void deliverPointerMove(short x, short y) {
        Bitmask eventMask = createPointerEventMask();
        Window grabWindow = xServer.grabManager.getWindow();
        Window window = grabWindow == null || xServer.grabManager.isOwnerEvents() ? pointWindow.getAncestorWithEventMask(eventMask) : null;

        if (grabWindow != null || window != null) {
            Window eventWindow = window != null ? window : grabWindow;

            FullscreenTransformation fullscreenTransformation = eventWindow.getFullscreenTransformation();
            if (fullscreenTransformation != null) {
                short[] transformedPoint = fullscreenTransformation.transformPointerCoords(x, y);
                x = transformedPoint[0];
                y = transformedPoint[1];
            }

            short[] localPoint = eventWindow.rootPointToLocal(x, y);
            Window child = eventWindow.isAncestorOf(pointWindow) ? pointWindow : null;
            sendEvent(window, eventMask, new MotionNotify(false, xServer.windowManager.rootWindow, eventWindow, child, x, y, localPoint[0], localPoint[1], getKeyButMask()));
        }
    }

    @Override
    public void onKeyPress(byte keycode, int keysym) {
        Window focusedWindow = xServer.windowManager.resolveFocusedWindow(
                xServer.pointer.getClampedX(), xServer.pointer.getClampedY());
        if (focusedWindow == null) return;
        xServer.grabManager.activatePassiveKeyGrab(focusedWindow, keycode,
                xServer.keyboard.getModifiersMask().getBits());
        updatePointWindow();
        sendXiKeyEvent(XInputExtension.XI_KEY_PRESS, keycode, focusedWindow);

        Window eventWindow = null;
        Window child = null;
        if (focusedWindow.isAncestorOf(pointWindow)) {
            eventWindow = pointWindow.getAncestorWithEventId(Event.KEY_PRESS, focusedWindow);
            child = eventWindow.isAncestorOf(pointWindow) ? pointWindow : null;
        }
        if (eventWindow == null) {
            if (focusedWindow.hasEventListenerFor(Event.KEY_PRESS)) {
                eventWindow = focusedWindow;
            }
        }

        Window grabWindow = xServer.grabManager.getKeyboardWindow();
        XClient grabClient = xServer.grabManager.getKeyboardClient();
        boolean sendDirectlyToGrabber = grabWindow != null
                && (!xServer.grabManager.isKeyboardOwnerEvents()
                || eventWindow == null
                || !grabClient.isInterestedIn(Event.KEY_PRESS, eventWindow));
        if (sendDirectlyToGrabber) {
            eventWindow = grabWindow;
            child = grabWindow.isAncestorOf(focusedWindow) ? focusedWindow : null;
        }
        else if (eventWindow == null) return;

        if (!eventWindow.attributes.isEnabled()) return;

        Bitmask keyButMask = getKeyButMask();
        short x = xServer.pointer.getX();
        short y = xServer.pointer.getY();

        FullscreenTransformation fullscreenTransformation = eventWindow.getFullscreenTransformation();
        if (fullscreenTransformation != null) {
            short[] transformedPoint = fullscreenTransformation.transformPointerCoords(x, y);
            x = transformedPoint[0];
            y = transformedPoint[1];
        }

        short[] localPoint = eventWindow.rootPointToLocal(x, y);
        if (keysym != 0 && !xServer.keyboard.hasKeysym(keycode, keysym)) {
            xServer.keyboard.setKeysyms(keycode, keysym, keysym);
            eventWindow.sendEvent(new MappingNotify(MappingNotify.Request.KEYBOARD, keycode, 1));
        }

        KeyPress event = new KeyPress(keycode, xServer.windowManager.rootWindow,
                eventWindow, child, x, y, localPoint[0], localPoint[1], keyButMask);
        if (sendDirectlyToGrabber) grabClient.sendEvent(event);
        else if (grabWindow != null) {
            eventWindow.sendEvent(Event.KEY_PRESS, event, grabClient);
        }
        else eventWindow.sendEvent(Event.KEY_PRESS, event);
    }

    @Override
    public void onKeyRelease(byte keycode) {
        boolean allKeysReleased =
                xServer.keyboard.willHaveNoPressedKeysAfterRelease(keycode);
        Window focusedWindow = xServer.windowManager.resolveFocusedWindow(
                xServer.pointer.getClampedX(), xServer.pointer.getClampedY());
        if (focusedWindow == null) {
            xServer.grabManager.releasePassiveKeyGrab(allKeysReleased);
            return;
        }
        updatePointWindow();
        sendXiKeyEvent(XInputExtension.XI_KEY_RELEASE, keycode, focusedWindow);

        Window eventWindow = null;
        Window child = null;
        if (focusedWindow.isAncestorOf(pointWindow)) {
            eventWindow = pointWindow.getAncestorWithEventId(Event.KEY_RELEASE, focusedWindow);
            child = eventWindow.isAncestorOf(pointWindow) ? pointWindow : null;
        }
        if (eventWindow == null) {
            if (focusedWindow.hasEventListenerFor(Event.KEY_RELEASE)) {
                eventWindow = focusedWindow;
            }
        }

        Window grabWindow = xServer.grabManager.getKeyboardWindow();
        XClient grabClient = xServer.grabManager.getKeyboardClient();
        boolean sendDirectlyToGrabber = grabWindow != null
                && (!xServer.grabManager.isKeyboardOwnerEvents()
                || eventWindow == null
                || !grabClient.isInterestedIn(Event.KEY_RELEASE, eventWindow));
        if (sendDirectlyToGrabber) {
            eventWindow = grabWindow;
            child = grabWindow.isAncestorOf(focusedWindow) ? focusedWindow : null;
        }
        else if (eventWindow == null) {
            xServer.grabManager.releasePassiveKeyGrab(allKeysReleased);
            return;
        }

        if (!eventWindow.attributes.isEnabled()) {
            xServer.grabManager.releasePassiveKeyGrab(allKeysReleased);
            return;
        }

        Bitmask keyButMask = getKeyButMask();
        short x = xServer.pointer.getX();
        short y = xServer.pointer.getY();

        FullscreenTransformation fullscreenTransformation = eventWindow.getFullscreenTransformation();
        if (fullscreenTransformation != null) {
            short[] transformedPoint = fullscreenTransformation.transformPointerCoords(x, y);
            x = transformedPoint[0];
            y = transformedPoint[1];
        }

        short[] localPoint = eventWindow.rootPointToLocal(x, y);
        KeyRelease event = new KeyRelease(keycode, xServer.windowManager.rootWindow,
                eventWindow, child, x, y, localPoint[0], localPoint[1], keyButMask);
        if (sendDirectlyToGrabber) grabClient.sendEvent(event);
        else if (grabWindow != null) {
            eventWindow.sendEvent(Event.KEY_RELEASE, event, grabClient);
        }
        else eventWindow.sendEvent(Event.KEY_RELEASE, event);
        xServer.grabManager.releasePassiveKeyGrab(allKeysReleased);
    }

    @Override
    public void onModifiersChanged(byte keycode, boolean pressed) {
        xServer.sendXkbStateNotify(new XkbStateNotify(
                xServer.keyboard.getModifiersMask().getBits(),
                xServer.keyboard.getBaseModifiers(),
                xServer.keyboard.getLockedModifiers(), keycode,
                pressed ? 2 : 3));
    }

    private Bitmask createPointerEventMask() {
        Bitmask eventMask = new Bitmask();
        eventMask.set(Event.POINTER_MOTION);

        Bitmask buttonMask = xServer.pointer.getButtonMask();
        if (!buttonMask.isEmpty()) {
            eventMask.set(Event.BUTTON_MOTION);

            if (buttonMask.isSet(Pointer.Button.BUTTON_LEFT.flag())) {
                eventMask.set(Event.BUTTON1_MOTION);
            }
            if (buttonMask.isSet(Pointer.Button.BUTTON_MIDDLE.flag())) {
                eventMask.set(Event.BUTTON2_MOTION);
            }
            if (buttonMask.isSet(Pointer.Button.BUTTON_RIGHT.flag())) {
                eventMask.set(Event.BUTTON3_MOTION);
            }
            if (buttonMask.isSet(Pointer.Button.BUTTON_SCROLL_UP.flag())) {
                eventMask.set(Event.BUTTON4_MOTION);
            }
            if (buttonMask.isSet(Pointer.Button.BUTTON_SCROLL_DOWN.flag())) {
                eventMask.set(Event.BUTTON5_MOTION);
            }
        }
        return eventMask;
    }

    private XInputExtension getXInputExtension() {
        return (XInputExtension)xServer.getExtensionByName("XInputExtension");
    }

    private void sendXiPointerEvent(int eventType, int detail) {
        updatePointWindow();
        getXInputExtension().sendDeviceEvent(2, eventType, detail, pointWindow,
                xServer.pointer.getX(), xServer.pointer.getY());
    }

    private void sendXiKeyEvent(int eventType, byte keycode,
                                Window focusedWindow) {
        getXInputExtension().sendDeviceEvent(3, eventType, keycode & 0xff,
                focusedWindow, xServer.pointer.getX(), xServer.pointer.getY());
    }

    public Bitmask getKeyButMask() {
        Bitmask keyButMask = new Bitmask();
        keyButMask.join(xServer.pointer.getButtonMask());
        keyButMask.join(xServer.keyboard.getModifiersMask());
        return keyButMask;
    }
}
