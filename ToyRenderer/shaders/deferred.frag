#version 450


layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 Fragcolor;

layout(set = 0, binding = 0) uniform sampler2D gbuffer_color;
layout(set = 0, binding = 1) uniform sampler2D gbuffer_normal;

void main() {
    vec3 light = normalize(vec3(15, 12, -50));

    vec3 color = texture(gbuffer_color, uv).xyz;
    vec3 normal = texture(gbuffer_normal, uv).xyz;

    Fragcolor = vec4(color * clamp(0.7 + dot(normal, light), 0.0, 1.0), 1.0);
}
