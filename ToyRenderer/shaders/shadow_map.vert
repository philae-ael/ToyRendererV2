#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec3 color;
layout(location = 4) in vec2 uv1;
layout(location = 5) in vec2 uv2;

layout(set = 0, binding = 0) uniform Global{
    mat4 projMat;
    mat4 viewMat;
    vec3 cameraPosition;
};

layout(push_constant) uniform Mesh{
    mat4 modelMat;
};

void main() {
    gl_Position = projMat * viewMat * modelMat * vec4(pos, 1.0);;
}
