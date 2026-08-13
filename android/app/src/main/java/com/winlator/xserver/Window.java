package com.winlator.xserver;

import android.util.ArrayMap;
import android.util.SparseArray;

import com.winlator.core.Bitmask;
import com.winlator.renderer.FullscreenTransformation;
import com.winlator.xserver.events.Event;
import com.winlator.xserver.events.PropertyNotify;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Stack;

public class Window extends XResource {
    public static final int FLAG_X = 1<<0;
    public static final int FLAG_Y = 1<<1;
    public static final int FLAG_WIDTH = 1<<2;
    public static final int FLAG_HEIGHT = 1<<3;
    public static final int FLAG_BORDER_WIDTH = 1<<4;
    public static final int FLAG_SIBLING = 1<<5;
    public static final int FLAG_STACK_MODE = 1<<6;
    public enum StackMode {ABOVE, BELOW, TOP_IF, BOTTOM_IF, OPPOSITE}
    public enum MapState {UNMAPPED, UNVIEWABLE, VIEWABLE}
    public enum WMHints {FLAGS, INPUT, INITIAL_STATE, ICON_PIXMAP, ICON_WINDOW, ICON_X, ICON_Y, ICON_MASK, WINDOW_GROUP}
    public enum Type {NORMAL, DIALOG}
    private Drawable content;
    private short x;
    private short y;
    private short width;
    private short height;
    private short borderWidth;
    private ArrayMap<String, Object> tags;
    private Window parent;
    public final XClient originClient;
    public final WindowAttributes attributes = new WindowAttributes(this);
    private final SparseArray<Property> properties = new SparseArray<>();
    private final ArrayList<Window> children = new ArrayList<>();
    private final List<Window> immutableChildren = Collections.unmodifiableList(children);
    private final ArrayList<EventListener> eventListeners = new ArrayList<>();
    private FullscreenTransformation fullscreenTransformation;
    public static final int SHAPE_BOUNDING = 0;
    public static final int SHAPE_CLIP = 1;
    public static final int SHAPE_INPUT = 2;

    private volatile ShapeRegion boundingShape;
    private volatile ShapeRegion clipShape;
    private volatile ShapeRegion inputShape;

    public static final class ShapeRectangle {
        public final int x;
        public final int y;
        public final int width;
        public final int height;

        public ShapeRectangle(int x, int y, int width, int height) {
            this.x = x;
            this.y = y;
            this.width = width;
            this.height = height;
        }
    }

    private static final class ShapeRegion {
        private final ArrayList<ShapeRectangle> rectangles;
        private final int xOffset;
        private final int yOffset;

        private ShapeRegion(List<ShapeRectangle> rectangles, int xOffset, int yOffset) {
            this.rectangles = new ArrayList<>(rectangles);
            this.xOffset = xOffset;
            this.yOffset = yOffset;
        }

        private boolean contains(int x, int y) {
            for (ShapeRectangle rectangle : rectangles) {
                int left = xOffset + rectangle.x;
                int top = yOffset + rectangle.y;
                if (x >= left && y >= top && x < left + rectangle.width
                        && y < top + rectangle.height) return true;
            }
            return false;
        }
    }

    public Window(int id, Drawable content, int x, int y, int width, int height, XClient originClient) {
        super(id);
        this.content = content;
        this.x = (short)x;
        this.y = (short)y;
        this.width = (short)width;
        this.height = (short)height;
        this.originClient = originClient;
    }

    public short getX() {
        return x;
    }

    public void setX(short x) {
        this.x = x;
    }

    public short getY() {
        return y;
    }

    public void setY(short y) {
        this.y = y;
    }

    public short getWidth() {
        return width;
    }

    public void setWidth(short width) {
        this.width = width;
    }

    public short getHeight() {
        return height;
    }

    public void setHeight(short height) {
        this.height = height;
    }

    public short getBorderWidth() {
        return borderWidth;
    }

    public void setBorderWidth(short borderWidth) {
        this.borderWidth = borderWidth;
    }

    public void setTag(Object value) {
        setTag("tag", value);
    }

    public Object getTag() {
        return getTag("tag");
    }

