// ==============================================================================
// == Camera Data Uniform Buffer Block & Global Aliases ========================
// ==============================================================================

layout (std140, binding = 0) uniform CameraData {
    mat4 u_view;
    mat4 u_proj;
    mat4 u_viewProj;
    mat4 u_invViewProj;
    
    vec3 u_cameraPos;
    float u_fovY;
    
    vec3 u_cameraForward;
    float aspectRatio;
    
    vec3 u_cameraRight;
    float u_cameraHeight;
    
    vec3 u_cameraUp;
    float u_farPlane;
    
    vec2 u_viewportSize;
    float u_nearPlane;
    float cameraData_padding;
};