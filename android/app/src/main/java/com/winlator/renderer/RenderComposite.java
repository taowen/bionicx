package com.winlator.renderer;

import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.opengl.GLES30;
import android.util.Log;

import com.winlator.renderer.material.CompositeMaterial;
import com.winlator.xserver.Drawable;

import java.nio.ByteBuffer;
import java.util.IdentityHashMap;
import java.util.List;

/**
 * Glamor-style Render: GLES texture is the 32-bit drawable.
 * Fill/copy/Porter-Duff/glyphs run here. GetImage downloads on demand.
 */
public class RenderComposite {
    private static final String TAG = "BionicXRenderGL";
    static final int AHB_MIN_PIXELS = 256 * 256;
    private static boolean loggedEglImage;
    private static boolean loggedFramebuffer;

    private final CompositeMaterial textureMaterial = new CompositeMaterial();
    private CompositeMaterial fetchMaterial;
    private boolean fetchChecked;
    private CompositeMaterial material = textureMaterial;
    private final VertexAttribute quadVertices;
    private final Texture scratch = new Texture(null);
    private int scratchWidth;
    private int scratchHeight;
    private final IdentityHashMap<byte[], Texture> glyphTextures =
            new IdentityHashMap<>();

    public RenderComposite(VertexAttribute quadVertices) {
        this.quadVertices = quadVertices;
    }

    private CompositeMaterial activeShader() {
        if (!fetchChecked) {
            fetchChecked = true;
            String extensions = GLES20.glGetString(GLES20.GL_EXTENSIONS);
            if (extensions == null) extensions = "";
            CompositeMaterial.Fetch fetch = CompositeMaterial.Fetch.TEXTURE;
            if (extensions.contains("GL_EXT_shader_framebuffer_fetch"))
                fetch = CompositeMaterial.Fetch.EXT;
            else if (extensions.contains("GL_ARM_shader_framebuffer_fetch"))
                fetch = CompositeMaterial.Fetch.ARM;
            if (fetch != CompositeMaterial.Fetch.TEXTURE) {
                try {
                    fetchMaterial = new CompositeMaterial(fetch);
                    fetchMaterial.use();
                }
                catch (RuntimeException ignored) {
                    fetchMaterial = null;
                }
            }
        }
        material = fetchMaterial != null ? fetchMaterial : textureMaterial;
        return material;
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
        return composite(3, source, sourceRepeat, sourceX, sourceY, mask,
                maskX, maskY, maskIsA8, destination, destX, destY, width,
                height, solidArgb, clips);
    }

