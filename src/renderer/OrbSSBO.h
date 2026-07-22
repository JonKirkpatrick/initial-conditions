#pragma once
#include <vector>
#include <algorithm>
#include <GL/glew.h>
#include <glm/glm.hpp>

struct OrbData {
    glm::vec4 centreAndSpeciesIdx;
    glm::vec4 forwardAndRadius;
    glm::vec4 rightPadded;
    glm::vec4 upPadded;
    glm::vec4 gazeDirDilationAndEyelidClosure;
};
static_assert(sizeof(OrbData) == 80, "OrbData must be exactly 80 bytes");

class OrbSSBO {
public:
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

    ~OrbSSBO() {
        if (m_ssbo) {
            glDeleteBuffers(1, &m_ssbo);
        }
    }

    // Prevent accidental copying
    OrbSSBO(const OrbSSBO&) = delete;
    OrbSSBO& operator=(const OrbSSBO&) = delete;

    void update(const OrbData* data, std::size_t count) 
    {
        if (count == 0 || !data) 
        {
            m_activeCount = 0;
            return;
        }

        m_activeCount = std::min(count, m_capacity);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        
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

    void bind(GLuint bindingPoint = 0) const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_ssbo);
    }

    int count() const { return static_cast<int>(m_activeCount); }
    GLuint handle() const { return m_ssbo; }

private:
    GLuint      m_ssbo = 0;
    std::size_t m_capacity = 0;
    std::size_t m_activeCount = 0;
};