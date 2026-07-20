#version 460 core
#include "ubos/camera.glsl"

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform float time;

void main()
{
    // 1. Compute the flat world position relative to the moving grid model matrix
    vec4 worldPos = model * vec4(aPos, 1.0);

    // 2. Generate wave offsets using absolute WORLD space coordinates (worldPos.xz)
    // This ensures waves stay structurally locked in place as the grid snaps underneath them
    float wave1 = sin(worldPos.x * 0.05 + time * 1.5) * 1.5;
    float wave2 = cos((worldPos.x + worldPos.z) * 0.03 + time * 1.0) * 1.0;
    
    // Displace height vertically in the world
    worldPos.y += (wave1 + wave2);

    // 3. Populate your baseline outputs
    FragPos = vec3(worldPos);
    
    // For now, pass an upward normal transformed by the model matrix orientation
    Normal = mat3(transpose(inverse(model))) * vec3(0.0, 1.0, 0.0);
    
    TexCoords = aTexCoords;
    
    // 4. Project using your camera UBO
    gl_Position = u_proj * u_view * worldPos;
}