    public void setTag(String key, Object value) {
        (tags == null ? (tags = new ArrayMap<>()) : tags).put(key, value);
    }

    public Object getTag(String key) {
        return getTag(key, null);
    }

    public Object getTag(String key, Object fallback) {
        return (tags == null ? (tags = new ArrayMap<>()) : tags).getOrDefault(key, fallback);
    }

    public void removeTag(String key) {
        if (tags != null) tags.remove(key);
    }

    public Drawable getContent() {
        return content;
    }

    public void setContent(Drawable content) {
        this.content = content;
    }

    public Window getParent() {
        return parent;
    }

    public Property getProperty(int id) {
        return properties.get(id);
    }

    public int[] getPropertyNames() {
        int[] names = new int[properties.size()];
        for (int i = 0; i < names.length; i++) names[i] = properties.keyAt(i);
        return names;
    }

    public void addProperty(Property property) {
        properties.put(property.name, property);
    }

    public void removeProperty(int id) {
        properties.remove(id);
        sendEvent(Event.PROPERTY_CHANGE, new PropertyNotify(this, id, true));
    }

    public Property modifyProperty(int atom, int type, Property.Format format, Property.Mode mode, byte[] data) {
        Property property = getProperty(atom);
        boolean modified = false;
        if (property == null) {
            addProperty((property = new Property(atom, type, format, data)));
            modified = true;
        }
        else if (mode == Property.Mode.REPLACE) {
            if (property.format == format) {
                property.replace(data);
            }
            else properties.put(atom, new Property(atom, type, format, data));
            modified = true;
        }
        else if (property.format == format && property.type == type) {
            if (mode == Property.Mode.PREPEND) {
                property.prepend(data);
            }
            else if (mode == Property.Mode.APPEND) {
                property.append(data);
            }
            modified = true;
        }

        if (modified) {
            sendEvent(Event.PROPERTY_CHANGE, new PropertyNotify(this, atom, false));
            return property;
        }
        else return null;
    }

    public String getName() {
        Property property = getProperty(Atom.WM_NAME);
        return property != null ? property.toString() : "";
    }

    public String getClassName() {
        Property property = getProperty(Atom.WM_CLASS);
        return property != null ? property.toString() : "";
    }

    public int getWMHintsValue(WMHints wmHints) {
        Property property = getProperty(Atom.WM_HINTS);
        return property != null ? property.getInt(wmHints.ordinal()) : 0;
    }

    public int getProcessId() {
        Property property = getProperty(Atom._NET_WM_PID);
        return property != null ? property.getInt(0) : 0;
    }

    public int getTransientFor() {
        Property property = getProperty(Atom.WM_TRANSIENT_FOR);
        return property != null ? property.getInt(0) : 0;
    }

    public boolean isWoW64() {
        Property property = getProperty(Atom._NET_WM_WOW64);
        return property != null && property.getByte(0) == 1;
    }

    public boolean isSurface() {
        Property property = getProperty(Atom._NET_WM_SURFACE);
        return property != null && property.getByte(0) == 1;
    }

    public boolean isDesktopWindow() {
        return getClassName().equals("explorer.exe");
    }

    public boolean isDialogBox() {
        return getType() == Type.DIALOG && getTransientFor() > 0 && hasDecoration(Decoration.TITLE) && !(hasDecoration(Decoration.MINIMIZE) && hasDecoration(Decoration.MAXIMIZE));
    }

    public Bitmask getDecorations() {
        Property property = getProperty(Atom._MOTIF_WM_HINTS);
        return new Bitmask(property != null ? property.getInt(2) : 0);
    }

    public boolean hasNoDecorations() {
        return getDecorations().isEmpty();
    }

    public boolean hasDecoration(Decoration decoration) {
        return getDecorations().isSet(decoration.flag());
    }

    public Type getType() {
        Property property = getProperty(Atom._NET_WM_WINDOW_TYPE);
        return property != null && property.toString().equals("_NET_WM_WINDOW_TYPE_DIALOG") ? Type.DIALOG : Type.NORMAL;
    }

