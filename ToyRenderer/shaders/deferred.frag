#version 450

#include "brdf.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 Fragcolor;

layout(set = 0, binding = 0, rgba32f) uniform readonly image2D[4] gbuffers;
layout(set = 0, binding = 1) uniform sampler2D shadow_map;

layout(push_constant) uniform light{
    mat4 LightProjMatrix;
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

PixelData getPixelData(){
    PixelData pixel;
    ivec2 size = imageSize(gbuffers[0]);
    ivec2 screen_coordinate = ivec2(vec2(size) * uv);

    vec4 t0 = imageLoad(gbuffers[0], screen_coordinate);
    vec4 t1 = imageLoad(gbuffers[1], screen_coordinate);
    vec4 t2 = imageLoad(gbuffers[2], screen_coordinate);
    vec4 t3 = imageLoad(gbuffers[3], screen_coordinate);

    // TODO: more complex packing
    pixel.albedo =  t0.xyz;
    pixel.perceptualRoughness = t0.a;
    pixel.normal = t1.xyz;
    pixel.metallic = t1.a;
    pixel.view = t2.xyz;
    pixel.pos = t3.xyz;

    pixel.shadow = compute_shadow(pixel.pos);

    return pixel;
}

void main() {
    PixelData pixel = getPixelData();

    vec3 color = 0.1*pixel.albedo + LightColor * direction_light(LightPosition, pixel);
    Fragcolor = vec4(color, 1.0);
} 
