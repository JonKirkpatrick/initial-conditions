// SpeciesSSBO.h
#pragma once
#include <vector>
#include <glm/glm.hpp>

struct SpeciesData {
    glm::vec4 irisColourAndRadius;       // xyz = irisColour,     w = irisRadius
    glm::vec4 scleraColour;              // xyz = scleraColour,   w = unused
    glm::vec4 tapetumColourAndPresence;  // xyz = tapetumColour,  w = presence (0 or 1)
};

static_assert(sizeof(SpeciesData) == 48, "SpeciesData must be exactly 48 bytes");

class SpeciesSSBO {
public:
    SpeciesSSBO() {
        glGenBuffers(1, &m_ssbo);
    }
    ~SpeciesSSBO() {
        glDeleteBuffers(1, &m_ssbo);
    }

    // Call once at load time
    void upload(const std::vector<SpeciesData>& species) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     species.size() * sizeof(SpeciesData),
                     species.data(),
                     GL_STATIC_DRAW);   // Static, not Dynamic
        m_count = static_cast<int>(species.size());
    }

    // Rarely needed, but handy if you want to hot-reload a species entry
    void updateOne(int index, const SpeciesData& s) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        index * sizeof(SpeciesData),
                        sizeof(SpeciesData),
                        &s);
    }

    void bind(GLuint bindingPoint = 1) const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_ssbo);
    }

    int count() const { return m_count; }

private:
    GLuint m_ssbo  = 0;
    int    m_count = 0;
};