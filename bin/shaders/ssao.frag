#version 460 core

out float FragColor;
in vec2 v_uv;

// ==============================================================================
// == Uniform Buffer Binding 0 (Camera Data) ====================================
// ==============================================================================
layout (std140, binding = 0) uniform CameraData {
    mat4 u_view;
    mat4 u_proj;
    mat4 u_viewProj;
    mat4 u_invViewProj;
    vec3 u_cameraPos;
    float fovY;
    vec3 u_cameraForward;
    float aspectRatio;
    vec3 u_cameraRight;
    float u_cameraHeight;
    vec3 u_cameraUp;
    float u_farPlane;
    vec2 u_viewportSize;
    float u_nearPlane;
};

// Aliases to bridge standard SSAO variable names seamlessly
#define u_projection u_proj
#define u_view       u_view
#define u_near       u_nearPlane
#define u_far        u_farPlane

// ==============================================================================
// == Texture Samplers ==========================================================
// ==============================================================================
uniform sampler2D u_gNormal;
uniform sampler2D u_gDepth;
uniform sampler2D u_texNoise;

// ==============================================================================
// == Remaining Loose Uniforms ==================================================
// ==============================================================================
uniform vec3  u_samples[64];
uniform float u_radius = 4.0;     
uniform float u_bias = 0.05;       
uniform vec2  u_noiseScale;  
uniform int   u_sampleCount = 64;

vec3 getPositionInViewSpace(vec2 uv) {
    float depth = texture(u_gDepth, uv).r;
    
    // 1. Get standard linear view-space depth (Z)
    float zNDC = depth * 2.0 - 1.0; 
    float linearDepth = (2.0 * u_near * u_far) / (u_far + u_near - zNDC * (u_far - u_near));
    
    // 2. Reconstruct X and Y directly from the Projection Matrix components
    // This turns screen UVs into a pristine View-Space Ray completely independent of world rotation!
    float x = (uv.x * 2.0 - 1.0) / u_projection[0][0];
    float y = (uv.y * 2.0 - 1.0) / u_projection[1][1];
    
    // Remember: In OpenGL view space, objects in front of the camera have negative Z,
    // so we multiply our projection scalars by the linear depth distance.
    return vec3(x * linearDepth, y * linearDepth, -linearDepth);
}

void main() {
    // Current pixel position (Guaranteed to be camera-rotation stable!)
    vec3 fragPos = getPositionInViewSpace(v_uv);
    
    // Fetch and transform World Normals into View Space
    vec3 worldNormal = normalize(texture(u_gNormal, v_uv).xyz);
    vec3 normal      = normalize(mat3(u_view) * worldNormal);

    // Tangent-Bitangent-Normal rotation kernel matrix
    vec3 randomVec = normalize(texture(u_texNoise, v_uv * u_noiseScale).xyz);
    vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for(int i = 0; i < u_sampleCount; ++i) {
        vec3 samplePos = TBN * u_samples[i]; 
        samplePos = fragPos + samplePos * u_radius; 
        
        // Project sample space back to screen coordinates
        vec4 offset = vec4(samplePos, 1.0);
        offset      = u_projection * offset;    
        offset.xyz /= offset.w;               
        offset.xyz  = offset.xyz * 0.5 + 0.5; 
        
        // Sample actual geometry at this offset point
        vec3 sampleActualPos = getPositionInViewSpace(offset.xy);
        
        // Scale-invariant dynamic range check to handle outdoor distances smoothly
        float depthDelta = abs(fragPos.z - sampleActualPos.z);
        float dynamicDistanceThreshold = u_radius * max(1.0, -fragPos.z * 0.05); 
        float rangeCheck = smoothstep(0.0, 1.0, dynamicDistanceThreshold / depthDelta);
        
        // Check depth delta occlusion
        occlusion += (sampleActualPos.z >= samplePos.z + u_bias ? 1.0 : 0.0) * rangeCheck;           
    }
    float samples = float(u_sampleCount);
    occlusion = 1.0 - (occlusion / samples);
    FragColor = occlusion;
}