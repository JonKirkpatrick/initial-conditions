#version 460 core
#include "ubos/camera.glsl"
#include "common/gBuffer.glsl"

out float FragColor;
in vec2 v_uv;

// ==============================================================================
// == Texture Samplers ==========================================================
// ==============================================================================
layout(location = 0) uniform sampler2D u_ssaoInput; 

// Linearize the raw depth so we are working with actual world units (meters)
float getLinearDepth(vec2 uv) {
    float depth = texture(u_gDepth, uv).r;
    float zNDC = depth * 2.0 - 1.0; 
    return (2.0 * u_nearPlane * u_farPlane) / (u_farPlane + u_nearPlane - zNDC * (u_farPlane - u_nearPlane));
}

void main() {
    float centerAo     = texture(u_ssaoInput, v_uv).r;
    vec3  centerNormal = normalize(texture(u_gNormal, v_uv).xyz);
    float centerDepth  = getLinearDepth(v_uv); // Flawless world distance

    float result      = 0.0;
    float totalWeight = 0.0;
    
    vec2 texelSize = 1.0 / vec2(textureSize(u_ssaoInput, 0));
    
    // Standard 1D half-Gaussian weight curve values for a 4x4 coordinate space
    // Indices [0, 1, 2] map to offsets of [0, 1, 2] pixels away from center
    float gaussian[3] = float[](0.3989, 0.2419, 0.0539);

    // Depth tolerance scales with distance from the camera, just like u_radius does
    // in ssao.frag. A fixed "2.0 meters" cutoff zeroes out almost every neighbor at
    // grazing angles or on distant/sloped geometry, since a couple of screen pixels
    // there can represent many meters of depth change on a single continuous surface.
    float depthTolerance = max(0.15, centerDepth * 0.02);

    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 neighborUV = v_uv + offset;

            float neighborAo     = texture(u_ssaoInput, neighborUV).r;
            vec3  neighborNormal = normalize(texture(u_gNormal, neighborUV).xyz);
            float neighborDepth  = getLinearDepth(neighborUV);

            // A. Calculate true Spatial Gaussian Weight
            float spatialWeight = gaussian[abs(x)] * gaussian[abs(y)];

            // B. Normal weight, softened. An exponent of 16 required near-perfectly
            // matching normals to contribute any weight at all - on terrain with
            // detail/normal maps this zeroed out nearly every neighbor, so the "blur"
            // was collapsing to just the center sample (i.e. no visible blur).
            float normalWeight = max(dot(neighborNormal, centerNormal), 0.0);
            normalWeight = pow(normalWeight, 4.0);

            // C. Smooth depth weight with a distance-scaled tolerance instead of a
            // hard cutoff. Falls off for anything far from centerDepth, but no longer
            // discontinuous, and no longer over-eager to reject legitimate
            // same-surface samples on sloped ground.
            float depthDelta = neighborDepth - centerDepth;
            float depthWeight = exp(-(depthDelta * depthDelta) / (2.0 * depthTolerance * depthTolerance));

            // Combine weights safely
            float weight = spatialWeight * normalWeight * depthWeight;

            result      += neighborAo * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight > 0.0001) {
        FragColor = result / totalWeight;
    } else {
        FragColor = centerAo;
    }
}