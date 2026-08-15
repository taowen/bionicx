package com.winlator.xserver;

import com.winlator.core.Bitmask;
import com.winlator.math.Mathf;

import java.util.ArrayList;

public class Pointer {
    public enum Button {
        BUTTON_LEFT, BUTTON_MIDDLE, BUTTON_RIGHT, BUTTON_SCROLL_UP, BUTTON_SCROLL_DOWN, BUTTON_SCROLL_CLICK_LEFT, BUTTON_SCROLL_CLICK_RIGHT;

        public byte code() {
            return (byte)(ordinal() + 1);
        }

        public int flag() {
            return 1<<(code() + MAX_BUTTONS);
        }
    }
    public static final byte MAX_BUTTONS = 7;
    private final ArrayList<OnPointerMotionListener> onPointerMotionListeners = new ArrayList<>();
    private final Bitmask buttonMask = new Bitmask();
    private final XServer xServer;
    private short x;
    private short y;
    private int accelNumerator = 2;
    private int accelDenominator = 1;
    private int accelThreshold = 4;
    private byte[] buttonMap = {1, 2, 3};

    public interface OnPointerMotionListener {
        default void onPointerButtonPress(Button button) {}
        default void onPointerButtonRelease(Button button) {}
        default void onPointerMove(short x, short y) {}
    }

    public Pointer(XServer xServer) {
        this.xServer = xServer;
    }

    public void setX(int x) {
        this.x = (short)x;
    }

    public void setY(int y) {
        this.y = (short)y;
    }

    public short getX() {
        return x;
    }

    public short getY() {
        return y;
    }

    public short getClampedX() {
        return (short)Mathf.clamp(x, 0, xServer.screenInfo.width -1);
    }

    public short getClampedY() {
        return (short)Mathf.clamp(y, 0, xServer.screenInfo.height -1);
    }

    public void setPosition(int x, int y) {
        setX(x);
        setY(y);
        triggerOnPointerMove(this.x, this.y);
    }

    public int getAccelNumerator() {
        return accelNumerator;
    }

    public int getAccelDenominator() {
        return accelDenominator;
    }

    public int getAccelThreshold() {
        return accelThreshold;
    }

    public void setAccel(int numerator, int denominator, int threshold) {
        accelNumerator = numerator;
        accelDenominator = denominator;
        accelThreshold = threshold;
    }

    public byte[] getButtonMap() {
        return buttonMap.clone();
    }

    public boolean setButtonMap(byte[] map) {
        if (map == null || map.length != buttonMap.length) return false;
        buttonMap = map.clone();
        return true;
    }

    public Bitmask getButtonMask() {
        return buttonMask;
    }

    public void setButton(Button button, boolean pressed) {
        boolean oldPressed = isButtonPressed(button);
        buttonMask.set(button.flag(), pressed);
        if (oldPressed != pressed) {
            if (pressed) {
                triggerOnPointerButtonPress(button);
            }
            else triggerOnPointerButtonRelease(button);
        }
    }

    public boolean isButtonPressed(Button button) {
        return buttonMask.isSet(button.flag());
    }

    public void addOnPointerMotionListener(OnPointerMotionListener onPointerMotionListener) {
        onPointerMotionListeners.add(onPointerMotionListener);
    }

    public void removeOnPointerMotionListener(OnPointerMotionListener onPointerMotionListener) {
        onPointerMotionListeners.remove(onPointerMotionListener);
    }

    private void triggerOnPointerButtonPress(Button button) {
        for (int i = onPointerMotionListeners.size()-1; i >= 0; i--) {
            onPointerMotionListeners.get(i).onPointerButtonPress(button);
        }
    }

    private void triggerOnPointerButtonRelease(Button button) {
        for (int i = onPointerMotionListeners.size()-1; i >= 0; i--) {
            onPointerMotionListeners.get(i).onPointerButtonRelease(button);
        }
    }

    private void triggerOnPointerMove(short x, short y) {
        for (int i = onPointerMotionListeners.size()-1; i >= 0; i--) {
            onPointerMotionListeners.get(i).onPointerMove(x, y);
        }
    }
}
