package com.winlator.renderer;

import android.opengl.GLES20;
import android.opengl.GLES30;
import android.util.Log;

import com.winlator.renderer.material.CompositeMaterial;
import com.winlator.xserver.Drawable;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.IdentityHashMap;
import java.util.List;

/**
 * Glamor-style Render Composite: Porter-Duff Over and A8 glyphs on GLES.
 * Destination CPU heap stays the GetImage cache via glReadPixels. Large
 * destinations may attach an AHardwareBuffer EGLImage after that cache is
 * filled; Mali will not see GPU writes in a permanently locked mapping.
 */
public class RenderComposite {
    private static final String TAG = "BionicXRenderGL";
    static final int AHB_MIN_PIXELS = 256 * 256;
    public static final int GPU_MIN_PIXELS = 64 * 64;
    private static boolean loggedEglImage;
    private static boolean loggedFramebuffer;

    private final CompositeMaterial material = new CompositeMaterial();
    private final VertexAttribute quadVertices;
    private final Texture scratch = new Texture(null);
    private int scratchWidth;
    private int scratchHeight;
    private ByteBuffer readback;
    private final IdentityHashMap<byte[], Texture> glyphTextures =
            new IdentityHashMap<>();

    public RenderComposite(VertexAttribute quadVertices) {
        this.quadVertices = quadVertices;
    }

    public static boolean shouldUseAhb(Drawable drawable) {
        /* Mali will not sample CPU stores into an AHB that is still bound
         * as an EGLImage FBO. Keep the heap+glReadPixels cache; bind
         * EGLImage only after that path is the GetImage source of truth. */
        return false;
    }

