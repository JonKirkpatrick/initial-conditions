/**
 * @file SpeciesSSBO.h
 * @brief GPU Shader Storage Buffer Object (SSBO) layout and management for species-specific ocular properties.
 * 
 * Defines std430-aligned ocular species parameters (iris color/radius, sclera tint, and tapetum 
 * lucidum eye-shine properties) uploaded to the GPU for procedural eye and creature rendering shaders.
 */

#pragma once
#include <vector>
#include <glm/glm.hpp>

/**
 * @brief Packed 48-byte species ocular property payload for GPU storage shaders.
 * 
 * Standardized std430 memory layout containing iris dimensions, scleral color tints,
 * and tapetum lucidum presence/color metrics.
 */
struct SpeciesData {
    glm::vec4 irisColourAndRadius;       ///< XYZ iris RGB color tint + W relative iris radius scale.
    glm::vec4 scleraColour;              ///< XYZ sclera RGB color tint + W reserved padding slot.
    glm::vec4 tapetumColourAndPresence;  ///< XYZ tapetum lucidum eye-shine RGB color + W presence flag [0.0 = absent, 1.0 = present].
};

static_assert(sizeof(SpeciesData) == 48, "SpeciesData must be exactly 48 bytes");

/**
 * @brief Wrapper class managing an OpenGL Shader Storage Buffer Object (SSBO) for species eye parameters.
 */
class SpeciesSSBO {
public:
    /**
     * @brief Constructs SSBO manager and generates OpenGL buffer handle.
     */
    SpeciesSSBO() {
        glGenBuffers(1, &m_ssbo);
    }

    /**
     * @brief Destructor releasing OpenGL SSBO resources.
     */
    ~SpeciesSSBO() {
        glDeleteBuffers(1, &m_ssbo);
    }

    /**
     * @brief Uploads species definition table to GPU memory (`GL_STATIC_DRAW`).
     * 
     * Intended to be invoked once during system initialization or scene setup.
     * @param species Vector of `SpeciesData` structures to upload.
     */
    void upload(const std::vector<SpeciesData>& species) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     species.size() * sizeof(SpeciesData),
                     species.data(),
                     GL_STATIC_DRAW);   // Static, not Dynamic
        m_count = static_cast<int>(species.size());
    }

    /**
     * @brief Updates a single species entry in the GPU buffer.
     * 
     * Useful for live material tweaking or asset hot-reloading at runtime.
     * @param index Zero-based species array index to modify.
     * @param s Updated `SpeciesData` structure.
     */
    void updateOne(int index, const SpeciesData& s) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        index * sizeof(SpeciesData),
                        sizeof(SpeciesData),
                        &s);
    }

    /**
     * @brief Binds the SSBO to a specified indexed shader storage binding point.
     * @param bindingPoint Target shader storage binding layout slot (defaults to 1).
     */
    void bind(GLuint bindingPoint = 1) const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_ssbo);
    }

    /**
     * @brief Gets total count of species definitions active in GPU memory.
     * @return Number of uploaded `SpeciesData` elements.
     */
    int count() const { return m_count; }

private:
    GLuint m_ssbo  = 0; ///< OpenGL Shader Storage Buffer Object handle.
    int    m_count = 0; ///< Count of species entries currently allocated on GPU.
};