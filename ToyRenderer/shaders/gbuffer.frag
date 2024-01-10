#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragViewDir;
layout(location = 2) in vec2 fragUV1;
layout(location = 3) in vec2 fragUV2;

layout(location = 0) out vec4 g1;
layout(location = 1) out vec4 g2;
layout(location = 2) out vec4 g3;

layout(set = 1, binding = 0) uniform sampler2D[2] texs;

void main() {
    vec4 albedo = texture(texs[0], fragUV1);
    vec4 roughness_metallic = texture(texs[1], fragUV1);

    g1.xyz = albedo.xyz;
    // roughness
    g1.a = roughness_metallic.g;

    // TODO: Normal map
    g2.xyz = fragNormal;
    // metallic
    g2.a = roughness_metallic.b;

    // TODO: not necessary, remove it and deduce from matrix + depth in deferred
    g3.xyz = fragViewDir;
}
