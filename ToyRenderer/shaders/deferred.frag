#version 450


layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

layout(set = 0, binding = 0) uniform Block {
    float factor;
};
layout(set = 0, binding = 1) uniform sampler2D gbuffer;

void main() {
    color = vec4(factor * texture(gbuffer, uv).xyz, 1.0);
}
