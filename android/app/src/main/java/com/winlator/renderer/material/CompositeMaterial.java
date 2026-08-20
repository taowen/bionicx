package com.winlator.renderer.material;

public class CompositeMaterial extends ShaderMaterial {
    public enum Fetch { TEXTURE, EXT, ARM }

    public final Uniforms uniforms = new Uniforms();
    public final Fetch fetch;

    public CompositeMaterial() {
        this(Fetch.TEXTURE);
    }

    public CompositeMaterial(Fetch fetch) {
        this.fetch = fetch;
    }

    public boolean usesFramebufferFetch() {
        return fetch != Fetch.TEXTURE;
    }

    public static class Uniforms {
        public final Uniform destRect = new Uniform("destRect");
        public final Uniform destSize = new Uniform("destSize");
        public final Uniform srcRect = new Uniform("srcRect");
        public final Uniform srcSize = new Uniform("srcSize");
        public final Uniform maskRect = new Uniform("maskRect");
        public final Uniform maskSize = new Uniform("maskSize");
        public final Uniform srcTexture = new Uniform("srcTexture");
        public final Uniform dstTexture = new Uniform("dstTexture");
        public final Uniform maskTexture = new Uniform("maskTexture");
        public final Uniform solidColor = new Uniform("solidColor");
        public final Uniform hasSrc = new Uniform("hasSrc");
        public final Uniform hasMask = new Uniform("hasMask");
        public final Uniform solidSrc = new Uniform("solidSrc");
        public final Uniform srcRepeat = new Uniform("srcRepeat");
        public final Uniform maskChannel = new Uniform("maskChannel");
        public final Uniform srcFromDst = new Uniform("srcFromDst");
        public final Uniform srcUnusedAlpha = new Uniform("srcUnusedAlpha");
        public final Uniform srcXform0 = new Uniform("srcXform0");
        public final Uniform srcXform1 = new Uniform("srcXform1");
        public final Uniform srcXform2 = new Uniform("srcXform2");
        public final Uniform op = new Uniform("op");
    }

    @Override
    protected String getVertexShader() {
        return String.join("\n",
            "attribute vec2 position;",
            "uniform vec4 destRect;",
            "uniform vec2 destSize;",
            "uniform vec4 srcRect;",
            "uniform vec2 srcSize;",
            "uniform vec4 maskRect;",
            "uniform vec2 maskSize;",
            "uniform vec3 srcXform0;",
            "uniform vec3 srcXform1;",
            "uniform vec3 srcXform2;",
            "varying vec2 vSrcUV;",
            "varying vec2 vDstUV;",
            "varying vec2 vMaskUV;",

            "void main() {",
                "vec2 destPos = destRect.xy + position * destRect.zw;",
                "vDstUV = destPos / destSize;",
                "vec2 srcPx = srcRect.xy + position * destRect.zw;",
                "vec3 srcP = vec3(srcPx, 1.0);",
                "vec3 mapped = vec3(dot(srcXform0, srcP),",
                    "dot(srcXform1, srcP), dot(srcXform2, srcP));",
                "vec2 srcMapped = abs(mapped.z) > 0.000001",
                    "? mapped.xy / mapped.z : mapped.xy;",
                "vSrcUV = srcSize.x > 0.5 ? srcMapped / srcSize : vec2(0.0);",
                "vMaskUV = maskSize.x > 0.5",
                    "? (maskRect.xy + position * destRect.zw) / maskSize",
                    ": vec2(0.0);",
                "gl_Position = vec4(2.0 * destPos.x / destSize.x - 1.0,",
                    "2.0 * destPos.y / destSize.y - 1.0, 0.0, 1.0);",
            "}"
        );
    }

