package com.winlator.renderer;

import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.opengl.GLES30;

import com.winlator.xserver.Drawable;

import java.nio.ByteBuffer;

public class Texture {
    protected int textureId = 0;
    protected int framebuffer = 0;
    protected int wrapS = GLES20.GL_CLAMP_TO_EDGE;
    protected int wrapT = GLES20.GL_CLAMP_TO_EDGE;
    protected int magFilter = GLES20.GL_LINEAR;
    protected int minFilter = GLES20.GL_LINEAR;
    protected int format = GLES11Ext.GL_BGRA;
    protected boolean needsUpdate = true;
    private boolean flipY = false;
    protected Drawable owner;

    public Texture(Drawable owner) {
        this.owner = owner;
    }

    protected void generateTextureId() {
        int[] textureIds = new int[1];
        GLES20.glGenTextures(1, textureIds, 0);
        textureId = textureIds[0];
    }

    protected void setTextureParameters() {
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, wrapS);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, wrapT);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, magFilter);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, minFilter);
    }

    public void allocateTexture(short width, short height, ByteBuffer data) {
        generateTextureId();

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, format, width, height, 0,
                format, GLES20.GL_UNSIGNED_BYTE, data);
        setTextureParameters();
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
    }

    public void adoptTextureId(int textureId) {
        this.textureId = textureId;
    }

    public int getFramebuffer() {
        return framebuffer;
    }

    public boolean ensureFramebuffer() {
        if (!isAllocated()) return false;
        if (framebuffer != 0) return true;
        int[] framebuffers = new int[1];
        GLES20.glGenFramebuffers(1, framebuffers, 0);
        framebuffer = framebuffers[0];
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, framebuffer);
        GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER,
                GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, textureId, 0);
        int status = GLES20.glCheckFramebufferStatus(GLES20.GL_FRAMEBUFFER);
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
        if (status == GLES20.GL_FRAMEBUFFER_COMPLETE) return true;
        int[] delete = new int[] {framebuffer};
        GLES20.glDeleteFramebuffers(1, delete, 0);
        framebuffer = 0;
        return false;
    }

    public Drawable getOwner() {
        return owner;
    }

    public void setOwner(Drawable owner) {
        this.owner = owner;
    }

    public boolean isFlipY() {
        return flipY;
    }

    public void setFlipY(boolean flipY) {
        this.flipY = flipY;
    }

    public int getWrapS() {
        return wrapS;
    }

    public void setWrapS(int wrapS) {
        this.wrapS = wrapS;
    }

    public int getWrapT() {
        return wrapT;
    }

    public void setWrapT(int wrapT) {
        this.wrapT = wrapT;
    }

    public int getMagFilter() {
        return magFilter;
    }

    public void setMagFilter(int magFilter) {
        this.magFilter = magFilter;
    }

    public int getMinFilter() {
        return minFilter;
    }

    public void setMinFilter(int minFilter) {
        this.minFilter = minFilter;
    }

    public int getFormat() {
        return format;
    }

    public void setFormat(int format) {
        this.format = format;
    }

    public boolean isNeedsUpdate() {
        return needsUpdate;
    }

    public void setNeedsUpdate(boolean needsUpdate) {
        this.needsUpdate = needsUpdate;
    }

    public void updateFromDrawable() {
        if (owner == null || owner.getData() == null) return;

        ByteBuffer data = owner.getData();
        if (!isAllocated()) {
            allocateTexture(owner.width, owner.height, data);
            owner.clearDirty();
            needsUpdate = false;
            return;
        }
        if (!needsUpdate) return;

        int[] dirty = owner.takeDirtyRect();
        int x = 0;
        int y = 0;
        int width = owner.width;
        int height = owner.height;
        if (dirty != null) {
            x = dirty[0];
            y = dirty[1];
            width = dirty[2];
            height = dirty[3];
        }
        if (width <= 0 || height <= 0) {
            needsUpdate = false;
            return;
        }

        data.position(0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 4);
        if (x == 0 && y == 0 && width == owner.width && height == owner.height) {
            GLES20.glTexSubImage2D(GLES20.GL_TEXTURE_2D, 0, 0, 0, width, height,
                    format, GLES20.GL_UNSIGNED_BYTE, data);
        }
        else {
            GLES20.glPixelStorei(GLES30.GL_UNPACK_ROW_LENGTH, owner.getImageStride());
            GLES20.glPixelStorei(GLES30.GL_UNPACK_SKIP_ROWS, y);
            GLES20.glPixelStorei(GLES30.GL_UNPACK_SKIP_PIXELS, x);
            GLES20.glTexSubImage2D(GLES20.GL_TEXTURE_2D, 0, x, y, width, height,
                    format, GLES20.GL_UNSIGNED_BYTE, data);
            GLES20.glPixelStorei(GLES30.GL_UNPACK_ROW_LENGTH, 0);
            GLES20.glPixelStorei(GLES30.GL_UNPACK_SKIP_ROWS, 0);
            GLES20.glPixelStorei(GLES30.GL_UNPACK_SKIP_PIXELS, 0);
        }
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        data.rewind();
        needsUpdate = false;
    }

    public boolean isAllocated() {
        return textureId > 0;
    }

    public int getTextureId() {
        return textureId;
    }

    public void copyFromReadBuffer(short width, short height) {
        if (!isAllocated()) allocateTexture(width, height, null);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        GLES20.glCopyTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, 0, 0, width, height, 0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        GLES20.glFlush();
    }

    public void destroy() {
        if (framebuffer != 0) {
            int[] framebuffers = new int[]{framebuffer};
            GLES20.glDeleteFramebuffers(1, framebuffers, 0);
            framebuffer = 0;
        }
        if (textureId > 0) {
            int[] textureIds = new int[]{textureId};
            GLES20.glDeleteTextures(textureIds.length, textureIds, 0);
            textureId = 0;
        }
    }
}