    public long getHandle() {
        Property property = getProperty(Atom._NET_WM_HWND);
        return property != null ? property.getLong(0) : 0;
    }

    public boolean isApplicationWindow() {
        int windowGroup = getWMHintsValue(WMHints.WINDOW_GROUP);
        return isRenderable() && !getName().isEmpty() && windowGroup == id;
    }

    public boolean isInputOutput() {
        return content != null;
    }

    public void addChild(Window child) {
        if (child == null || child.parent == this) return;
        child.parent = this;
        children.add(child);
    }

    public void removeChild(Window child) {
        if (child == null || child.parent != this) return;
        child.parent = null;
        children.remove(child);
    }

    public Window previousSibling() {
        if (parent == null) return null;
        int index = parent.children.indexOf(this);
        return index > 0 ? parent.children.get(index-1) : null;
    }

    public Window nextSibling() {
        if (parent == null) return null;
        int index = parent.children.indexOf(this);
        return index >= 0 && (index+1) < parent.children.size() ? parent.children.get(index+1) : null;
    }

    public void moveChildAbove(Window child, Window sibling) {
        children.remove(child);
        if (sibling != null && children.contains(sibling)) {
            children.add(children.indexOf(sibling) + 1, child);
        }
        else children.add(child);
    }

    public void moveChildBelow(Window child, Window sibling) {
        children.remove(child);
        if (sibling != null && children.contains(sibling)) {
            children.add(children.indexOf(sibling), child);
        }
        else children.add(0, child);
    }

    public List<Window> getChildren() {
        return immutableChildren;
    }

    public int getChildCount() {
        return children.size();
    }

    public void addEventListener(EventListener eventListener) {
        eventListeners.add(eventListener);
    }

    public void removeEventListener(EventListener eventListener) {
        eventListeners.remove(eventListener);
    }

    public boolean hasEventListenerFor(int eventId) {
        for (EventListener eventListener : eventListeners) {
            if (eventListener.isInterestedIn(eventId)) return true;
        }
        return false;
    }

    public boolean hasEventListenerForOtherClient(int eventId, XClient client) {
        for (EventListener eventListener : eventListeners) {
            if (eventListener.client != client
                    && eventListener.isInterestedIn(eventId)) return true;
        }
        return false;
    }

    public boolean hasEventListenerFor(Bitmask mask) {
        for (EventListener eventListener : eventListeners) {
            if (eventListener.isInterestedIn(mask)) return true;
        }
        return false;
    }

    public void sendEvent(int eventId, Event event) {
        for (EventListener eventListener : eventListeners) {
            if (eventListener.isInterestedIn(eventId)) {
                eventListener.sendEvent(event);
            }
        }
    }

    public void sendEvent(Bitmask eventMask, Event event) {
        for (EventListener eventListener : eventListeners) {
            if (eventListener.isInterestedIn(eventMask)) {
                eventListener.sendEvent(event);
            }
        }
    }

    public void sendEvent(int eventId, Event event, XClient client) {
        for (EventListener eventListener : eventListeners) {
            if (eventListener.isInterestedIn(eventId) && eventListener.client == client) {
                eventListener.sendEvent(event);
            }
        }
    }

    public void sendEvent(Bitmask eventMask, Event event, XClient client) {
        for (EventListener eventListener : eventListeners) {
            if (eventListener.isInterestedIn(eventMask) && eventListener.client == client) {
                eventListener.sendEvent(event);
            }
        }
    }

    public void sendEvent(Event event) {
        for (EventListener eventListener : eventListeners) eventListener.sendEvent(event);
    }

    public boolean isRenderable() {
        return attributes.isMapped() && width > 1 && height > 1;
    }

    public boolean containsPoint(short rootX, short rootY) {
        return containsPoint(rootX, rootY, false);
    }

    public boolean containsPoint(short rootX, short rootY, boolean useFullscreenTransformation) {
        short[] localPoint = rootPointToLocal(rootX, rootY, useFullscreenTransformation);
        short width = fullscreenTransformation != null && useFullscreenTransformation ? fullscreenTransformation.width : this.width;
        short height = fullscreenTransformation != null && useFullscreenTransformation ? fullscreenTransformation.height : this.height;
        boolean inBounds = localPoint[0] >= 0 && localPoint[1] >= 0
                && localPoint[0] < width && localPoint[1] < height;
        return inBounds && (inputShape == null
                || inputShape.contains(localPoint[0], localPoint[1]));
    }

