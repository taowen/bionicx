package com.winlator.xserver.errors;

public class BadFont extends XRequestError {
    public BadFont(int data) {
        super(7, data);
    }
}
