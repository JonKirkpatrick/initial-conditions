#version 460 core
out float FragColor;

in vec2 v_uv;

uniform sampler2D u_ssaoInput; // The raw, grainy SSAO texture
uniform sampler2D u_gNormal;    // Used to detect sharp geometric corners
uniform sampler2D u_gDepth;     // Used to detect sharp depth discontinuities

void main() {
    // 1. Fetch properties of the center pixel (the one we are blurring)
    float centerAo     = texture(u_ssaoInput, v_uv).r;
    vec3  centerNormal = texture(u_gNormal, v_uv).xyz;
    float centerDepth  = texture(u_gDepth, v_uv).r;

    float result     = 0.0;
    float totalWeight = 0.0;
    
    // Determine texel size for a 4x4 blur neighborhood
    vec2 texelSize = 1.0 / vec2(textureSize(u_ssaoInput, 0));
    
    // 2. Loop through a 4x4 neighborhood (-2 to 1 keeps it centered nicely)
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 neighborUV = v_uv + offset;

            // Fetch neighbor properties
            float neighborAo     = texture(u_ssaoInput, neighborUV).r;
            vec3  neighborNormal = texture(u_gNormal, neighborUV).xyz;
            float neighborDepth  = texture(u_gDepth, neighborUV).r;

            // 3. Calculate edge-preservation weights
            
            // Spatial weight (closer pixels matter more)
            float spatialWeight = 1.0; 

            // Normal weight: If the neighbor's face points a completely different direction, ignore it
            float normalWeight = max(dot(neighborNormal, centerNormal), 0.0);
            normalWeight = pow(normalWeight, 16.0); // Sharpen the cutoff threshold

            // Depth weight: If the neighbor is much closer or further away, ignore it
            float depthWeight = 1.0 / (1.0 + abs(centerDepth - neighborDepth) * 200.0);

            // Combine weights
            float weight = spatialWeight * normalWeight * depthWeight;

            // 4. Accumulate
            result      += neighborAo * weight;
            totalWeight += weight;
        }
    }

    // 5. Fallback in case weights drop to 0 (isolated sub-pixel edges)
    if (totalWeight > 0.0001) {
        FragColor = result / totalWeight;
    } else {
        FragColor = centerAo;
    }
}