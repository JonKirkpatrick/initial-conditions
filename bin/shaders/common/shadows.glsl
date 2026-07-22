layout(location = 200) uniform mat4  u_lightViewProj[5];
layout(location = 205) uniform float u_texelWorldSize[5];
layout(location = 210) uniform float u_cascadeSplitDepths[5];
layout(location = 215) uniform sampler2DArrayShadow u_shadowMap;

int selectCascade(vec3 worldPos) {
    float viewDepth = dot(worldPos - u_cameraPos, u_cameraForward);
    for (int i = 0; i < 4; ++i) {
        if (viewDepth < u_cascadeSplitDepths[i]) return i;
    }
    return 4;
}

float computeShadow(vec3 worldPos, vec3 normal, vec3 sunDir, int cascade) {
    float NdotL = max(dot(normal, sunDir), 0.0);
    float normalOffsetTexels = mix(1.0, 0.5, NdotL);
    vec3 offsetPos = worldPos + normal * (normalOffsetTexels * u_texelWorldSize[cascade]);

    vec4 lightClip = u_lightViewProj[cascade] * vec4(offsetPos, 1.0);
    vec3 lightNdc  = lightClip.xyz / lightClip.w;
    vec3 shadowUV  = lightNdc * 0.5 + 0.5;

    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 ||
        shadowUV.y < 0.0 || shadowUV.y > 1.0 ||
        shadowUV.z < 0.0 || shadowUV.z > 1.0) {
        return 1.0;
    }

    float tanTheta = sqrt(1.0 - NdotL * NdotL) / max(NdotL, 0.05);
    const float baseBias = 0.0003;
    const float maxBias  = 0.004; 
    float bias = clamp(baseBias * (1.0 + tanTheta), baseBias, maxBias);

    float texelSize = 1.0 / float(textureSize(u_shadowMap, 0).x);
    float sum = 0.0;
    int K = max(1, 3 - cascade);
    int taps = 0;
    for (int x = -K; x <= K; ++x) {
        for (int y = -K; y <= K; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            sum += texture(u_shadowMap, vec4(shadowUV.xy + offset, float(cascade), shadowUV.z - bias));
            taps++;
        }
    }
    return sum / float(taps);
}