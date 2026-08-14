package com.winlator.renderer;

import android.opengl.GLES20;
import android.opengl.GLES30;

import androidx.annotation.Keep;

import com.winlator.xserver.Drawable;

import java.nio.ByteBuffer;

public class GPUImage extends Texture {
    private long hardwareBufferPtr;
    private long imageKHRPtr;
    private ByteBuffer virtualData;
    private ByteBuffer staging;
    private short stride;
    private boolean locked = false;
    private int nativeHandle;

    static {
        System.loadLibrary("winlator");
    }

    public GPUImage(Drawable owner) {
        this(owner, true, true);
    }

    public GPUImage(Drawable owner, boolean cpuAccess) {
        this(owner, cpuAccess, true);
    }

    public GPUImage(Drawable owner, boolean cpuAccess, boolean useHALPixelFormatBGRA8888) {
        super(owner);
        hardwareBufferPtr = createHardwareBuffer(owner.width, owner.height, cpuAccess, useHALPixelFormatBGRA8888);
        if (cpuAccess && hardwareBufferPtr != 0) {
            virtualData = lockHardwareBuffer(hardwareBufferPtr);
            locked = true;
        }
    }

    @Override
    public void allocateTexture(short width, short height, ByteBuffer data) {
        if (isAllocated()) return;
        generateTextureId();
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, width, height, 0,
                GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);
        setTextureParameters();
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
    }

    @Override
    public void updateFromDrawable() {
        if (owner == null || hardwareBufferPtr == 0) return;
        if (!isAllocated()) allocateTexture(owner.width, owner.height, null);
        /* Vulkan present publishes a host-visible copy into staging.
         * Never lock the window AHB from the GL thread. */
        if (!needsUpdate) return;
        ByteBuffer pixels = staging != null ? staging : virtualData;
        if (pixels != null) {
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
            GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 4);
            if (stride > owner.width)
                GLES20.glPixelStorei(GLES30.GL_UNPACK_ROW_LENGTH, stride);
            pixels.position(0);
            GLES20.glTexSubImage2D(GLES20.GL_TEXTURE_2D, 0, 0, 0, owner.width, owner.height,
                    GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, pixels);
            if (stride > owner.width)
                GLES20.glPixelStorei(GLES30.GL_UNPACK_ROW_LENGTH, 0);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        }
        needsUpdate = false;
    }

    public void publishPixels(ByteBuffer pixels, int width, int height) {
        if (pixels == null) return;
        int size = width * height * 4;
        if (size <= 0 || pixels.capacity() < size) return;
        if (staging == null || staging.capacity() < size) {
            staging = ByteBuffer.allocateDirect(size);
        }
        pixels.position(0);
        pixels.limit(size);
        staging.clear();
        staging.limit(size);
        staging.put(pixels);
        staging.flip();
        stride = (short)width;
    }

    public short getStride() {
        return stride;
    }

    @Keep
    private void setStride(short stride) {
        this.stride = stride;
    }

    public int getNativeHandle() {
        return nativeHandle;
    }

    @Keep
    private void setNativeHandle(int nativeHandle) {
        this.nativeHandle = nativeHandle;
    }

    public ByteBuffer getVirtualData() {
        return virtualData;
    }

    @Override
    public void destroy() {
        destroyImageKHR(imageKHRPtr);
        destroyHardwareBuffer(hardwareBufferPtr, locked);
        virtualData = null;
        imageKHRPtr = 0;
        hardwareBufferPtr = 0;
        super.destroy();
    }

    public long getHardwareBufferPtr() {
        return hardwareBufferPtr;
    }

    private native long createHardwareBuffer(short width, short height, boolean cpuAccess, boolean useHALPixelFormatBGRA8888);

    private native void destroyHardwareBuffer(long hardwareBufferPtr, boolean locked);

    private native ByteBuffer lockHardwareBuffer(long hardwareBufferPtr);

    private native void unlockHardwareBuffer(long hardwareBufferPtr);

    private native long createImageKHR(long hardwareBufferPtr, int textureId);

    private native void destroyImageKHR(long imageKHRPtr);
}