    @Override
    protected String getFragmentShader() {
        String fetchExt = fetch == Fetch.EXT
                ? "#extension GL_EXT_shader_framebuffer_fetch : require"
                : fetch == Fetch.ARM
                ? "#extension GL_ARM_shader_framebuffer_fetch : require"
                : "";
        String sampleDest = fetch == Fetch.EXT
                ? "return gl_LastFragData[0];"
                : fetch == Fetch.ARM
                ? "return gl_LastFragColorARM;"
                : "return texture2D(dstTexture, vDstUV);";
        return String.join("\n",
            fetchExt,
            "precision mediump float;",

            "uniform sampler2D srcTexture;",
            "uniform sampler2D dstTexture;",
            "uniform sampler2D maskTexture;",
            "uniform vec4 solidColor;",
            "uniform int hasSrc;",
            "uniform int hasMask;",
            "uniform int solidSrc;",
            "uniform int srcRepeat;",
            "uniform int maskChannel;",
            "uniform int srcFromDst;",
            "uniform int srcUnusedAlpha;",
            "uniform float op;",
            "varying vec2 vSrcUV;",
            "varying vec2 vDstUV;",
            "varying vec2 vMaskUV;",

            "vec2 applyRepeat(vec2 uv) {",
                "if (srcRepeat == 1) return fract(uv);",
                "return uv;",
            "}",

            "vec4 sampleDest() {",
                sampleDest,
            "}",

            "vec4 sampleSource(vec4 dst) {",
                "if (srcFromDst != 0) return dst;",
                "if (solidSrc != 0) return solidColor;",
                "if (hasSrc == 0) return vec4(0.0);",
                "vec2 uv = applyRepeat(vSrcUV);",
                "if (srcRepeat == 0 && (uv.x < 0.0 || uv.y < 0.0",
                        "|| uv.x > 1.0 || uv.y > 1.0))",
                    "return vec4(0.0);",
                "return texture2D(srcTexture, clamp(uv, 0.0, 1.0));",
            "}",

            "float sampleMask() {",
                "if (hasMask == 0) return 1.0;",
                "if (vMaskUV.x < 0.0 || vMaskUV.y < 0.0",
                        "|| vMaskUV.x > 1.0 || vMaskUV.y > 1.0)",
                    "return 0.0;",
                "vec4 texel = texture2D(maskTexture, vMaskUV);",
                "if (maskChannel == 2) return texel.b;",
                "if (maskChannel == 1) return texel.r;",
                "return texel.a;",
            "}",

            "vec4 unusedAlpha(vec4 pixel) {",
                "if (pixel.a < 0.001 && (pixel.r + pixel.g + pixel.b) > 0.0)",
                    "pixel.a = 1.0;",
                "return pixel;",
            "}",

            "void main() {",
                "vec4 dst = unusedAlpha(sampleDest());",
                "vec4 src = sampleSource(dst);",
                "if (srcUnusedAlpha != 0) src = unusedAlpha(src);",
                "src.a *= sampleMask();",
                "if (op > 0.5 && op < 1.5) {",
                    "gl_FragColor = src.a <= 0.0 ? vec4(0.0) : vec4(src.rgb, src.a);",
                    "return;",
                "}",
                "if (op > 4.5 && op < 5.5) {",
                    "float a = src.a * dst.a;",
                    "gl_FragColor = a <= 0.0 ? vec4(0.0) : vec4(src.rgb, a);",
                    "return;",
                "}",
                "if (op > 7.5 && op < 8.5) {",
                    "float a = dst.a * (1.0 - src.a);",
                    "gl_FragColor = a <= 0.0 ? vec4(0.0) : vec4(dst.rgb, a);",
                    "return;",
                "}",
                "if (op > 11.5 && op < 12.5) {",
                    "float a = min(1.0, src.a + dst.a);",
                    "vec3 premul = src.rgb * src.a + dst.rgb * dst.a;",
                    "gl_FragColor = a <= 0.0 ? vec4(0.0)",
                        ": vec4(min(vec3(1.0), premul / a), a);",
                    "return;",
                "}",
                "if (op > 12.5 && op < 13.5) {",
                    "float srcContrib = min(src.a, 1.0 - dst.a);",
                    "float a = srcContrib + dst.a;",
                    "vec3 premul = src.rgb * srcContrib + dst.rgb * dst.a;",
                    "gl_FragColor = a <= 0.0 ? vec4(0.0)",
                        ": vec4(min(vec3(1.0), premul / a), a);",
                    "return;",
                "}",
                "float outA = src.a + dst.a * (1.0 - src.a);",
                "vec3 outC = outA > 0.0",
                    "? (src.rgb * src.a + dst.rgb * dst.a * (1.0 - src.a)) / outA",
                    ": vec3(0.0);",
                "gl_FragColor = vec4(outC, outA);",
            "}"
        );
    }
}
