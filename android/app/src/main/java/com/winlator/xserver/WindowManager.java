package com.winlator.xserver;

import android.util.SparseArray;

import com.winlator.core.Bitmask;
import com.winlator.xconnector.XInputStream;
import com.winlator.xserver.errors.BadIdChoice;
import com.winlator.xserver.errors.BadMatch;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.XRequestError;
import com.winlator.xserver.events.ConfigureNotify;
import com.winlator.xserver.events.ConfigureRequest;
import com.winlator.xserver.events.DestroyNotify;
import com.winlator.xserver.events.Event;
import com.winlator.xserver.events.Expose;
import com.winlator.xserver.events.MapNotify;
import com.winlator.xserver.events.MapRequest;
import com.winlator.xserver.events.ReparentNotify;
import com.winlator.xserver.events.ResizeRequest;
import com.winlator.xserver.events.UnmapNotify;
import com.winlator.xserver.events.VisibilityNotify;

import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;

public class WindowManager extends XResourceManager {
    public enum FocusRevertTo {NONE, POINTER_ROOT, PARENT}
    public final Window rootWindow;
    private final SparseArray<Window> windows = new SparseArray<>();
    public final DrawableManager drawableManager;
    private Window focusedWindow;
    private FocusRevertTo focusRevertTo = FocusRevertTo.NONE;
    private boolean pointerRootFocus;
    private final ArrayList<OnWindowModificationListener> onWindowModificationListeners = new ArrayList<>();
    private final IdentityHashMap<Window, Boolean> dirtyContent =
            new IdentityHashMap<>();
    private final ArrayList<Window> exposeAfterRoundTrip = new ArrayList<>();

    public interface OnWindowModificationListener {
        default void onMapWindow(Window window) {}

        default void onUnmapWindow(Window window) {}

        default void onChangeWindowZOrder(Window window) {}

        default void onUpdateWindowContent(Window window) {}

        default void onUpdateWindowGeometry(Window window, boolean resized) {}

        default void onUpdateWindowAttributes(Window window, Bitmask mask) {}

        default void onModifyWindowProperty(Window window, Property property) {}
    }

    public WindowManager(ScreenInfo screenInfo, DrawableManager drawableManager) {
        this.drawableManager = drawableManager;
        int id = IDGenerator.generate();
        Drawable drawable = drawableManager.createDrawable(id, screenInfo.width, screenInfo.height, drawableManager.getVisual());
        rootWindow = new Window(id, drawable, 0, 0, screenInfo.width, screenInfo.height, null);
        rootWindow.attributes.setMapped(true);
        // Xorg starts with the special PointerRoot focus.  Represent it as
        // rootWindow plus POINTER_ROOT, and resolve it dynamically for input.
        focusedWindow = rootWindow;
        focusRevertTo = FocusRevertTo.POINTER_ROOT;
        pointerRootFocus = true;
        windows.put(id, rootWindow);
    }

    public Window getWindow(int id) {
        return windows.get(id);
    }

    public void markExposeAfterRoundTrip(Window window) {
        if (window == null) return;
        window.markExposeAfterRoundTrip();
        exposeAfterRoundTrip.add(window);
    }

    public void flushExposeAfterRoundTrip() {
        int pending = exposeAfterRoundTrip.size();
        if (pending == 0) return;
        for (int i = 0; i < pending; i++) {
            Window window = exposeAfterRoundTrip.get(i);
            if (window == null || !window.takeExposeAfterRoundTrip()) continue;
            if (windows.get(window.id) != window) continue;
            if (!window.isInputOutput()
                    || window.getMapState() != Window.MapState.VIEWABLE)
                continue;
            if (window.hasEventListenerFor(Event.EXPOSURE))
                window.sendEvent(Event.EXPOSURE, new Expose(window));
            else
                window.sendEvent(new Expose(window));
        }
        exposeAfterRoundTrip.clear();
    }

    public ArrayList<Window> findDialogWindows(int id) {
        ArrayList<Window> result = new ArrayList<>();
        for (int i = 0; i < windows.size(); i++) {
            Window window = windows.valueAt(i);
            if (window != null && window.getTransientFor() == id && window.isDialogBox()) result.add(window);
        }
        return result;
    }

    public Window findWindowWithProcessId(int processId) {
        for (int i = 0; i < windows.size(); i++) {
            Window window = windows.valueAt(i);
            if (window != null && window.getProcessId() == processId) return window;
        }
        return null;
    }