    public boolean over(Drawable source, boolean sourceRepeat, int sourceX,
                        int sourceY, Drawable mask, int maskX, int maskY,
                        boolean maskIsA8, Drawable destination, int destX,
                        int destY, int width, int height, Integer solidArgb,
                        List<int[]> clips) {
        if (destination == null || destination.getData() == null
                || clips == null || clips.isEmpty())
            return false;
        Texture destTexture = destination.getTexture();
        if (destTexture == null) return false;
        synchronized (destination.renderLock) {
            destTexture.updateFromDrawable();
            if (!destTexture.ensureFramebuffer()) {
                logFramebufferOnce();
                return false;
            }
            if (source != null && source != destination) {
                Texture srcTexture = source.getTexture();
                if (srcTexture == null) return false;
                srcTexture.updateFromDrawable();
            }
            if (mask != null && mask != destination) {
                Texture maskTexture = mask.getTexture();
                if (maskTexture == null) return false;
                maskTexture.updateFromDrawable();
            }
            if (!blitDestinationToScratch(destTexture, destination.width,
                    destination.height))
                return false;
            material.use();
            quadVertices.bind(material.programId);
            GLES20.glDisable(GLES20.GL_BLEND);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,
                    destTexture.getFramebuffer());
            GLES20.glViewport(0, 0, destination.width, destination.height);
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, scratch.getTextureId());
            material.setUniformInt(material.uniforms.dstTexture, 0);
            boolean solid = solidArgb != null;
            material.setUniformInt(material.uniforms.solidSrc, solid ? 1 : 0);
            material.setUniformInt(material.uniforms.hasSrc,
                    !solid && source != null ? 1 : 0);
            if (solid) setSolidColor(solidArgb);
            if (!solid && source != null) {
                GLES20.glActiveTexture(GLES20.GL_TEXTURE1);
                int srcId = source == destination
                        ? scratch.getTextureId()
                        : source.getTexture().getTextureId();
                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, srcId);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
                        GLES20.GL_TEXTURE_WRAP_S, sourceRepeat
                                ? GLES20.GL_REPEAT : GLES20.GL_CLAMP_TO_EDGE);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
                        GLES20.GL_TEXTURE_WRAP_T, sourceRepeat
                                ? GLES20.GL_REPEAT : GLES20.GL_CLAMP_TO_EDGE);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
                        GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
                        GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_NEAREST);
                material.setUniformInt(material.uniforms.srcTexture, 1);
                material.setUniformVec2(material.uniforms.srcSize,
                        source.width, source.height);
            }
            else {
                material.setUniformVec2(material.uniforms.srcSize, 1, 1);
            }
            material.setUniformInt(material.uniforms.srcRepeat,
                    sourceRepeat ? 1 : 0);
            boolean hasMask = mask != null;
            material.setUniformInt(material.uniforms.hasMask, hasMask ? 1 : 0);
            if (hasMask) {
                GLES20.glActiveTexture(GLES20.GL_TEXTURE2);
                int maskId = mask == destination
                        ? scratch.getTextureId()
                        : mask.getTexture().getTextureId();
                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, maskId);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
                        GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
                        GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_NEAREST);
                material.setUniformInt(material.uniforms.maskTexture, 2);
                material.setUniformVec2(material.uniforms.maskSize,
                        mask.width, mask.height);
                material.setUniformInt(material.uniforms.maskChannel,
                        maskIsA8 ? 2 : 0);
            }
            else {
                material.setUniformVec2(material.uniforms.maskSize, 1, 1);
                material.setUniformInt(material.uniforms.maskChannel, 0);
            }
            material.setUniformVec2(material.uniforms.destSize,
                    destination.width, destination.height);
            int dirtyLeft = destination.width;
            int dirtyTop = destination.height;
            int dirtyRight = 0;
            int dirtyBottom = 0;
            for (int[] clip : clips) {
                int x = clip[0];
                int y = clip[1];
                int w = clip[2];
                int h = clip[3];
                if (w <= 0 || h <= 0) continue;
                int srcOffX = sourceX + (x - destX);
                int srcOffY = sourceY + (y - destY);
                int maskOffX = maskX + (x - destX);
                int maskOffY = maskY + (y - destY);
                material.setUniformVec4(material.uniforms.destRect,
                        x, y, w, h);
                material.setUniformVec4(material.uniforms.srcRect,
                        srcOffX, srcOffY, w, h);
                material.setUniformVec4(material.uniforms.maskRect,
                        maskOffX, maskOffY, w, h);
                GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
                copyDestRectToScratch(x, y, w, h);
                dirtyLeft = Math.min(dirtyLeft, x);
                dirtyTop = Math.min(dirtyTop, y);
                dirtyRight = Math.max(dirtyRight, x + w);
                dirtyBottom = Math.max(dirtyBottom, y + h);
            }
            quadVertices.disable();
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            boolean downloaded = dirtyRight > dirtyLeft && dirtyBottom > dirtyTop
                    && download(destination, dirtyLeft, dirtyTop,
                    dirtyRight - dirtyLeft, dirtyBottom - dirtyTop);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
            GLES20.glEnable(GLES20.GL_BLEND);
            GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA,
                    GLES20.GL_ONE_MINUS_SRC_ALPHA);
            if (!downloaded) return false;
            destination.markGpuPixelsCurrent(dirtyLeft, dirtyTop,
                    dirtyRight - dirtyLeft, dirtyBottom - dirtyTop);
            return true;
        }
    }

    public boolean glyphs(Drawable destination, int color, List<GlyphQuad> quads) {
        if (destination == null || destination.getData() == null
                || quads == null || quads.isEmpty())
            return false;
        Texture destTexture = destination.getTexture();
        if (destTexture == null) return false;
        synchronized (destination.renderLock) {
            destTexture.updateFromDrawable();
            if (!destTexture.ensureFramebuffer()) return false;
            if (!blitDestinationToScratch(destTexture, destination.width,
                    destination.height))
                return false;
            material.use();
            quadVertices.bind(material.programId);
            GLES20.glDisable(GLES20.GL_BLEND);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,
                    destTexture.getFramebuffer());
            GLES20.glViewport(0, 0, destination.width, destination.height);
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, scratch.getTextureId());
            material.setUniformInt(material.uniforms.dstTexture, 0);
            material.setUniformInt(material.uniforms.solidSrc, 1);
            material.setUniformInt(material.uniforms.hasSrc, 0);
            material.setUniformInt(material.uniforms.hasMask, 1);
            material.setUniformInt(material.uniforms.srcRepeat, 0);
            material.setUniformInt(material.uniforms.maskChannel, 1);
            setSolidColor(color);
            material.setUniformVec2(material.uniforms.srcSize, 1, 1);
            material.setUniformVec2(material.uniforms.destSize,
                    destination.width, destination.height);
            material.setUniformVec4(material.uniforms.srcRect, 0, 0, 1, 1);
            int dirtyLeft = destination.width;
            int dirtyTop = destination.height;
            int dirtyRight = 0;
            int dirtyBottom = 0;
            for (GlyphQuad quad : quads) {
                Texture glyph = glyphTexture(quad);
                if (glyph == null) continue;
                GLES20.glActiveTexture(GLES20.GL_TEXTURE2);
                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, glyph.getTextureId());
                material.setUniformInt(material.uniforms.maskTexture, 2);
                material.setUniformVec2(material.uniforms.maskSize,
                        quad.glyphWidth, quad.glyphHeight);
                material.setUniformVec4(material.uniforms.destRect,
                        quad.x, quad.y, quad.width, quad.height);
                material.setUniformVec4(material.uniforms.maskRect,
                        quad.maskX, quad.maskY, quad.width, quad.height);
                GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
                copyDestRectToScratch(quad.x, quad.y, quad.width, quad.height);
                dirtyLeft = Math.min(dirtyLeft, quad.x);
                dirtyTop = Math.min(dirtyTop, quad.y);
                dirtyRight = Math.max(dirtyRight, quad.x + quad.width);
                dirtyBottom = Math.max(dirtyBottom, quad.y + quad.height);
            }
            quadVertices.disable();
            boolean downloaded = dirtyRight > dirtyLeft && dirtyBottom > dirtyTop
                    && download(destination, dirtyLeft, dirtyTop,
                    dirtyRight - dirtyLeft, dirtyBottom - dirtyTop);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
            GLES20.glEnable(GLES20.GL_BLEND);
            GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA,
                    GLES20.GL_ONE_MINUS_SRC_ALPHA);
            if (!downloaded) return false;
            destination.markGpuPixelsCurrent(dirtyLeft, dirtyTop,
                    dirtyRight - dirtyLeft, dirtyBottom - dirtyTop);
            return true;
        }
    }

    private void setSolidColor(int argb) {
        float inv = 1.0f / 255.0f;
        material.setUniformVec4(material.uniforms.solidColor,
                ((argb >>> 16) & 0xff) * inv,
                ((argb >>> 8) & 0xff) * inv,
                (argb & 0xff) * inv,
                ((argb >>> 24) & 0xff) * inv);
    }

    private boolean blitDestinationToScratch(Texture destTexture, int width,
                                             int height) {
        if (scratchWidth != width || scratchHeight != height
                || !scratch.isAllocated()) {
            if (scratch.isAllocated()) scratch.destroy();
            scratch.allocateTexture((short)width, (short)height, null);
            scratchWidth = width;
            scratchHeight = height;
        }
        if (!scratch.ensureFramebuffer()) return false;
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,
                scratch.getFramebuffer());
        GLES20.glViewport(0, 0, width, height);
        GLES20.glDisable(GLES20.GL_BLEND);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, destTexture.getTextureId());
        material.use();
        quadVertices.bind(material.programId);
        material.setUniformInt(material.uniforms.dstTexture, 0);
        material.setUniformInt(material.uniforms.hasSrc, 0);
        material.setUniformInt(material.uniforms.hasMask, 0);
        material.setUniformInt(material.uniforms.solidSrc, 1);
        material.setUniformVec4(material.uniforms.solidColor, 0, 0, 0, 0);
        material.setUniformVec2(material.uniforms.destSize, width, height);
        material.setUniformVec4(material.uniforms.destRect, 0, 0, width, height);
        material.setUniformVec2(material.uniforms.srcSize, 1, 1);
        material.setUniformVec2(material.uniforms.maskSize, 1, 1);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        quadVertices.disable();
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
        return true;
    }

    private void copyDestRectToScratch(int x, int y, int width, int height) {
        if (width <= 0 || height <= 0 || !scratch.isAllocated()) return;
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, scratch.getTextureId());
        GLES20.glCopyTexSubImage2D(GLES20.GL_TEXTURE_2D, 0, x, y, x, y,
                width, height);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
    }

    private boolean download(Drawable destination, int x, int y, int width,
                             int height) {
        if (x < 0) {
            width += x;
            x = 0;
        }
        if (y < 0) {
            height += y;
            y = 0;
        }
        if (x + width > destination.width) width = destination.width - x;
        if (y + height > destination.height) height = destination.height - y;
        if (width <= 0 || height <= 0) return false;
        int bytes = width * height * 4;
        if (readback == null || readback.capacity() < bytes) {
            readback = ByteBuffer.allocateDirect(bytes)
                    .order(ByteOrder.LITTLE_ENDIAN);
        }
        readback.position(0);
        readback.limit(bytes);
        GLES20.glFinish();
        GLES20.glPixelStorei(GLES20.GL_PACK_ALIGNMENT, 4);
        GLES20.glReadPixels(x, y, width, height, GLES20.GL_RGBA,
                GLES20.GL_UNSIGNED_BYTE, readback);
        ByteBuffer dest = destination.getData();
        if (dest == null) return false;
        int stride = destination.getImageStride();
        IntBuffer pixels = readback.order(ByteOrder.LITTLE_ENDIAN).asIntBuffer();
        for (int row = 0; row < height; row++) {
            int destRow = ((y + row) * stride + x) * 4;
            for (int column = 0; column < width; column++) {
                int rgba = pixels.get(row * width + column);
                int argb = (rgba & 0xff00ff00)
                        | ((rgba & 0x000000ff) << 16)
                        | ((rgba & 0x00ff0000) >> 16);
                dest.putInt(destRow + column * 4, argb);
            }
        }
        dest.rewind();
        return true;
    }

    private Texture glyphTexture(GlyphQuad quad) {
        Texture texture = glyphTextures.get(quad.alpha);
        if (texture != null && texture.isAllocated()) return texture;
        ByteBuffer buffer = ByteBuffer.allocateDirect(quad.alpha.length);
        buffer.put(quad.alpha);
        buffer.position(0);
        texture = new Texture(null);
        texture.setFormat(GLES30.GL_R8);
        int[] ids = new int[1];
        GLES20.glGenTextures(1, ids, 0);
        texture.adoptTextureId(ids[0]);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, ids[0]);
        GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
        GLES30.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES30.GL_R8,
                quad.glyphWidth, quad.glyphHeight, 0, GLES30.GL_RED,
                GLES20.GL_UNSIGNED_BYTE, buffer);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
                GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
                GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_NEAREST);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
                GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
                GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        glyphTextures.put(quad.alpha, texture);
        return texture;
    }

    private static void logFramebufferOnce() {
        if (loggedFramebuffer) return;
        loggedFramebuffer = true;
        Log.w(TAG, "Render dest FBO incomplete; staying on CPU Composite");
    }

    static void logEglImage(boolean bound) {
        if (loggedEglImage) return;
        loggedEglImage = true;
        Log.i(TAG, bound
                ? "Render dest bound as EGLImage; GetImage uses glReadPixels cache"
                : "Render dest EGLImage bind failed; using GL texture FBO");
    }

    public static final class GlyphQuad {
        public final byte[] alpha;
        public final int glyphWidth;
        public final int glyphHeight;
        public final int x;
        public final int y;
        public final int width;
        public final int height;
        public final int maskX;
        public final int maskY;

        public GlyphQuad(byte[] alpha, int glyphWidth, int glyphHeight,
                         int x, int y, int width, int height,
                         int maskX, int maskY) {
            this.alpha = alpha;
            this.glyphWidth = glyphWidth;
            this.glyphHeight = glyphHeight;
            this.x = x;
            this.y = y;
            this.width = width;
            this.height = height;
            this.maskX = maskX;
            this.maskY = maskY;
        }
    }
}
