package com.winlator.xserver;

import androidx.collection.ArrayMap;

import com.winlator.core.Bitmask;
import com.winlator.core.Callback;
import com.winlator.xconnector.ConnectedClient;
import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
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
    private final ArrayList<XResource> resources = new ArrayList<>();
    private final ArrayList<Callback<XClient>> onDestroyListeners = new ArrayList<>();

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

    public void sendEvent(Event event) {
        try {
            event.send(sequenceNumber, outputStream);
        }
        catch (IOException e) {
            e.printStackTrace();
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
        }
        resources.remove(resource);
    }

    public boolean isValidResourceId(int id) {
        return xServer.resourceIDs.isInInterval(id, resourceIDBase);
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
