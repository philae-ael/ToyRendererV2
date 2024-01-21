#version 450


layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 Fragcolor;

layout(set = 0, binding = 0) uniform sampler2D internal_image;

void main() {
    Fragcolor = vec4(texture(internal_image, uv).xyz, 1.0);
} 
