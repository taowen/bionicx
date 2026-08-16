package com.winlator.xserver.errors;

public class BadFence extends XRequestError {
    public BadFence(int id) {
        super(com.winlator.xserver.extensions.SyncExtension.FIRST_ERROR + 2, id);
    }
}
