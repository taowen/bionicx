package com.winlator.xserver;

import androidx.collection.ArrayMap;
import androidx.collection.ArraySet;

import com.winlator.core.Bitmask;
import com.winlator.core.Callback;
import com.winlator.xconnector.ConnectedClient;
import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xserver.errors.XRequestError;
import com.winlator.xserver.events.Event;

import java.io.IOException;
import java.util.ArrayList;

public class XClient extends ConnectedClient implements XResourceManager.OnResourceLifecycleListener {
    public final XServer xServer;
    private boolean authenticated = false;
    public final Integer resourceIDBase;
    private short sequenceNumber = 0;
    private int requestLength;
    private byte requestData;
    private int initialLength;
    private int xkbEventMask;
    private int xkbMapMask;
    private final ArrayMap<Window, EventListener> eventListeners = new ArrayMap<>();
    private final ArrayMap<Window, ArrayMap<Integer, byte[]>> xiEventMasks =
            new ArrayMap<>();
    private final ArrayMap<Window, Integer> randrEventMasks = new ArrayMap<>();
    private final ArrayList<XResource> resources = new ArrayList<>();
    private final ArrayMap<Integer, String> openFonts = new ArrayMap<>();
    private final ArrayList<Window> saveSet = new ArrayList<>();
    private final ArrayList<Callback<XClient>> onDestroyListeners = new ArrayList<>();
    private int lastOpcode;
    private int lastRequestData;
    private boolean usedXiPassivePointerGrab;

    public XClient(long nativePtr, int fd, XServer xServer) {
        super(nativePtr, fd);
        this.xServer = xServer;

        try (XLock lock = xServer.lockAll()) {
            resourceIDBase = xServer.resourceIDs.get();
            xServer.windowManager.addOnResourceLifecycleListener(this);
            xServer.pixmapManager.addOnResourceLifecycleListener(this);
            xServer.graphicsContextManager.addOnResourceLifecycleListener(this);
            xServer.cursorManager.addOnResourceLifecycleListener(this);
        }
    }

    public void registerAsOwnerOfResource(XResource resource) {
        resources.add(resource);
    }

    public void setEventListenerForWindow(Window window, Bitmask eventMask) {
        EventListener eventListener = eventListeners.get(window);
        if (eventListener != null) window.removeEventListener(eventListener);
        if (eventMask.isEmpty()) return;
        eventListener = new EventListener(this, eventMask);
        eventListeners.put(window, eventListener);
        window.addEventListener(eventListener);
    }

    public boolean sendEvent(Event event) {
        try {
            event.send(sequenceNumber, outputStream);
            return true;
        }
        catch (IOException e) {
            GrabManager.grabTrace("BXINFO grab-press-io client="
                    + GrabManager.describeClient(this) + " " + e.getMessage());
            return false;
        }
    }

    public boolean isInterestedIn(int eventId, Window window) {
        EventListener eventListener = eventListeners.get(window);
        return eventListener != null && eventListener.isInterestedIn(eventId);
    }

    public boolean isAuthenticated() {
        return authenticated;
    }

    public void setAuthenticated(boolean authenticated) {
        this.authenticated = authenticated;
    }

    public void freeResources() {
        try (XLock lock = xServer.lockAll()) {
            restoreSaveSet();
            while (!resources.isEmpty()) {
                XResource resource = resources.remove(resources.size()-1);
                if (resource instanceof Window) {
                    xServer.windowManager.destroyWindow(resource.id);
                }
                else if (resource instanceof Pixmap) {
                    xServer.pixmapManager.freePixmap(resource.id);
                }
                else if (resource instanceof GraphicsContext) {
                    xServer.graphicsContextManager.freeGraphicsContext(resource.id);
                }
                else if (resource instanceof Cursor) {
                    xServer.cursorManager.freeCursor(resource.id);
                }
            }

            while (!eventListeners.isEmpty()) {
                int i = eventListeners.size()-1;
                eventListeners.keyAt(i).removeEventListener(eventListeners.removeAt(i));
            }
            xiEventMasks.clear();
            randrEventMasks.clear();
            openFonts.clear();

            xServer.windowManager.removeOnResourceLifecycleListener(this);
            xServer.pixmapManager.removeOnResourceLifecycleListener(this);
            xServer.graphicsContextManager.removeOnResourceLifecycleListener(this);
            xServer.cursorManager.removeOnResourceLifecycleListener(this);
            xServer.resourceIDs.free(resourceIDBase);
        }
    }

    public void generateSequenceNumber() {
        sequenceNumber++;
    }

    public short getSequenceNumber() {
        return sequenceNumber;
    }

    public int getRequestLength() {
        return requestLength;
    }

    public void setRequestLength(int requestLength) {
        this.requestLength = requestLength;
        initialLength = inputStream.available();
    }

    public byte getRequestData() {
        return requestData;
    }

    public void setRequestData(byte requestData) {
        this.requestData = requestData;
    }

    public void noteRequest(int opcode, int data) {
        lastOpcode = opcode;
        lastRequestData = data;
    }

    public void noteXiPassivePointerGrab() {
        usedXiPassivePointerGrab = true;
    }

    public boolean usedXiPassivePointerGrab() {
        return usedXiPassivePointerGrab;
    }

