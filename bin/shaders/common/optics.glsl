float linearizeDepth(float d, float nearZ, float farZ)
{
    float ndc = d * 2.0 - 1.0;
    return (2.0 * nearZ * farZ) / (farZ + nearZ - ndc * (farZ - nearZ));
}

vec3 reconstructWorldPos(vec2 uv, float rawDepth, mat4 invViewProj) {
    vec4 ndc = vec4(
        uv.x * 2.0 - 1.0,
        uv.y * 2.0 - 1.0,
        rawDepth * 2.0 - 1.0,
        1.0
    );
    vec4 worldPosPadded = invViewProj * ndc;
    return worldPosPadded.xyz / worldPosPadded.w;
}

float schlickFresnel(float f0, float cosTheta)
{
    float clampedCos = clamp(cosTheta, 0.0, 1.0);
    float x = 1.0 - clampedCos;
    float x2 = x * x;
    return f0 + (1.0 - f0) * (x2 * x2 * x);
}

vec3 schlickFresnel(vec3 f0, float cosTheta)
{
    float clampedCos = clamp(cosTheta, 0.0, 1.0);
    float x = 1.0 - clampedCos;
    float x2 = x * x;
    return f0 + (1.0 - f0) * (x2 * x2 * x);
}