#pragma once
#include <glm/glm.hpp>

struct alignas(16) CameraBlock {
    // --- 16-byte Aligned Types (Matrices) ---
    glm::mat4 view;         // 64 bytes
    glm::mat4 proj;         // 64 bytes
    glm::mat4 viewProj;     // 64 bytes
    glm::mat4 invViewProj;  // 64 bytes

    // --- 16-byte Vectors (Packed naturally) ---
    glm::vec3 cameraPos;    // 12 bytes
    float     fovY;         // 4 bytes  <-- Packs perfectly into the 4th slot of cameraPos's vec4

    glm::vec3 cameraForward;// 12 bytes
    float     aspectRatio;  // 4 bytes  <-- Packs perfectly with forward

    glm::vec3 cameraRight;  // 12 bytes
    float     cameraHeight; // 4 bytes  <-- Packs perfectly with right (holds getCameraHeightAboveGround)

    glm::vec3 cameraUp;     // 12 bytes
    float     farPlane;     // 4 bytes  <-- Packs perfectly with up

    // --- 8-byte and 4-byte remaining variables ---
    glm::vec2 viewportSize; // 8 bytes (x, y)
    float     nearPlane;    // 4 bytes
    float     padding;      // 4 bytes  <-- Explicitly completes the final 16-byte chunk
};

struct alignas(16) EnvironmentBlock {
    // --- 16-byte Aligned Vectors ---
    glm::vec4 sunColor;         // 16 bytes (r, g, b, a)
    glm::vec3 sunDirection;     // 12 bytes
    float     ambientStrength;  // 4 bytes  <-- Packs cleanly into the 4th slot of sunDirection

    glm::vec3 moonDirection;    // 12 bytes
    float     skyExposure;      // 4 bytes  <-- Packs cleanly into the 4th slot of moonDirection
};

struct alignas(16) AtmosphereBlock {
    // --- 16-byte Aligned Memory Zones ---
    glm::vec4 fogColorDay;          // 16 bytes (r, g, b, alpha padding)
    glm::vec4 fogColorNight;        // 16 bytes (r, g, b, alpha padding)
    
    float     fogDensity;           // 4 bytes
    float     fogBaseHeight;        // 4 bytes
    float     fogHeightFalloff;     // 4 bytes
    float     _padding;             // 4 bytes (Explicitly round out to a 16-byte block)
};