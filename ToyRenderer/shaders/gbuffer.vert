#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 base_color;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(set = 0, binding = 0) uniform Matrices {
    mat4 projMat;
    mat4 viewMat;
};

void main() {
    gl_Position = projMat * viewMat * vec4(pos, 1.0);
    fragColor = base_color;
}