    public boolean composite(int operation, Drawable source,
                        boolean sourceRepeat, int sourceX, int sourceY,
                        Drawable mask, int maskX, int maskY,
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
            int dirtyLeft = destination.width;
            int dirtyTop = destination.height;
            int dirtyRight = 0;
            int dirtyBottom = 0;
            for (int[] clip : clips) {
                if (clip[2] <= 0 || clip[3] <= 0) continue;
                dirtyLeft = Math.min(dirtyLeft, clip[0]);
                dirtyTop = Math.min(dirtyTop, clip[1]);
                dirtyRight = Math.max(dirtyRight, clip[0] + clip[2]);
                dirtyBottom = Math.max(dirtyBottom, clip[1] + clip[3]);
            }
            if (dirtyRight <= dirtyLeft || dirtyBottom <= dirtyTop)
                return true;
            CompositeMaterial shader = activeShader();
            boolean fetch = shader.usesFramebufferFetch();
            boolean readsDest = operation != 0 && operation != 1;
            boolean maskFromDest = mask != null && mask == destination;
            if ((readsDest && !fetch || maskFromDest)
                    && !blitDestinationToScratch(destTexture,
                    destination.width, destination.height, dirtyLeft, dirtyTop,
                    dirtyRight - dirtyLeft, dirtyBottom - dirtyTop))
                return false;
            shader.use();
            shader.setUniformFloat(shader.uniforms.op, operation);
            quadVertices.bind(shader.programId);
            GLES20.glDisable(GLES20.GL_BLEND);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,
                    destTexture.getFramebuffer());
            GLES20.glViewport(0, 0, destination.width, destination.height);
            if (!fetch) {
                GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, scratch.getTextureId());
                shader.setUniformInt(shader.uniforms.dstTexture, 0);
            }
            boolean solid = solidArgb != null;
            shader.setUniformInt(shader.uniforms.srcUnusedAlpha, solid ? 0 : 1);
            boolean srcFromDst = fetch && source != null
                    && source == destination && !solid;
            shader.setUniformInt(shader.uniforms.solidSrc, solid ? 1 : 0);
            shader.setUniformInt(shader.uniforms.hasSrc,
                    !solid && source != null && !srcFromDst ? 1 : 0);
            shader.setUniformInt(shader.uniforms.srcFromDst, srcFromDst ? 1 : 0);
            if (solid) setSolidColor(solidArgb);
            if (!solid && source != null && !srcFromDst) {
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
                shader.setUniformInt(shader.uniforms.srcTexture, 1);
                shader.setUniformVec2(shader.uniforms.srcSize,
                        source.width, source.height);
            }
            else {
                shader.setUniformVec2(shader.uniforms.srcSize, 1, 1);
            }
            shader.setUniformInt(shader.uniforms.srcRepeat,
                    sourceRepeat ? 1 : 0);
            boolean hasMask = mask != null;
            shader.setUniformInt(shader.uniforms.hasMask, hasMask ? 1 : 0);
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
                shader.setUniformInt(shader.uniforms.maskTexture, 2);
                shader.setUniformVec2(shader.uniforms.maskSize,
                        mask.width, mask.height);
                shader.setUniformInt(shader.uniforms.maskChannel,
                        maskIsA8 ? 2 : 0);
            }
            else {
                shader.setUniformVec2(shader.uniforms.maskSize, 1, 1);
                shader.setUniformInt(shader.uniforms.maskChannel, 0);
            }
            shader.setUniformVec2(shader.uniforms.destSize,
                    destination.width, destination.height);
            boolean several = (!fetch || maskFromDest) && clips.size() > 1;
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
                shader.setUniformVec4(shader.uniforms.destRect,
                        x, y, w, h);
                shader.setUniformVec4(shader.uniforms.srcRect,
                        srcOffX, srcOffY, w, h);
                shader.setUniformVec4(shader.uniforms.maskRect,
                        maskOffX, maskOffY, w, h);
                GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
                if (several) copyDestRectToScratch(x, y, w, h);
            }
            quadVertices.disable();
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
            GLES20.glEnable(GLES20.GL_BLEND);
            GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA,
                    GLES20.GL_ONE_MINUS_SRC_ALPHA);
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
            int dirtyLeft = destination.width;
            int dirtyTop = destination.height;
            int dirtyRight = 0;
            int dirtyBottom = 0;
            for (GlyphQuad quad : quads) {
                if (quad.width <= 0 || quad.height <= 0) continue;
                dirtyLeft = Math.min(dirtyLeft, quad.x);
                dirtyTop = Math.min(dirtyTop, quad.y);
                dirtyRight = Math.max(dirtyRight, quad.x + quad.width);
                dirtyBottom = Math.max(dirtyBottom, quad.y + quad.height);
            }
            if (dirtyRight <= dirtyLeft || dirtyBottom <= dirtyTop)
                return true;
            CompositeMaterial shader = activeShader();
            boolean fetch = shader.usesFramebufferFetch();
            if (!fetch && !blitDestinationToScratch(destTexture,
                    destination.width, destination.height, dirtyLeft, dirtyTop,
                    dirtyRight - dirtyLeft, dirtyBottom - dirtyTop))
                return false;
            shader.use();
            quadVertices.bind(shader.programId);
            GLES20.glDisable(GLES20.GL_BLEND);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,
                    destTexture.getFramebuffer());
            GLES20.glViewport(0, 0, destination.width, destination.height);
            if (!fetch) {
                GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, scratch.getTextureId());
                shader.setUniformInt(shader.uniforms.dstTexture, 0);
            }
            shader.setUniformFloat(shader.uniforms.op, 3);
            shader.setUniformInt(shader.uniforms.srcUnusedAlpha, 0);
            shader.setUniformInt(shader.uniforms.solidSrc, 1);
            shader.setUniformInt(shader.uniforms.hasSrc, 0);
            shader.setUniformInt(shader.uniforms.hasMask, 1);
            shader.setUniformInt(shader.uniforms.srcRepeat, 0);
            shader.setUniformInt(shader.uniforms.maskChannel, 1);
            shader.setUniformInt(shader.uniforms.srcFromDst, 0);
            setSolidColor(color);
            shader.setUniformVec2(shader.uniforms.srcSize, 1, 1);
            shader.setUniformVec2(shader.uniforms.destSize,
                    destination.width, destination.height);
            shader.setUniformVec4(shader.uniforms.srcRect, 0, 0, 1, 1);
            boolean several = !fetch && quads.size() > 1;
            for (GlyphQuad quad : quads) {
                Texture glyph = glyphTexture(quad);
                if (glyph == null) continue;
                GLES20.glActiveTexture(GLES20.GL_TEXTURE2);
                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, glyph.getTextureId());
                shader.setUniformInt(shader.uniforms.maskTexture, 2);
                shader.setUniformVec2(shader.uniforms.maskSize,
                        quad.glyphWidth, quad.glyphHeight);
                shader.setUniformVec4(shader.uniforms.destRect,
                        quad.x, quad.y, quad.width, quad.height);
                shader.setUniformVec4(shader.uniforms.maskRect,
                        quad.maskX, quad.maskY, quad.width, quad.height);
                GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
                if (several) copyDestRectToScratch(quad.x, quad.y, quad.width,
                        quad.height);
            }
            quadVertices.disable();
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
            GLES20.glEnable(GLES20.GL_BLEND);
            GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA,
                    GLES20.GL_ONE_MINUS_SRC_ALPHA);
            destination.markGpuPixelsCurrent(dirtyLeft, dirtyTop,
                    dirtyRight - dirtyLeft, dirtyBottom - dirtyTop);
            return true;
        }
    }

    private static int opaqueRgb(int argb) {
        if ((argb >>> 24) == 0 && (argb & 0x00ffffff) != 0)
            return 0xff000000 | argb;
        return argb;
    }

    private void setSolidColor(int argb) {
        argb = opaqueRgb(argb);
        float inv = 1.0f / 255.0f;
        material.setUniformVec4(material.uniforms.solidColor,
                ((argb >>> 16) & 0xff) * inv,
                ((argb >>> 8) & 0xff) * inv,
                (argb & 0xff) * inv,
                ((argb >>> 24) & 0xff) * inv);
    }

    private boolean blitDestinationToScratch(Texture destTexture, int destWidth,
                                             int destHeight, int x, int y,
                                             int width, int height) {
        if (scratchWidth != destWidth || scratchHeight != destHeight
                || !scratch.isAllocated()) {
            if (scratch.isAllocated()) scratch.destroy();
            scratch.allocateTexture((short)destWidth, (short)destHeight, null);
            scratchWidth = destWidth;
            scratchHeight = destHeight;
        }
        if (!scratch.ensureFramebuffer()) return false;
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,
                scratch.getFramebuffer());
        GLES20.glViewport(0, 0, destWidth, destHeight);
        GLES20.glDisable(GLES20.GL_BLEND);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, destTexture.getTextureId());
        textureMaterial.use();
        quadVertices.bind(textureMaterial.programId);
        textureMaterial.setUniformInt(textureMaterial.uniforms.dstTexture, 0);
        textureMaterial.setUniformInt(textureMaterial.uniforms.hasSrc, 0);
        textureMaterial.setUniformInt(textureMaterial.uniforms.hasMask, 0);
        textureMaterial.setUniformFloat(textureMaterial.uniforms.op, 3);
        textureMaterial.setUniformInt(textureMaterial.uniforms.srcUnusedAlpha, 0);
        textureMaterial.setUniformInt(textureMaterial.uniforms.solidSrc, 1);
        textureMaterial.setUniformInt(textureMaterial.uniforms.srcFromDst, 0);
        textureMaterial.setUniformVec4(textureMaterial.uniforms.solidColor,
                0, 0, 0, 0);
        textureMaterial.setUniformVec2(textureMaterial.uniforms.destSize,
                destWidth, destHeight);
        textureMaterial.setUniformVec4(textureMaterial.uniforms.destRect,
                x, y, width, height);
        textureMaterial.setUniformVec2(textureMaterial.uniforms.srcSize, 1, 1);
        textureMaterial.setUniformVec2(textureMaterial.uniforms.maskSize, 1, 1);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        quadVertices.disable();
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
        return true;
    }

    public boolean fill(Drawable destination, int argb, List<int[]> clips) {
        if (destination == null || clips == null || clips.isEmpty())
            return false;
        Texture destTexture = destination.getTexture();
        if (destTexture == null) return false;
        synchronized (destination.renderLock) {
            destTexture.updateFromDrawable();
            if (!destTexture.ensureFramebuffer()) return false;
            int dirtyLeft = destination.width;
            int dirtyTop = destination.height;
            int dirtyRight = 0;
            int dirtyBottom = 0;
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,
                    destTexture.getFramebuffer());
            GLES20.glViewport(0, 0, destination.width, destination.height);
            GLES20.glEnable(GLES20.GL_SCISSOR_TEST);
            argb = opaqueRgb(argb);
            float inv = 1.0f / 255.0f;
            GLES20.glClearColor(((argb >>> 16) & 0xff) * inv,
                    ((argb >>> 8) & 0xff) * inv,
                    (argb & 0xff) * inv,
                    ((argb >>> 24) & 0xff) * inv);
            for (int[] clip : clips) {
                if (clip[2] <= 0 || clip[3] <= 0) continue;
                dirtyLeft = Math.min(dirtyLeft, clip[0]);
                dirtyTop = Math.min(dirtyTop, clip[1]);
                dirtyRight = Math.max(dirtyRight, clip[0] + clip[2]);
                dirtyBottom = Math.max(dirtyBottom, clip[1] + clip[3]);
                GLES20.glScissor(clip[0], clip[1], clip[2], clip[3]);
                GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
            }
            GLES20.glDisable(GLES20.GL_SCISSOR_TEST);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
            if (dirtyRight <= dirtyLeft || dirtyBottom <= dirtyTop)
                return true;
            destination.markGpuPixelsCurrent(dirtyLeft, dirtyTop,
                    dirtyRight - dirtyLeft, dirtyBottom - dirtyTop);
            return true;
        }
    }

    public boolean forceOpaqueRgb(Drawable destination, int x, int y,
                                  int width, int height) {
        if (destination == null || width <= 0 || height <= 0) return false;
        Texture destTexture = destination.getTexture();
        if (destTexture == null) return false;
        synchronized (destination.renderLock) {
            destTexture.updateFromDrawable();
            if (!destTexture.ensureFramebuffer()) return false;
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,
                    destTexture.getFramebuffer());
            GLES20.glViewport(0, 0, destination.width, destination.height);
            GLES20.glColorMask(false, false, false, true);
            GLES20.glEnable(GLES20.GL_SCISSOR_TEST);
            GLES20.glScissor(x, y, width, height);
            GLES20.glClearColor(0, 0, 0, 1);
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
            GLES20.glDisable(GLES20.GL_SCISSOR_TEST);
            GLES20.glColorMask(true, true, true, true);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
            destination.markGpuPixelsCurrent(x, y, width, height);
            return true;
        }
    }

    public boolean downloadGpuDirty(Drawable destination) {
        if (destination == null) return false;
        return downloadGpuDirty(destination, 0, 0, destination.width,
                destination.height);
    }

    public boolean downloadGpuDirty(Drawable destination, int x, int y,
            int width, int height) {
        synchronized (destination.renderLock) {
            int[] dirty = destination.peekGpuDirty();
            if (dirty == null) return true;
            int left = Math.max(dirty[0], Math.max(0, x));
            int top = Math.max(dirty[1], Math.max(0, y));
            int right = Math.min(dirty[0] + dirty[2],
                    Math.min(destination.width, x + width));
            int bottom = Math.min(dirty[1] + dirty[3],
                    Math.min(destination.height, y + height));
            if (right <= left || bottom <= top) return true;
            Texture destTexture = destination.getTexture();
            if (destTexture == null || !destTexture.isAllocated()) return false;
            if (!destTexture.ensureFramebuffer()) return false;
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,
                    destTexture.getFramebuffer());
            boolean ok = download(destination, left, top, right - left,
                    bottom - top);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
            if (ok && left == dirty[0] && top == dirty[1]
                    && right == dirty[0] + dirty[2]
                    && bottom == dirty[1] + dirty[3])
                destination.clearGpuDirty();
            return ok;
        }
    }

    public void uploadFromCpu(Drawable destination) {
        if (destination == null) return;
        Texture destTexture = destination.getTexture();
        if (destTexture == null) return;
        synchronized (destination.renderLock) {
            destTexture.updateFromDrawable();
        }
    }

    public boolean copy(Drawable source, int srcX, int srcY,
                        Drawable destination, int dstX, int dstY,
                        int width, int height) {
        if (source == null || destination == null || width <= 0 || height <= 0)
            return false;
        Texture destTexture = destination.getTexture();
        Texture srcTexture = source.getTexture();
        if (destTexture == null || srcTexture == null) return false;
        synchronized (destination.renderLock) {
            destTexture.updateFromDrawable();
            if (source != destination) srcTexture.updateFromDrawable();
            if (!destTexture.ensureFramebuffer()) return false;
            if (!srcTexture.ensureFramebuffer()) return false;
            boolean overlap = source == destination
                    && srcX < dstX + width && dstX < srcX + width
                    && srcY < dstY + height && dstY < srcY + height;
            while (GLES20.glGetError() != GLES20.GL_NO_ERROR) {}
            if (overlap) {
                if (!blitDestinationToScratch(destTexture, destination.width,
                        destination.height, srcX, srcY, width, height))
                    return false;
                GLES30.glBindFramebuffer(GLES30.GL_READ_FRAMEBUFFER,
                        scratch.getFramebuffer());
            }
            else {
                GLES30.glBindFramebuffer(GLES30.GL_READ_FRAMEBUFFER,
                        srcTexture.getFramebuffer());
            }
            GLES30.glBindFramebuffer(GLES30.GL_DRAW_FRAMEBUFFER,
                    destTexture.getFramebuffer());
            GLES30.glBlitFramebuffer(srcX, srcY, srcX + width, srcY + height,
                    dstX, dstY, dstX + width, dstY + height,
                    GLES20.GL_COLOR_BUFFER_BIT, GLES20.GL_NEAREST);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
            if (GLES20.glGetError() != GLES20.GL_NO_ERROR) return false;
            destination.markGpuPixelsCurrent(dstX, dstY, width, height);
            return true;
        }
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
        ByteBuffer dest = destination.getData();
        if (dest == null) return false;
        int stride = destination.getImageStride();
        dest.position(0);
        dest.limit(dest.capacity());
        while (GLES20.glGetError() != GLES20.GL_NO_ERROR) {}
        GLES20.glPixelStorei(GLES20.GL_PACK_ALIGNMENT, 4);
        GLES20.glPixelStorei(GLES30.GL_PACK_ROW_LENGTH, stride);
        GLES20.glPixelStorei(GLES30.GL_PACK_SKIP_ROWS, y);
        GLES20.glPixelStorei(GLES30.GL_PACK_SKIP_PIXELS, x);
        GLES20.glReadPixels(x, y, width, height, GLES11Ext.GL_BGRA,
                GLES20.GL_UNSIGNED_BYTE, dest);
        GLES20.glPixelStorei(GLES30.GL_PACK_ROW_LENGTH, 0);
        GLES20.glPixelStorei(GLES30.GL_PACK_SKIP_ROWS, 0);
        GLES20.glPixelStorei(GLES30.GL_PACK_SKIP_PIXELS, 0);
        dest.rewind();
        return GLES20.glGetError() == GLES20.GL_NO_ERROR;
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