    public void setInputShape(List<ShapeRectangle> rectangles, int xOffset, int yOffset) {
        inputShape = rectangles == null ? null
                : new ShapeRegion(rectangles, xOffset, yOffset);
    }

    public void setWindowShape(int kind, List<ShapeRectangle> rectangles,
                               int xOffset, int yOffset) {
        ShapeRegion region = rectangles == null ? null
                : new ShapeRegion(rectangles, xOffset, yOffset);
        switch (kind) {
            case SHAPE_BOUNDING:
                boundingShape = region;
                break;
            case SHAPE_CLIP:
                clipShape = region;
                break;
            case SHAPE_INPUT:
                setInputShape(rectangles, xOffset, yOffset);
                break;
            default:
                break;
        }
    }

    public boolean isWindowShaped(int kind) {
        return shapeRegion(kind) != null;
    }

    public List<ShapeRectangle> copyWindowShape(int kind) {
        ShapeRegion region = shapeRegion(kind);
        if (region == null) return Collections.emptyList();
        ArrayList<ShapeRectangle> copied = new ArrayList<>(region.rectangles.size());
        for (ShapeRectangle rectangle : region.rectangles) {
            copied.add(new ShapeRectangle(region.xOffset + rectangle.x,
                    region.yOffset + rectangle.y, rectangle.width,
                    rectangle.height));
        }
        return copied;
    }

    public int[] getWindowShapeExtents(int kind) {
        ShapeRegion region = shapeRegion(kind);
        if (region == null) {
            if (kind == SHAPE_BOUNDING) {
                return new int[]{-borderWidth, -borderWidth,
                        width + 2 * borderWidth, height + 2 * borderWidth};
            }
            return new int[]{0, 0, width, height};
        }
        if (region.rectangles.isEmpty()) return new int[]{0, 0, 0, 0};
        int minX = Integer.MAX_VALUE;
        int minY = Integer.MAX_VALUE;
        int maxX = Integer.MIN_VALUE;
        int maxY = Integer.MIN_VALUE;
        for (ShapeRectangle rectangle : region.rectangles) {
            int left = region.xOffset + rectangle.x;
            int top = region.yOffset + rectangle.y;
            minX = Math.min(minX, left);
            minY = Math.min(minY, top);
            maxX = Math.max(maxX, left + rectangle.width);
            maxY = Math.max(maxY, top + rectangle.height);
        }
        return new int[]{minX, minY, maxX - minX, maxY - minY};
    }

    private ShapeRegion shapeRegion(int kind) {
        switch (kind) {
            case SHAPE_BOUNDING:
                return boundingShape;
            case SHAPE_CLIP:
                return clipShape;
            case SHAPE_INPUT:
                return inputShape;
            default:
                return null;
        }
    }

    public short[] rootPointToLocal(short x, short y) {
        return rootPointToLocal(x, y, false);
    }

    public short[] rootPointToLocal(short x, short y, boolean useFullscreenTransformation) {
        Window window = this;
        while (window != null) {
            x -= window.fullscreenTransformation != null && useFullscreenTransformation ? window.fullscreenTransformation.x : window.x;
            y -= window.fullscreenTransformation != null && useFullscreenTransformation ? window.fullscreenTransformation.y : window.y;
            window = window.parent;
        }
        return new short[]{x, y};
    }

    public short[] localPointToRoot(short x, short y) {
        return localPointToRoot(x, y, false);
    }

    public short[] localPointToRoot(short x, short y, boolean useFullscreenTransformation) {
        Window window = this;
        while (window != null) {
            x += window.fullscreenTransformation != null && useFullscreenTransformation ? window.fullscreenTransformation.x : window.x;
            y += window.fullscreenTransformation != null && useFullscreenTransformation ? window.fullscreenTransformation.y : window.y;
            window = window.parent;
        }
        return new short[]{x, y};
    }

