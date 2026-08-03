#pragma once

#include <GL/glew.h>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>

class GLProgramRegistry
{
public:
    static GLProgramRegistry& Instance();

    // Loads the combined program manifest & layout JSON file
    bool loadManifest(const std::filesystem::path& manifestPath);

    // Retrieves a compiled GL program handle
    GLuint getProgram(const std::string& name) const;

    // Explicit cleanup when GL context terminates
    void shutdown();

private:
    GLProgramRegistry() = default;
    ~GLProgramRegistry() = default;

    std::string preprocessShaderSource(const std::filesystem::path& filePath, const std::string& targetName);
    GLuint compileShader(GLenum type, const std::filesystem::path& path, const std::string& targetName);
    GLuint linkProgram(const std::string& name, const std::filesystem::path& vertPath, const std::filesystem::path& fragPath);

    std::unordered_map<std::string, GLuint> m_programs;
    nlohmann::json m_manifestData;
};