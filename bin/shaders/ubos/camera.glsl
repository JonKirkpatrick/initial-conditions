// ==============================================================================
// == Camera Data Uniform Buffer Block & Global Aliases ========================
// ==============================================================================
#define u_invViewProj   u_invViewProj_Block
#define u_cameraPos     u_cameraPos_Block
#define u_cameraForward u_cameraForward_Block
#define u_fovY          fovY

layout (std140, binding = 0) uniform CameraData {
    mat4 u_view;
    mat4 u_proj;
    mat4 u_viewProj;
    mat4 u_invViewProj_Block;
    
    vec3 u_cameraPos_Block;
    float fovY;
    
    vec3 u_cameraForward_Block;
    float aspectRatio;
    
    vec3 u_cameraRight;
    float u_cameraHeight;
    
    vec3 u_cameraUp;
    float u_farPlane;
    
    vec2 u_viewportSize;
    float u_nearPlane;
    float cameraData_padding;
};