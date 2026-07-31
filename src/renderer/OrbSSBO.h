/**
 * @file OrbSSBO.h
 * @brief GPU Shader Storage Buffer Object (SSBO) layout and streaming manager for ocular orb instances.
 * 
 * Manages streaming instance data for dynamic eyes/orbs (gaze directions, pupil dilations, eyelid closures,
 * coordinate orientation basis vectors, and species indexing) passed to procedural eye rendering shaders.
 */

#pragma once
#include <vector>
#include <algorithm>
#include <GL/glew.h>
#include <glm/glm.hpp>

/**
 * @brief Packed 80-byte ocular orb instance payload for GPU storage shaders.
 * 
 * Standardized std430 memory layout packing coordinate frame transforms, species metadata, 
 * pupil dilation, and procedural eyelid dynamics.
 */
struct OrbData {
    glm::vec4 centreAndSpeciesIdx;             ///< XYZ orb center position + W species index identifier.
    glm::vec4 forwardAndRadius;                ///< XYZ local forward basis vector + W orb sphere radius.
    glm::vec4 rightPadded;                     ///< XYZ local right basis vector + W reserved padding slot.
    glm::vec4 upPadded;                        ///< XYZ local up basis vector + W reserved padding slot.
    glm::vec4 gazeDirDilationAndEyelidClosure; ///< XY gaze direction offset, Z pupil dilation factor [0,1], W eyelid closure fraction [0,1].
};
static_assert(sizeof(OrbData) == 80, "OrbData must be exactly 80 bytes");

/**
 * @brief Dynamic OpenGL SSBO manager with streaming buffer-orphaning strategy for orb render batches.
 */
class OrbSSBO {
public:
    /**
     * @brief Constructs SSBO manager and pre-allocates fixed GPU capacity.
     * @param maxCapacity Maximum number of `OrbData` instances allocated on the GPU. Defaults to 10,000.
     */
    explicit OrbSSBO(std::size_t maxCapacity = 10000)
        : m_capacity(maxCapacity) 
    {
        glGenBuffers(1, &m_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        
        // Allocate fixed capacity ONCE. Passes nullptr so no CPU data is sent yet.
        glBufferData(GL_SHADER_STORAGE_BUFFER, 
                     m_capacity * sizeof(OrbData), 
                     nullptr, 
                     GL_DYNAMIC_DRAW);
                     
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    /**
     * @brief Destructor releasing OpenGL SSBO buffer handle.
     */
    ~OrbSSBO() {
        if (m_ssbo) {
            glDeleteBuffers(1, &m_ssbo);
        }
    }

    // Prevent accidental copying
    OrbSSBO(const OrbSSBO&) = delete;
    OrbSSBO& operator=(const OrbSSBO&) = delete;

    /**
     * @brief Streams new frame orb instance data to the GPU using buffer-orphaning.
     * 
     * Re-allocates backing storage with `nullptr` to orphan the previous buffer 
     * (avoiding GPU synchronization stalls) and writes active slice via `glBufferSubData`.
     * 
     * @param data Pointer to contiguous array of `OrbData` instances.
     * @param count Number of valid instances to upload.
     */
    void update(const OrbData* data, std::size_t count) 
    {
        if (count == 0 || !data) 
        {
            m_activeCount = 0;
            return;
        }

        m_activeCount = std::min(count, m_capacity);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        
        // Orphan existing GPU buffer memory block
        glBufferData(GL_SHADER_STORAGE_BUFFER, 
                    m_capacity * sizeof(OrbData), 
                    nullptr, 
                    GL_STREAM_DRAW);

        // Write the active slice into the newly orphaned buffer
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                        m_activeCount * sizeof(OrbData), 
                        data);
                        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    /**
     * @brief Binds the SSBO to a specified indexed shader storage binding point.
     * @param bindingPoint Target shader storage binding layout slot (defaults to 0).
     */
    void bind(GLuint bindingPoint = 0) const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_ssbo);
    }

    /**
     * @brief Gets count of active uploaded orb instances in current frame slice.
     * @return Number of active `OrbData` elements.
     */
    int count() const { return static_cast<int>(m_activeCount); }

    /**
     * @brief Gets raw OpenGL handle for SSBO object.
     * @return OpenGL buffer handle (`GLuint`).
     */
    GLuint handle() const { return m_ssbo; }

private:
    GLuint      m_ssbo = 0;         ///< OpenGL Shader Storage Buffer Object handle.
    std::size_t m_capacity = 0;    ///< Maximum capacity in `OrbData` element units.
    std::size_t m_activeCount = 0; ///< Number of valid elements uploaded for current frame.
};