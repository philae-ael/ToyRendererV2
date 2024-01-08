#version 450


layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 Fragcolor;

layout(set = 0, binding = 0) uniform sampler2D[3] gbuffers;

float PI = 3.14159;

// Mainly from https://google.github.io/filament/Filament.html

vec3 F_Schlick(float u, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL + 1e-5);
}

float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}


struct PixelData {
    vec3 normal;
    vec3 albedo;
    vec3 view;
    float perceptualRoughness;
    float metallic;
};

vec3 saturate(vec3 v) {
    return clamp(v, 0.0, 1.0);
}

float saturate(float v) {
    return clamp(v, 0.0, 1.0);
}

PixelData getPixelData(){
    PixelData pixel;
    vec4 t1 = texture(gbuffers[0], uv);
    vec4 t2 = texture(gbuffers[1], uv);
    vec4 t3 = texture(gbuffers[2], uv);

    // TODO: more complex packing
    pixel.albedo =  t1.xyz;
    pixel.perceptualRoughness = t1.a;
    pixel.normal =  t2.xyz;
    pixel.metallic = t2.a;
    pixel.view = t3.xyz;

    return pixel;
}

vec3 sampleBRDF(vec3 l, PixelData pixel) {
    vec3 diffuseColor = (1.0 - pixel.metallic) * pixel.albedo;
    vec3 f0 = vec3(0.2) * (1.0 - pixel.metallic) + pixel.albedo * pixel.metallic;
    float roughness = saturate(pixel.perceptualRoughness * pixel.perceptualRoughness);

    vec3 h = normalize(pixel.view + l);

    float NoV = abs(dot(pixel.normal, pixel.view)) + 1e-5;
    float NoL = saturate(dot(pixel.normal, l));
    float NoH = saturate(dot(pixel.normal, h));
    float LoH = saturate(dot(l, h));


    float D = D_GGX(NoH, roughness);
    vec3  F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, roughness);


    vec3 Fr = D * F * V;
    vec3 Fd = diffuseColor;

    return Fr + Fd;
}

vec3 direction_light(vec3 light_direction, PixelData pixel) {
    return sampleBRDF(light_direction, pixel);
}

void main() {
    PixelData pixel = getPixelData();

    vec3 color = 0.6*direction_light(normalize(vec3(1, -1, 0)), pixel);
    color += 0.4*direction_light(normalize(vec3(1, 0, -1)), pixel);
    Fragcolor = vec4(color, 1.0);
} 
