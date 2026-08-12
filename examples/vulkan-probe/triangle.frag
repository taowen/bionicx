#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 color;
layout(set = 0, binding = 0) uniform Tint {
    vec4 value;
} tint;

void main() {
    outColor = vec4(color * tint.value.rgb, tint.value.a);
}
