#version 450
#include "brdf.glsl"

layout(location = 0) in mat3 TBN;
layout(location = 3) in vec3 fragViewDir;
layout(location = 4) in vec2 fragUV1;
layout(location = 5) in vec2 fragUV2;
layout(location = 6) in vec4 fragPos;

layout(location = 0) out vec4 Fragcolor;


layout(set = 1, binding = 0) uniform sampler2D[3] texs;
layout(set = 1, binding = 1) uniform sampler2D shadow_map;

layout(push_constant) uniform light{
    layout (offset = 64) mat4 LightProjMatrix;
    mat4 LightViewMatrix;
    vec3 LightPosition;
    vec3 LightColor;
};

#ifndef PERCENTAGE_CLOSER_FILTERING_ITER
#define PERCENTAGE_CLOSER_FILTERING_ITER 3
#endif

#ifndef SHADOW_BIAS
#define SHADOW_BIAS 0.001
#endif

float compute_shadow(vec3 pos) {
    vec4 f = LightProjMatrix * LightViewMatrix * vec4(pos, 1.0);
    vec3 d = f.xyz / f.w;

#ifdef PERCENTAGE_CLOSER_FILTERING
    vec2 width = 1.0 / textureSize(shadow_map, 0).xy;

    float s = 0;
    for(int i = -PERCENTAGE_CLOSER_FILTERING_ITER; i <= PERCENTAGE_CLOSER_FILTERING_ITER; i++){
        for(int j = -PERCENTAGE_CLOSER_FILTERING_ITER; j <= PERCENTAGE_CLOSER_FILTERING_ITER; j++){
            vec2 pos =  0.5 * d.xy + vec2(0.5) + vec2(i*width.x, j*width.y);
            float v = texture(shadow_map, pos).x;
            s += v >= (d.z - SHADOW_BIAS) ? 1.0 : 0.0;
        }
    }

    float count = (2*PERCENTAGE_CLOSER_FILTERING_ITER + 1) * (2*PERCENTAGE_CLOSER_FILTERING_ITER+1);
    return s / count;
#else
    float v = texture(shadow_map, 0.5 * d.xy + vec2(0.5)).x;
    return  v >= (d.z - SHADOW_BIAS) ? 1.0 : 0.0;
#endif
}

PixelData getPixelData() {
    PixelData pixel;
    pixel.normal = TBN * (texture(texs[2], fragUV1) * 2.0 - 1.0).rgb; 
    pixel.albedo = texture(texs[0], fragUV1).rgb;

    vec4 roughness_metallic = texture(texs[1], fragUV1);
    pixel.perceptualRoughness = roughness_metallic.g;
    pixel.metallic = roughness_metallic.b;

    pixel.view = fragViewDir;
    pixel.pos = fragPos.xyz;
    pixel.shadow = compute_shadow(pixel.pos);
    return pixel;
}

void main() {
    PixelData pixel = getPixelData();
    float alpha = texture(texs[0], fragUV1).a;

    vec3 color = 0.1*pixel.albedo + LightColor * direction_light(LightPosition, pixel);
    Fragcolor = vec4(color, alpha);
}
