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
    private final boolean cpuAccess;
    private boolean preferEglImage;

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
        this.cpuAccess = cpuAccess;
        hardwareBufferPtr = createHardwareBuffer(owner.width, owner.height, cpuAccess, useHALPixelFormatBGRA8888);
        /* Do not keep the buffer locked. Mali GPU writes are invisible to a
         * mapping that stays locked, and GetImage then returns zeros. */
    }

    public void setPreferEglImage(boolean preferEglImage) {
        this.preferEglImage = preferEglImage;
    }

    public boolean bindEglImage() {
        if (imageKHRPtr != 0) return true;
        if (!preferEglImage || hardwareBufferPtr == 0 || textureId == 0)
            return false;
        unlockCpu();
        imageKHRPtr = createImageKHR(hardwareBufferPtr, textureId);
        RenderComposite.logEglImage(imageKHRPtr != 0);
        return imageKHRPtr != 0;
    }

    @Override
    public void allocateTexture(short width, short height, ByteBuffer data) {
        if (isAllocated()) return;
        generateTextureId();
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        if (preferEglImage && bindEglImage()) {
            setTextureParameters();
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            return;
        }
        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, format, width, height, 0,
                format, GLES20.GL_UNSIGNED_BYTE, null);
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
        if (staging != null) {
            upload(staging, owner.width, owner.height,
                    stride > 0 ? stride : owner.width, GLES20.GL_RGBA);
            needsUpdate = false;
            return;
        }
        if (imageKHRPtr != 0) {
            copyOwnerHeapToAhb();
            needsUpdate = false;
            owner.clearDirty();
            return;
        }
        ByteBuffer pixels = virtualData != null ? virtualData : owner.getData();
        if (pixels != null) {
            short row = virtualData != null && stride > 0 ? stride : owner.width;
            upload(pixels, owner.width, owner.height, row, format);
        }
        needsUpdate = false;
    }

    private void upload(ByteBuffer pixels, int width, int height, int rowLength,
                        int glFormat) {
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 4);
        if (rowLength > width)
            GLES20.glPixelStorei(GLES30.GL_UNPACK_ROW_LENGTH, rowLength);
        pixels.position(0);
        GLES20.glTexSubImage2D(GLES20.GL_TEXTURE_2D, 0, 0, 0, width, height,
                glFormat, GLES20.GL_UNSIGNED_BYTE, pixels);
        if (rowLength > width)
            GLES20.glPixelStorei(GLES30.GL_UNPACK_ROW_LENGTH, 0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
    }

    public void copyOwnerHeapToAhb() {
        ByteBuffer heap = owner != null ? owner.getData() : null;
        if (heap == null || hardwareBufferPtr == 0) return;
        ByteBuffer mapped = lockCpu();
        if (mapped == null) return;
        try {
            int width = owner.width;
            int height = owner.height;
            int srcStride = owner.getImageStride();
            int dstStride = stride > 0 ? stride : width;
            for (int row = 0; row < height; row++) {
                int src = row * srcStride * 4;
                int dst = row * dstStride * 4;
                for (int column = 0; column < width * 4; column++)
                    mapped.put(dst + column, heap.get(src + column));
            }
        }
        finally {
            unlockCpu();
        }
    }

    public ByteBuffer lockCpu() {
        if (hardwareBufferPtr == 0 || !cpuAccess) return null;
        if (!locked) {
            virtualData = lockHardwareBuffer(hardwareBufferPtr);
            locked = virtualData != null;
        }
        return virtualData;
    }

    public void unlockCpu() {
        if (!locked || hardwareBufferPtr == 0) return;
        unlockHardwareBuffer(hardwareBufferPtr);
        locked = false;
        virtualData = null;
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
        unlockCpu();
        destroyImageKHR(imageKHRPtr);
        destroyHardwareBuffer(hardwareBufferPtr, false);
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
