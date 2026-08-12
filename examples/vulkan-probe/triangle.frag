#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 color;
layout(set = 0, binding = 0) uniform Tint {
    vec4 value;
} tint;
layout(set = 0, binding = 1) uniform sampler2D textureSampler;

void main() {
    vec4 sampled = texture(textureSampler, vec2(0.5, 0.5));
    outColor = vec4(color * tint.value.rgb * sampled.rgb,
                    tint.value.a * sampled.a);
}