    public void destroyWindow(int id) {
        Window window = getWindow(id);
        if (window != null && rootWindow.id != id) {
            unmapWindow(window);
            removeAllSubwindowsAndWindow(window);
        }
    }

    private void removeAllSubwindowsAndWindow(Window window) {
        List<Window> children = new ArrayList<>(window.getChildren());
        for (Window child : children) removeAllSubwindowsAndWindow(child);

        Window parent = window.getParent();
        window.sendEvent(Event.STRUCTURE_NOTIFY, new DestroyNotify(window, window));
        parent.sendEvent(Event.SUBSTRUCTURE_NOTIFY, new DestroyNotify(parent, window));
        windows.remove(window.id);
        window.takeExposeAfterRoundTrip();
        if (window.isInputOutput()) drawableManager.removeDrawable(window.getContent().id);
        triggerOnFreeResourceListener(window);
        if (window == focusedWindow) revertFocus();
        parent.removeChild(window);
    }

    public void mapWindow(Window window, XClient requestingClient) {
        if (!window.attributes.isMapped()) {
            Window parent = window.getParent();
            boolean redirected = parent.hasEventListenerForOtherClient(
                    Event.SUBSTRUCTURE_REDIRECT, requestingClient)
                    && !window.attributes.isOverrideRedirect();
            boolean grabOwner = requestingClient != null
                    && requestingClient.xServer.getServerGrabClient()
                    == requestingClient;
            // A grab owner that maps while a WM holds SubstructureRedirect
            // would otherwise wait forever for MapNotify: the WM cannot
            // answer MapRequest until the grab is released.
            if (!redirected || grabOwner) {
                if (redirected) {
                    parent.sendEvent(Event.SUBSTRUCTURE_REDIRECT,
                            new MapRequest(parent, window));
                }
                window.attributes.setMapped(true);
                window.sendEvent(Event.STRUCTURE_NOTIFY, new MapNotify(window, window));
                parent.sendEvent(Event.SUBSTRUCTURE_NOTIFY, new MapNotify(parent, window));
                exposeNewlyViewableSubtree(window);
                triggerOnMapWindow(window);
            }
            else parent.sendEvent(Event.SUBSTRUCTURE_REDIRECT, new MapRequest(parent, window));
        }
    }

    private void exposeNewlyViewableSubtree(Window window) {
        if (window.getMapState() != Window.MapState.VIEWABLE) return;
        if (window.isInputOutput()) {
            // XMapWindow paints CWBackPixmap/pixel before Expose. xfwm4
            // title/sides are 1x1 windows that SetBG then Map+MoveResize
            // without ClearWindow on that path.
            window.attributes.clearBackground(0, 0, 0, 0);
            window.sendEvent(Event.EXPOSURE, new Expose(window));
            // GDK ensure_native maps then XSync. The Map Expose is read
            // during that sync while clip_region is still empty, so GTK
            // drops it and a newly nativized GtkTextView TEXT child stays
            // black. Send again after the GetInputFocus XSync reply.
            markExposeAfterRoundTrip(window);
            window.sendEvent(Event.VISIBILITY_CHANGE, new VisibilityNotify(
                    window, VisibilityNotify.State.UNOBSCURED));
        }
        for (Window child : window.getChildren())
            exposeNewlyViewableSubtree(child);
    }

    public void unmapWindow(Window window) {
        unmapWindow(window, false);
    }

    public void unmapWindow(Window window, boolean fromConfigure) {
        if (rootWindow.id != window.id && window.attributes.isMapped()) {
            window.attributes.setMapped(false);
            Window parent = window.getParent();
            window.sendEvent(Event.STRUCTURE_NOTIFY,
                    new UnmapNotify(window, window, fromConfigure));
            parent.sendEvent(Event.SUBSTRUCTURE_NOTIFY,
                    new UnmapNotify(parent, window, fromConfigure));
            if (window == focusedWindow) revertFocus();
            triggerOnUnmapWindow(window);
            // Unmapping an obscuring window makes portions of its siblings
            // visible.  Clients without backing-store must receive Expose so
            // they repaint those regions (dialogs managed by a WM exercise
            // this path heavily).  A full-window exposure is conservative
            // but protocol-correct and keeps the compositor stateless.
            for (Window sibling : parent.getChildren()) {
                if (sibling != window) exposeNewlyViewableSubtree(sibling);
            }
        }
    }

    public void mapSubWindows(Window window, XClient requestingClient) {
        for (Window child : new ArrayList<>(window.getChildren()))
            mapWindow(child, requestingClient);
    }

