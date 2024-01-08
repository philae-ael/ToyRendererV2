#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

layout(set = 0, binding = 0) uniform Global{
    mat4 projMat;
    mat4 viewMat;
};

layout(push_constant) uniform Mesh{
    mat4 modelMat;
};

void main() {
    gl_Position = projMat * viewMat * modelMat * vec4(pos, 1.0);

    fragColor = vec3(1);
    fragNormal = normal;
}
