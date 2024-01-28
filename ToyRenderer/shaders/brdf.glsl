#ifndef BRDF_GLSL
#define BRDF_GLSL

#include "math.glsl"

struct PixelData {
    vec3 normal;
    vec3 pos;
    vec3 albedo;
    vec3 view;
    float perceptualRoughness;
    float metallic;
    float shadow;
};

// Mainly from https://google.github.io/filament/Filament.html
vec3 F_Schlick(float u, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

float F_Schlick(float u, float f0, float f90) {
    return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

float Fd_Burley(float NoV, float NoL, float LoH, float roughness) {
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float lightScatter = F_Schlick(NoL, 1.0, f90);
    float viewScatter = F_Schlick(NoV, 1.0, f90);
    return lightScatter * viewScatter * (1.0 / PI);
}

vec3 sampleBRDF(vec3 l, PixelData pixel) {
    vec3 diffuseColor = (1.0 - pixel.metallic) * pixel.albedo;
    vec3 f0 = vec3(0.04) * (1.0 - pixel.metallic) + pixel.albedo * pixel.metallic;

    vec3 h = normalize(pixel.view + l);

    float NoV = abs(dot(pixel.normal, pixel.view)) + 1e-5;
    float NoL = saturate(dot(pixel.normal, l));
    float NoH = saturate(dot(pixel.normal, h));
    float LoH = saturate(dot(l, h));


    float D = D_GGX(NoH, pixel.perceptualRoughness);
    vec3  F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, pixel.perceptualRoughness);

    vec3 Fr = F * D * V ;
    vec3 Fd = diffuseColor * Fd_Burley(NoV, NoL, LoH, pixel.perceptualRoughness);

    return Fr + Fd;
}

vec3 direction_light(vec3 light_direction, PixelData pixel) {
    float NoL = saturate(dot(pixel.normal, light_direction));
    return pixel.shadow * sampleBRDF(light_direction, pixel) * NoL;
}

vec3 point_light(vec3 light_pos, PixelData pixel) {
    vec3 light_direction = normalize(light_pos - pixel.pos);

    float NoL = saturate(dot(pixel.normal, light_direction));
    return sampleBRDF(light_direction, pixel) * NoL;
}


#endif // BRDF_GLSL