    public Window getFocusedWindow() {
        return focusedWindow;
    }

    public void revertFocus() {
        switch (focusRevertTo) {
            case NONE:
                focusedWindow = null;
                pointerRootFocus = false;
                break;
            case POINTER_ROOT:
                focusedWindow = rootWindow;
                pointerRootFocus = true;
                break;
            case PARENT:
                if (focusedWindow.getParent() != null) focusedWindow = focusedWindow.getParent();
                pointerRootFocus = false;
                break;
        }
    }

    public void setFocus(Window focusedWindow, FocusRevertTo focusRevertTo) {
        this.focusedWindow = focusedWindow;
        this.focusRevertTo = focusRevertTo;
        pointerRootFocus = false;
    }

    public void setPointerRootFocus(FocusRevertTo focusRevertTo) {
        focusedWindow = rootWindow;
        this.focusRevertTo = focusRevertTo;
        pointerRootFocus = true;
    }

    public boolean isPointerRootFocus() {
        return pointerRootFocus;
    }

    /**
     * Resolve PointerRoot to the deepest currently viewable window below the
     * pointer.  X11 represents PointerRoot as the special focus ID 1; keeping
     * rootWindow as the stored value loses that dynamic routing semantic.
     */
    public Window resolveFocusedWindow(short pointerX, short pointerY) {
        if (!pointerRootFocus) return focusedWindow;
        Window pointed = findPointWindow(pointerX, pointerY, true);
        return pointed != null ? pointed : rootWindow;
    }

    public FocusRevertTo getFocusRevertTo() {
        return focusRevertTo;
    }

    public Window createWindow(int id, Window parent, short x, short y, short width, short height, WindowAttributes.WindowClass windowClass, Visual visual, byte depth, XClient client) throws XRequestError {
        if (windows.indexOfKey(id) >= 0) throw new BadIdChoice(id);

        boolean isInputOutput = false;
        switch (windowClass) {
            case COPY_FROM_PARENT:
                depth = (depth != 0 || !parent.isInputOutput()) ? depth : parent.getContent().visual.depth;
                isInputOutput = parent.isInputOutput();
                break;
            case INPUT_OUTPUT:
                if (parent.isInputOutput()) {
                    depth = depth == 0 ? parent.getContent().visual.depth : depth;
                    isInputOutput = true;
                } else throw new BadMatch();
                break;
            case INPUT_ONLY:
                isInputOutput = false;
                break;
        }

        if (isInputOutput) {
            visual = visual == null ? parent.getContent().visual : visual;
            if (depth != visual.depth) throw new BadMatch();
        }

        Drawable drawable = null;
        if (isInputOutput) {
            drawable = drawableManager.createDrawable(id, width, height, visual);
            if (drawable == null) throw new BadIdChoice(id);
        }

        final Window window = new Window(id, drawable, x, y, width, height, client);
        window.attributes.setWindowClass(isInputOutput
                ? WindowAttributes.WindowClass.INPUT_OUTPUT
                : WindowAttributes.WindowClass.INPUT_ONLY);
        if (drawable != null) drawable.setOnDrawListener(() -> markWindowContentDirty(window));
        windows.put(id, window);
        parent.addChild(window);
        triggerOnCreateResourceListener(window);
        return window;
    }

    private void changeWindowGeometry(Window window, short x, short y, short width, short height) {
        boolean resized = window.getWidth() != width || window.getHeight() != height;
        if (resized && window.hasEventListenerFor(Event.RESIZE_REDIRECT)) {
            window.sendEvent(Event.SUBSTRUCTURE_REDIRECT, new ResizeRequest(window, width, height));
            width = window.getWidth();
            height = window.getHeight();
            resized = false;
        }

        if (resized && window.isInputOutput()) {
            // Replacing the backing buffer disconnects NameWindowPixmap
            // aliases that shared the previous pixels. Clients must name
            // a new pixmap after ConfigureWindow.
            Drawable oldContent = window.getContent();
            drawableManager.removeDrawable(oldContent.id);
            Drawable newContent = drawableManager.createDrawable(oldContent.id, width, height, oldContent.visual);
            newContent.setOffscreenStorage(oldContent.isOffscreenStorage());
            newContent.setOnDrawListener(() -> markWindowContentDirty(window));
            window.setContent(newContent);
        }

        if (resized || window.getX() != x || window.getY() != y) {
            window.setX(x);
            window.setY(y);
            window.setWidth(width);
            window.setHeight(height);
            triggerOnUpdateWindowGeometry(window, resized);
        }

        if (resized && window.isInputOutput() && window.attributes.isMapped()) {
            window.attributes.clearBackground(0, 0, 0, 0);
        }
    }