    public short getRootX() {
        return getRootX(false);
    }

    public short getRootX(boolean useFullscreenTransformation) {
        short rootX = fullscreenTransformation != null && useFullscreenTransformation ? fullscreenTransformation.x : x;
        Window window = parent;
        while (window != null) {
            rootX += window.fullscreenTransformation != null && useFullscreenTransformation ? window.fullscreenTransformation.x : window.x;
            window = window.parent;
        }
        return rootX;
    }

    public short getRootY() {
        return getRootY(false);
    }

    public short getRootY(boolean useFullscreenTransformation) {
        short rootY = fullscreenTransformation != null && useFullscreenTransformation ? fullscreenTransformation.y : y;
        Window window = parent;
        while (window != null) {
            rootY += window.fullscreenTransformation != null && useFullscreenTransformation ? window.fullscreenTransformation.y : window.y;
            window = window.parent;
        }
        return rootY;
    }

    public Window getAncestorWithEventMask(Bitmask eventMask) {
        Window window = this;
        while (window != null) {
            if (window.hasEventListenerFor(eventMask)) return window;
            if (window.attributes.getDoNotPropagateMask().intersects(eventMask)) return null;
            window = window.parent;
        }
        return null;
    }

    public Window getAncestorWithEventId(int eventId) {
        return getAncestorWithEventId(eventId, null);
    }

    public Window getAncestorWithEventId(int eventId, Window endWindow) {
        Window window = this;
        while (window != null) {
            if (window.hasEventListenerFor(eventId)) return window;
            if (window == endWindow || window.attributes.getDoNotPropagateMask().isSet(eventId)) return null;
            window = window.parent;
        }
        return null;
    }

    public boolean isAncestorOf(Window window) {
        if (window == this) return false;
        while (window != null) {
            if (window == this) return true;
            window = window.parent;
        }
        return false;
    }

    public Window getChildByCoords(short x, short y) {
        return getChildByCoords(x, y, false);
    }

    public Window getChildByCoords(short x, short y, boolean useFullscreenTransformation) {
        for (int i = children.size()-1; i >= 0; i--) {
            Window child = children.get(i);
            if (child.attributes.isMapped() && child.containsPoint(x, y, useFullscreenTransformation)) return child;
        }
        return null;
    }

    public MapState getMapState() {
        if (!attributes.isMapped()) return MapState.UNMAPPED;
        Window window = this;
        do {
            window = window.parent;
            if (window == null) return MapState.VIEWABLE;
        }
        while (window.attributes.isMapped());
        return MapState.UNVIEWABLE;
    }

    public Bitmask getAllEventMasks() {
        Bitmask eventMask = new Bitmask();
        for (EventListener eventListener : eventListeners) eventMask.join(eventListener.eventMask);
        return eventMask;
    }

    public EventListener getButtonPressListener() {
        for (EventListener eventListener : eventListeners) {
            if (eventListener.isInterestedIn(Event.BUTTON_PRESS)) return eventListener;
        }
        return null;
    }

    public void disableAllDescendants() {
        Stack<Window> stack = new Stack<>();
        stack.push(this);
        while (!stack.isEmpty()) {
            Window window = stack.pop();
            window.attributes.setEnabled(false);
            stack.addAll(window.children);
        }
    }

    public String stringifyProperties() {
        String result = "";
        for (int i = 0; i < properties.size(); i++) {
            Property property = properties.valueAt(i);
            result += property.nameAsString()+"="+property+"\n";
        }
        return result;
    }

    public FullscreenTransformation getFullscreenTransformation() {
        return fullscreenTransformation;
    }

    public void setFullscreenTransformation(FullscreenTransformation fullscreenTransformation) {
        this.fullscreenTransformation = fullscreenTransformation;
    }

    public boolean isIconic() {
        final int iconicState = 3;
        return getWMHintsValue(WMHints.INITIAL_STATE) == iconicState || height <= 32;
    }

    public boolean isLayered() {
        final int WindowLayeredHint = 1<<16;
        return (getWMHintsValue(WMHints.FLAGS) & WindowLayeredHint) != 0;
    }
}
