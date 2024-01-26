#version 450

layout(location = 0) in vec3 fragBarycentric;
layout(location = 1) in vec3 fragColor;

layout(location = 0) out vec4 color;

void main() {
    vec3 delta = fwidth(fragBarycentric);
    vec3 bary = smoothstep(delta, 3*delta, fragBarycentric);
    float d = min(min(bary.y.x, bary.y), bary.z);

    color = vec4(fragColor, 1.0 - d);
}