    private void changeWindowZOrder(Window.StackMode stackMode, Window window, Window sibling) {
        Window parent = window.getParent();
        switch (stackMode) {
            case ABOVE:
                parent.moveChildAbove(window, sibling);
                break;
            case BELOW:
                parent.moveChildBelow(window, sibling);
                break;
        }
        triggerOnChangeWindowZOrder(window);
    }

    public void configureWindow(Window window, Bitmask valueMask,
            XInputStream inputStream, XClient requestingClient)
            throws XRequestError {
        short x = window.getX();
        short y = window.getY();
        short width = window.getWidth();
        short height = window.getHeight();
        short borderWidth = window.getBorderWidth();
        Window sibling = null;
        Window.StackMode stackMode = null;

        for (int index : valueMask) {
            switch (index) {
                case Window.FLAG_X:
                    x = (short)inputStream.readInt();
                    break;
                case Window.FLAG_Y:
                    y = (short)inputStream.readInt();
                    break;
                case Window.FLAG_WIDTH:
                    width = (short)inputStream.readInt();
                    break;
                case Window.FLAG_HEIGHT:
                    height = (short)inputStream.readInt();
                    break;
                case Window.FLAG_BORDER_WIDTH:
                    borderWidth = (short)inputStream.readInt();
                    break;
                case Window.FLAG_SIBLING:
                    sibling = getWindow(inputStream.readInt());
                    break;
                case Window.FLAG_STACK_MODE:
                    stackMode = Window.StackMode.values()[inputStream.readInt()];
                    break;
            }
        }

        if (width <= 0) throw new BadValue(width);
        if (height <= 0) throw new BadValue(height);

        Window parent = window.getParent();
        boolean overrideRedirect = window.attributes.isOverrideRedirect();
        boolean redirected = parent.hasEventListenerForOtherClient(
                Event.SUBSTRUCTURE_REDIRECT, requestingClient)
                && !overrideRedirect;
        boolean grabOwner = requestingClient != null
                && requestingClient.xServer.getServerGrabClient()
                == requestingClient;
        // A grab owner that resizes while a WM holds SubstructureRedirect
        // would otherwise wait forever for ConfigureNotify: the WM cannot
        // answer ConfigureRequest until the grab is released. GTK menus
        // GrabServer then ConfigureWindow the popup.
        if (!redirected || grabOwner) {
            if (redirected) {
                parent.sendEvent(Event.SUBSTRUCTURE_REDIRECT,
                        new ConfigureRequest(parent, window,
                                window.previousSibling(), x, y, width, height,
                                borderWidth, stackMode, valueMask));
            }
            boolean resized = window.getWidth() != width
                    || window.getHeight() != height;
            changeWindowGeometry(window, x, y, width, height);

            window.setBorderWidth(borderWidth);
            if (stackMode != null) changeWindowZOrder(stackMode, window, sibling);

            Window previousSibling = window.previousSibling();
            window.sendEvent(Event.STRUCTURE_NOTIFY, new ConfigureNotify(window, window, previousSibling, x, y, width, height, borderWidth, overrideRedirect));
            parent.sendEvent(Event.SUBSTRUCTURE_NOTIFY, new ConfigureNotify(parent, window, previousSibling, x, y, width, height, borderWidth, overrideRedirect));
            // Expose after ConfigureNotify so GTK recreates its cairo
            // surface at the new size before painting. The Applications
            // menu maps 32-bit, resizes, and otherwise stays unpainted.
            if (resized && window.isInputOutput()
                    && window.attributes.isMapped())
                window.sendEvent(new Expose(window));
        }
        else parent.sendEvent(Event.SUBSTRUCTURE_REDIRECT, new ConfigureRequest(parent, window, window.previousSibling(), x, y, width, height, borderWidth, stackMode, valueMask));
    }

    public void reparentWindow(Window window, Window newParent, short x, short y)
            throws XRequestError {
        reparentWindow(window, newParent, x, y, null);
    }

