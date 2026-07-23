// MaterialSSBO.h
#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <GL/glew.h>

struct MaterialData {
    // === Block 1: Base Color / Surface Tint ===
    glm::vec4 albedoTint    = glm::vec4(1.0f); // Multiplier for texture or flat color

    // === Block 2: Physically-Based Material Attributes ===
    float roughness         = 0.5f;            // 0.0 (smooth/mirror) to 1.0 (rough/matte)
    float metallic          = 0.0f;            // 0.0 (dielectric) to 1.0 (pure metal)
    float emissiveIntensity = 0.0f;            // Self-illumination factor
    float specularReflectance = 0.04f;          // Base specular level for non-metals (standard default is 0.04)

    // === Block 3: Texture Mapping Properties ===
    glm::vec2 uvScale       = glm::vec2(1.0f, 1.0f); // Tiling scales
    glm::vec2 uvOffset      = glm::vec2(0.0f, 0.0f); // Texture shifting

    // === Block 4: Pipeline Behavior & Logic ===
    uint32_t materialFlags  = 0;               // Bitmask (e.g., Bit 0: Unlit, Bit 1: Two-Sided)
    float spare0            = 0.0f;            // Available slots for future expansion
    float spare1            = 0.0f;
    float spare2            = 0.0f;
};

// Perfectly aligned to a clean 64 bytes (four 16-byte chunks)
static_assert(sizeof(MaterialData) == 64, "MaterialData alignment mismatch");

class MaterialSSBO {
public:
    MaterialSSBO() { glGenBuffers(1, &m_ssbo); }
    ~MaterialSSBO() { glDeleteBuffers(1, &m_ssbo); }

    void upload(const std::vector<MaterialData>& materials) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, materials.size() * sizeof(MaterialData), materials.data(), GL_STATIC_DRAW);
        m_count = static_cast<int>(materials.size());
    }

    void updateOne(int index, const MaterialData& m) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * sizeof(MaterialData), sizeof(MaterialData), &m);
    }

    void bind(GLuint bindingPoint = 1) const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_ssbo);
    }

    int count() const { return m_count; }

private:
    GLuint m_ssbo  = 0;
    int    m_count = 0;
};