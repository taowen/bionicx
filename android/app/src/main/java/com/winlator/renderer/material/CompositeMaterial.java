package com.winlator.renderer.material;

public class CompositeMaterial extends ShaderMaterial {
    public final Uniforms uniforms = new Uniforms();

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
            "varying vec2 vSrcUV;",
            "varying vec2 vDstUV;",
            "varying vec2 vMaskUV;",

            "void main() {",
                "vec2 destPos = destRect.xy + position * destRect.zw;",
                "vDstUV = destPos / destSize;",
                "vSrcUV = srcSize.x > 0.5",
                    "? (srcRect.xy + position * destRect.zw) / srcSize",
                    ": vec2(0.0);",
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
        return String.join("\n",
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
            "varying vec2 vSrcUV;",
            "varying vec2 vDstUV;",
            "varying vec2 vMaskUV;",

            "vec2 applyRepeat(vec2 uv) {",
                "if (srcRepeat == 1) return fract(uv);",
                "return uv;",
            "}",

            "vec4 sampleSource() {",
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

            "void main() {",
                "vec4 dst = texture2D(dstTexture, vDstUV);",
                "if (dst.a < 0.001 && (dst.r + dst.g + dst.b) > 0.0)",
                    "dst.a = 1.0;",
                "vec4 src = sampleSource();",
                "src.a *= sampleMask();",
                "float outA = src.a + dst.a * (1.0 - src.a);",
                "vec3 outC = outA > 0.0",
                    "? (src.rgb * src.a + dst.rgb * dst.a * (1.0 - src.a)) / outA",
                    ": vec3(0.0);",
                "gl_FragColor = vec4(outC, outA);",
            "}"
        );
    }
}
