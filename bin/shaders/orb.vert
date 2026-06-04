// orb.vert
uniform mat4 projectionMatrix;     // You can get this from SFML or calculate it
uniform mat4 viewMatrix;           // Same

uniform vec3 u_orbCenterView[256];   // Increase batch size
uniform vec4 u_orbColor[256];
uniform float u_orbDepthNorm[256];
uniform vec2 u_quadOrigin[256];      // Maybe not needed anymore
uniform vec2 u_texSize[256];

uniform int u_batchSize;

attribute vec2 position;             // The quad corner from sf::Vertex (in pixels)
attribute vec4 color;                // We'll repurpose this for instance index

varying vec4 v_color;
varying float v_depthNorm;
varying vec2 v_texCoord;
varying vec3 v_centerView;

void main()
{
    int instanceID = int(color.r);   // We're packing instance index into the red channel

    vec3 center = u_orbCenterView[instanceID];
    vec4 orbColor = u_orbColor[instanceID];
    float depthNorm = u_orbDepthNorm[instanceID];

    // Reconstruct the local quad offset
    vec2 offset = (position - u_quadOrigin[instanceID]) / u_texSize[instanceID] * 2.0 - 1.0;

    // Simple billboard in view space
    vec3 right = vec3(1.0, 0.0, 0.0);
    vec3 up    = vec3(0.0, 1.0, 0.0);
    vec3 worldPos = center + right * offset.x * /* radius in view space */ + up * offset.y * /* radius */;

    gl_Position = projectionMatrix * vec4(worldPos, 1.0);

    v_color = orbColor;
    v_depthNorm = depthNorm;
    v_texCoord = (position - u_quadOrigin[instanceID]) / u_texSize[instanceID];
    v_centerView = center;
}