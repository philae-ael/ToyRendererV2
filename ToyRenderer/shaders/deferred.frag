#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 normal;

void main() {
    color = vec4(fragColor, 1.0);
    normal = vec4(fragColor, 1.0);
}
