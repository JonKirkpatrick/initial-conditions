// OrbSSBO.h
#pragma once
#include <vector>
#include <glm/glm.hpp>

struct OrbData {
    glm::vec4 centreAndRadius;
    glm::vec4 forwardAndDilation;
    glm::vec4 rightAndEyelidClosure;
    glm::vec4 upPadded;
    glm::vec4 gazeDirPadded;
    glm::vec4 tapetumColourAndPresence;
    glm::vec4 squashAndDirection;
    glm::vec4 irisAndSpeciesIdx;
};
static_assert(sizeof(OrbData) == 128, "OrbData must be exactly 128 bytes");

class OrbSSBO {
public:
    OrbSSBO() {
        glGenBuffers(1, &m_ssbo);
    }

    ~OrbSSBO() {
        glDeleteBuffers(1, &m_ssbo);
    }

    // Call once when orb count changes, or on first upload
    void upload(const std::vector<OrbData>& orbs) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     orbs.size() * sizeof(OrbData),
                     orbs.data(),
                     GL_DYNAMIC_DRAW);
        m_count = orbs.size();
    }

    // Call each frame if orb data has changed
    void update(const std::vector<OrbData>& orbs) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                        orbs.size() * sizeof(OrbData),
                        orbs.data());
    }

    // Call just before your draw call
    void bind(GLuint bindingPoint = 0) const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_ssbo);
    }

    int count() const { return m_count; }

    GLuint handle() const { return m_ssbo; }

private:
    GLuint m_ssbo = 0;
    int    m_count = 0;
};