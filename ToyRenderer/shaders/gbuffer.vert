#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 uv1;
layout(location = 4) in vec2 uv2;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragViewDir;
layout(location = 2) out vec2 fragUV1;
layout(location = 3) out vec2 fragUV2;

layout(set = 0, binding = 0) uniform Global{
    mat4 projMat;
    mat4 viewMat;
};

layout(push_constant) uniform Mesh{
    mat4 modelMat;
};

void main() {
    vec4 WorldPos = modelMat * vec4(pos, 1.0);
    vec4 view = vec4(0.0, 0.0, 0.0, 1.0) - viewMat * WorldPos;
    gl_Position = projMat * viewMat * WorldPos;

    fragUV1 = uv1;
    fragUV2 = uv2;
    fragNormal = normal;
    fragViewDir =  normalize(view.xyz);
}