    public String describeLastRequest() {
        if (lastOpcode == 0 && sequenceNumber == 0) return "none";
        return "opcode=" + (lastOpcode & 0xff)
                + " data=" + (lastRequestData & 0xff)
                + " seq=" + (sequenceNumber & 0xffff);
    }

    public void updateXkbEventSelection(int affect, int clear, int selectAll,
                                        int affectMap, int map) {
        xkbEventMask = (xkbEventMask & ~(affect & clear))
                | (affect & selectAll);
        xkbMapMask = (xkbMapMask & ~affectMap) | (map & affectMap);
    }

    public int getXkbEventMask() {
        return xkbEventMask;
    }

    public int getXkbMapMask() {
        return xkbMapMask;
    }

    public void setXiEventMask(Window window, int deviceId, byte[] mask) {
        ArrayMap<Integer, byte[]> masks = xiEventMasks.get(window);
        if (mask.length == 0) {
            if (masks == null) return;
            masks.remove(deviceId);
            if (masks.isEmpty()) xiEventMasks.remove(window);
            return;
        }
        if (masks == null) {
            masks = new ArrayMap<>();
            xiEventMasks.put(window, masks);
        }
        masks.put(deviceId, mask.clone());
    }

    public ArrayMap<Integer, byte[]> getXiEventMasks(Window window) {
        ArrayMap<Integer, byte[]> result = new ArrayMap<>();
        ArrayMap<Integer, byte[]> masks = xiEventMasks.get(window);
        if (masks == null) return result;
        for (int i = 0; i < masks.size(); i++) {
            result.put(masks.keyAt(i), masks.valueAt(i).clone());
        }
        return result;
    }

    public boolean isXiEventSelected(Window window, int deviceId, int eventType) {
        ArrayMap<Integer, byte[]> masks = xiEventMasks.get(window);
        if (masks == null) return false;
        return xiMaskContains(masks.get(0), eventType)
                || xiMaskContains(masks.get(1), eventType)
                || xiMaskContains(masks.get(deviceId), eventType);
    }

    private static boolean xiMaskContains(byte[] mask, int eventType) {
        int index = eventType >> 3;
        return mask != null && index < mask.length
                && (mask[index] & (1 << (eventType & 7))) != 0;
    }

    public void setRandrEventMask(Window window, int eventMask) {
        if (eventMask == 0) randrEventMasks.remove(window);
        else randrEventMasks.put(window, eventMask);
    }

    public int getRandrEventMask(Window window) {
        Integer eventMask = randrEventMasks.get(window);
        return eventMask != null ? eventMask : 0;
    }

    public int getRemainingRequestLength() {
        int actualLength = initialLength - inputStream.available();
        return requestLength - actualLength;
    }

    public void skipRequest() {
        inputStream.skip(getRemainingRequestLength());
    }

    public XInputStream getInputStream() {
        return inputStream;
    }

    public XOutputStream getOutputStream() {
        return outputStream;
    }

    public Bitmask getEventMaskForWindow(Window window) {
        EventListener eventListener = eventListeners.get(window);
        return eventListener != null ? eventListener.eventMask : new Bitmask();
    }

    @Override
    public void onFreeResource(XResource resource) {
        if (resource instanceof Window) {
            eventListeners.remove(resource);
            xiEventMasks.remove(resource);
            randrEventMasks.remove(resource);
            saveSet.remove(resource);
        }
        resources.remove(resource);
    }

    public boolean isValidResourceId(int id) {
        return xServer.resourceIDs.isInInterval(id, resourceIDBase);
    }

    public boolean openFont(int id, String name) {
        if (openFonts.containsKey(id)) return false;
        openFonts.put(id, name);
        return true;
    }

    public boolean closeFont(int id) {
        return openFonts.remove(id) != null;
    }

    public boolean hasOpenFont(int id) {
        return openFonts.containsKey(id);
    }

    public String getOpenFontName(int id) {
        return openFonts.get(id);
    }

    public boolean changeSaveSet(Window window, boolean insert) {
        if (window.originClient == this) return false;
        if (insert) {
            if (saveSet.contains(window)) return false;
            saveSet.add(window);
        }
        else {
            if (!saveSet.remove(window)) return false;
        }
        return true;
    }

    private void restoreSaveSet() {
        for (Window window : new ArrayList<>(saveSet)) {
            if (xServer.windowManager.getWindow(window.id) != window) continue;
            short rootX = window.getRootX();
            short rootY = window.getRootY();
            Window oldParent = window.getParent();
            Window parent = oldParent;
            while (parent != null && parent.originClient == this)
                parent = parent.getParent();
            if (parent == null) parent = xServer.windowManager.rootWindow;
            short x = (short)(rootX - parent.getRootX());
            short y = (short)(rootY - parent.getRootY());
            try {
                xServer.windowManager.reparentWindow(window, parent, x, y, this);
            } catch (XRequestError ignored) {
                continue;
            }
            xServer.windowManager.mapWindow(window, this);
        }
        saveSet.clear();
    }

    public void addOnDestroyListener(Callback<XClient> onDestroyListener) {
        if (!onDestroyListeners.contains(onDestroyListener)) onDestroyListeners.add(onDestroyListener);
    }

    public void removeOnWindowModificationListener(Callback<XClient> onDestroyListener) {
        onDestroyListeners.remove(onDestroyListener);
    }

    @Override
    public void destroy() {
        super.destroy();

        for (Callback<XClient> onDestroyListener : onDestroyListeners) onDestroyListener.call(this);
    }
}
