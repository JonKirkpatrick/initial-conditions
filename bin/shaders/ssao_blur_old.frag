#version 460 core
out float FragColor;
in vec2 v_uv;

uniform sampler2D u_ssaoInput; 
uniform sampler2D u_gNormal;    
uniform sampler2D u_gDepth;     

uniform float     u_near = 1.0;       // Match your engine settings
uniform float     u_far  = 5000.0;    

// Linearize the raw depth so we are working with actual world units (meters)
float getLinearDepth(vec2 uv) {
    float depth = texture(u_gDepth, uv).r;
    float zNDC = depth * 2.0 - 1.0; 
    return (2.0 * u_near * u_far) / (u_far + u_near - zNDC * (u_far - u_near));
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

    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 neighborUV = v_uv + offset;

            float neighborAo     = texture(u_ssaoInput, neighborUV).r;
            vec3  neighborNormal = normalize(texture(u_gNormal, neighborUV).xyz);
            float neighborDepth  = getLinearDepth(neighborUV);

            // A. Calculate true Spatial Gaussian Weight
            float spatialWeight = gaussian[abs(x)] * gaussian[abs(y)];

            // B. Calculate Normal Weight (unchanged, works well)
            float normalWeight = max(dot(neighborNormal, centerNormal), 0.0);
            normalWeight = pow(normalWeight, 16.0); 

            // C. Fixed Asymmetric Depth Weight: Using linear world meters!
            float depthDelta = neighborDepth - centerDepth; // No abs() here initially

            float depthWeight;
            if (depthDelta > 2.0) {
                // The neighbor is more than 2 meters BEHIND the center pixel.
                // This means we are sampling across a silhouette gap onto background geometry.
                // Drop the weight to 0 to completely kill background white bleeding forward.
                depthWeight = 0.0;
            } else {
                // The neighbor is either on the same surface or in FRONT of the center pixel.
                // Use the standard smooth Gaussian falloff curve.
                float absDelta = abs(depthDelta);
                depthWeight = exp(-absDelta * absDelta * 2.0); 
            }

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