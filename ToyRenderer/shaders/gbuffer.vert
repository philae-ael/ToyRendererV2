#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec3 color;
layout(location = 4) in vec2 uv1;
layout(location = 5) in vec2 uv2;

layout(location = 0) out mat3 TBN;
layout(location = 3) out vec3 fragViewDir;
layout(location = 4) out vec2 fragUV1;
layout(location = 5) out vec2 fragUV2;

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

    vec3 bitangent = normalize(cross(normal, tangent));
    vec3 T = normalize(vec3(modelMat * vec4(tangent, 1.0)));
    vec3 B = normalize(vec3(modelMat * vec4(bitangent, 1.0)));
    vec3 N = normalize(vec3(modelMat * vec4(normal, 1.0)));
    TBN = mat3(T, B, N);

    fragUV1 = uv1;
    fragUV2 = uv2;
    fragViewDir =  normalize(view.xyz);
}
