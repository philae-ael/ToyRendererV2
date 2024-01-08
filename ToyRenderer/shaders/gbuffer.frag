#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 normal;

layout(set = 1, binding = 0) uniform sampler2D tex;

void main() {
    color = vec4(texture(tex, fragUV).xyz, 1.0);
    normal = vec4(fragNormal, 1.0);
}
