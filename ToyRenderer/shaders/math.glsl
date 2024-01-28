#ifndef MATH_GLSL
#define MATH_GLSL

const float PI = 3.14159265359;
vec3 saturate(vec3 v) {
    return clamp(v, 0.0, 1.0);
}
float saturate(float v) {
    return clamp(v, 0.0, 1.0);
}

#endif // MATH_GLSL