    public void reparentWindow(Window window, Window newParent, short x, short y,
                               XClient requestingClient) throws XRequestError {
        if (newParent == window || window.isAncestorOf(newParent)) throw new BadMatch();
        if (window.isInputOutput() && !newParent.isInputOutput()) throw new BadMatch();

        Window oldParent = window.getParent();
        // X protocol ReparentWindow: if mapped, UnmapWindow (from-configure),
        // then reparent, then MapWindow. WMs reparent already-mapped clients
        // into a new unmapped frame; skipping the unmap/remap leaves the
        // client mapped under an unmapped ancestor (IsUnviewable) and makes
        // the WM's subsequent MapWindow a no-op.
        boolean wasMapped = window.attributes.isMapped();
        if (wasMapped) unmapWindow(window, true);

        if (oldParent != null) oldParent.removeChild(window);
        newParent.addChild(window);
        boolean moved = window.getX() != x || window.getY() != y;
        window.setX(x);
        window.setY(y);
        if (moved) triggerOnUpdateWindowGeometry(window, false);

        boolean overrideRedirect = window.attributes.isOverrideRedirect();
        window.sendEvent(Event.STRUCTURE_NOTIFY,
                new ReparentNotify(window, window, newParent, x, y, overrideRedirect));
        if (oldParent != null) {
            oldParent.sendEvent(Event.SUBSTRUCTURE_NOTIFY,
                    new ReparentNotify(oldParent, window, newParent, x, y,
                            overrideRedirect));
        }
        if (newParent != oldParent) {
            newParent.sendEvent(Event.SUBSTRUCTURE_NOTIFY,
                    new ReparentNotify(newParent, window, newParent, x, y,
                            overrideRedirect));
        }

        if (wasMapped) mapWindow(window, requestingClient);
    }

    public Window findPointWindow(short rootX, short rootY) {
        return findPointWindow(rootWindow, rootX, rootY, false);
    }

    public Window findPointWindow(short rootX, short rootY, boolean useFullscreenTransformation) {
        return findPointWindow(rootWindow, rootX, rootY, useFullscreenTransformation);
    }

    private Window findPointWindow(Window window, short rootX, short rootY, boolean useFullscreenTransformation) {
        return window.findPointInWindow(rootX, rootY, useFullscreenTransformation);
    }

    public void addOnWindowModificationListener(OnWindowModificationListener onWindowModificationListener) {
        onWindowModificationListeners.add(onWindowModificationListener);
    }

    public void removeOnWindowModificationListener(OnWindowModificationListener onWindowModificationListener) {
        onWindowModificationListeners.remove(onWindowModificationListener);
    }

    public void triggerOnMapWindow(Window window) {
        for (int i = onWindowModificationListeners.size()-1; i >= 0; i--) {
            onWindowModificationListeners.get(i).onMapWindow(window);
        }
    }

    public void triggerOnUnmapWindow(Window window) {
        for (int i = onWindowModificationListeners.size()-1; i >= 0; i--) {
            onWindowModificationListeners.get(i).onUnmapWindow(window);
        }
    }

    public void triggerOnChangeWindowZOrder(Window window) {
        for (int i = onWindowModificationListeners.size()-1; i >= 0; i--) {
            onWindowModificationListeners.get(i).onChangeWindowZOrder(window);
        }
    }

    public void markWindowContentDirty(Window window) {
        synchronized (dirtyContent) {
            dirtyContent.put(window, Boolean.TRUE);
        }
    }

    public void flushDirtyWindowContent() {
        ArrayList<Window> batch;
        synchronized (dirtyContent) {
            if (dirtyContent.isEmpty()) return;
            batch = new ArrayList<>(dirtyContent.keySet());
            dirtyContent.clear();
        }
        for (Window window : batch) triggerOnUpdateWindowContent(window);
    }

    public void triggerOnUpdateWindowContent(Window window) {
        for (int i = onWindowModificationListeners.size()-1; i >= 0; i--) {
            onWindowModificationListeners.get(i).onUpdateWindowContent(window);
        }
    }

    public void triggerOnUpdateWindowGeometry(Window window, boolean resized) {
        for (int i = onWindowModificationListeners.size()-1; i >= 0; i--) {
            onWindowModificationListeners.get(i).onUpdateWindowGeometry(window, resized);
        }
    }

    public void triggerOnUpdateWindowAttributes(Window window, Bitmask mask) {
        for (int i = onWindowModificationListeners.size()-1; i >= 0; i--) {
            onWindowModificationListeners.get(i).onUpdateWindowAttributes(window, mask);
        }
    }

    public void triggerOnModifyWindowProperty(Window window, Property property) {
        for (int i = onWindowModificationListeners.size()-1; i >= 0; i--) {
            onWindowModificationListeners.get(i).onModifyWindowProperty(window, property);
        }
    }
}
