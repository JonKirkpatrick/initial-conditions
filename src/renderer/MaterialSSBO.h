/**
 * @file MaterialSSBO.h
 * @brief GPU Shader Storage Buffer Object (SSBO) layout and management for material data.
 * 
 * Defines std430-aligned PBR material properties passed to shaders via 
 * Shader Storage Buffer Objects (`GL_SHADER_STORAGE_BUFFER`).
 */

#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <GL/glew.h>

/**
 * @brief Packed 64-byte PBR material property payload for GPU storage.
 * 
 * Standardized std430 memory layout containing surface tint, microfacet rough/metal attributes,
 * UV scale/offset parameters, and pipeline behavior bitmasks.
 */
struct MaterialData {
    // === Block 1: Base Color / Surface Tint ===
    glm::vec4 albedoTint    = glm::vec4(1.0f); ///< Base surface tint multiplier (RGBA).

    // === Block 2: Physically-Based Material Attributes ===
    float roughness         = 0.5f;   ///< Microfacet roughness [0.0 = mirror smooth, 1.0 = rough matte].
    float metallic          = 0.0f;   ///< Metalness factor [0.0 = dielectric, 1.0 = pure metallic].
    float emissiveIntensity = 0.0f;   ///< Self-illumination emission multiplier.
    float specularReflectance = 0.04f; ///< Base dielectric Fresnel reflectivity $F_0$ (standard default $0.04$).

    // === Block 3: Texture Mapping Properties ===
    glm::vec2 uvScale       = glm::vec2(1.0f, 1.0f); ///< UV coordinate tiling multiplier $(U_{scale}, V_{scale})$.
    glm::vec2 uvOffset      = glm::vec2(0.0f, 0.0f); ///< UV coordinate translation offset $(U_{offset}, V_{offset})$.

    // === Block 4: Pipeline Behavior & Logic ===
    uint32_t materialFlags  = 0;      ///< Feature bitmask flags (e.g. Bit 0: Unlit, Bit 1: Two-Sided).
    float spare0            = 0.0f;   ///< Reserved padding element for std430 alignment.
    float spare1            = 0.0f;   ///< Reserved padding element for std430 alignment.
    float spare2            = 0.0f;   ///< Reserved padding element for std430 alignment.
};

// Perfectly aligned to a clean 64 bytes (four 16-byte chunks)
static_assert(sizeof(MaterialData) == 64, "MaterialData alignment mismatch");

/**
 * @brief Wrapper class managing an OpenGL Shader Storage Buffer Object (SSBO) for materials.
 */
class MaterialSSBO {
public:
    /**
     * @brief Constructs SSBO manager and generates OpenGL buffer handle.
     */
    MaterialSSBO() { glGenBuffers(1, &m_ssbo); }

    /**
     * @brief Destructor releasing OpenGL SSBO resources.
     */
    ~MaterialSSBO() { glDeleteBuffers(1, &m_ssbo); }

    /**
     * @brief Uploads a collection of materials to GPU memory (`GL_STATIC_DRAW`).
     * @param materials Vector of `MaterialData` structures to upload.
     */
    void upload(const std::vector<MaterialData>& materials) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, materials.size() * sizeof(MaterialData), materials.data(), GL_STATIC_DRAW);
        m_count = static_cast<int>(materials.size());
    }

    /**
     * @brief Updates a single material entry in the GPU buffer.
     * @param index Zero-based material array index to modify.
     * @param m Updated `MaterialData` structure.
     */
    void updateOne(int index, const MaterialData& m) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * sizeof(MaterialData), sizeof(MaterialData), &m);
    }

    /**
     * @brief Binds the SSBO to a specified indexed shader storage binding point.
     * @param bindingPoint Target shader binding layout slot (defaults to 1).
     */
    void bind(GLuint bindingPoint = 1) const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_ssbo);
    }

    /**
     * @brief Gets total count of materials active in GPU buffer.
     * @return Number of uploaded `MaterialData` elements.
     */
    int count() const { return m_count; }

private:
    GLuint m_ssbo  = 0; ///< OpenGL Shader Storage Buffer Object handle.
    int    m_count = 0; ///< Count of material entries currently allocated on GPU.
};