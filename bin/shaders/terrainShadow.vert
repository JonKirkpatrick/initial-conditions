#version 460 core

layout(location = 0) in vec3 a_position; // Or whatever attribute setup your vertex array has

// The active cascade matrix sent via glUniformMatrix4fv inside the loop
uniform mat4 u_shadowMatrix; 

void main() {
    // Transform the terrain vertex into the current cascade's light space
    gl_Position = u_shadowMatrix * vec4(a_position, 1.0);
}