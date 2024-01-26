#version 450

layout(location = 0) in vec3 pos;
layout(location = 3) in vec3 color;

layout(location = 0) out vec3 fragBarycentric;
layout(location = 1) out vec3 fragColor;

layout(set = 0, binding = 0) uniform Global{
    mat4 projMat;
    mat4 viewMat;
    vec3 cameraPosition;
};

void main() {
    gl_Position = projMat * viewMat * vec4(pos, 1.0);

    int i = gl_VertexIndex % 3;
    fragBarycentric = vec3(i == 0, i == 1, i == 2);
    fragColor = vec3(1.0, 1.0, 1.0);
}
