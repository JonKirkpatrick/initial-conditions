#pragma once

#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/OpenGL.hpp>
#include "AudioManager.h"
#include "SpeciesSSBO.h"
#include "MaterialSSBO.h"
           
#include <map>
#include <cassert>
#include <iostream>
#include <fstream>

struct RowConfigData {
    bool advanceRow;
    size_t xOffset;
    size_t frameWidth;
    size_t frameHeight;
    std::string action;
    size_t frameCount;
    std::vector<size_t> frames;
    std::vector<size_t> offsets;
};

struct SpriteSheetDescriptor {
    std::string textureName;
    std::vector<RowConfigData> rowConfigs;
};

struct SpeciesTexturePaths
{
    std::string diffuse;
    std::string normal;
};

class Assets
{
    std::map<std::string, sf::Texture>                  m_textureMap;
    std::map<std::string, sf::Font>                     m_fontMap;
    std::map<std::string, sf::SoundBuffer>              m_soundBufferMap;
    std::map<std::string, sf::Sound>                    m_soundMap;
    std::map<std::string, std::unique_ptr<sf::Music>>   m_musicMap;
    std::map<std::string, std::unique_ptr<sf::Shader>>  m_shaderMap;
    std::map<std::string, GLuint>                       m_glProgramMap;
    std::vector<SpeciesTexturePaths>                    m_speciesTexturePaths;
    SpeciesSSBO                                         m_speciesSSBO;
    MaterialSSBO                                        m_materialSSBO;
    GLuint                                              m_speciesDiffuseArray = 0;
    GLuint                                              m_speciesNormalArray = 0;
    std::map<std::string, int>                          m_speciesIdMap;
    std::vector<SpeciesData>                            m_speciesRegistry;

    void addTexture(const std::string& textureName, const std::string& path, bool smooth = false);
    void buildSpeciesTextureArrays();
    void releaseSpeciesTextures();
    void addFont(const std::string& fontName, const std::string& path);
    void addSound(const std::string& soundName, const std::string& path);
    void addMusic(const std::string& musicName, const std::string& path);
    void addShader(const std::string& shaderName, const std::string& path);
    void addGLProgram(const std::string& name, const std::string& vertPath, const std::string& fragPath);
    void addSpecies(const std::string& speciesName, const SpeciesData& speciesData);

    Assets() = default;

private:
    std::string preprocessShaderIncludes(const std::string& filePath);
    std::string preprocessShaderIncludesInternal(const std::string& filePath, std::unordered_set<std::string>& includeStack);
    std::map<std::string, GLuint> m_cubemapMap; 
    

public:

    struct CubeFaceSpec {
        const char* suffix;
        GLenum target;
    };

    bool readRawHalfFile(const std::filesystem::path& path, int& width, int& height, std::vector<uint16_t>& pixels);
    static Assets& Instance();
    Assets(const Assets&) = delete;
    Assets& operator=(const Assets&) = delete;

    void loadFromFile(const std::string& path);
    void loadFromSpeciesJSON(const std::string& path);
    void loadFromMaterialJSON(const std::string& path);
    void finalizeSpeciesBuffer();

    const sf::Texture& getTexture(const std::string& textureName) const;
    const sf::Font& getFont(const std::string& fontName) const;
    sf::Sound& getSound(const std::string& soundName);
    sf::Music& getMusic(const std::string& musicName);
    sf::Shader& getShader(const std::string& shaderName);
    void addCubemap(const std::string& name, const std::filesystem::path& folderPath);
    GLuint getCubemap(const std::string& name) const;
    GLuint getGLProgram(const std::string& name) const;
    GLuint getSpeciesDiffuseArray() const { return m_speciesDiffuseArray; }
    GLuint getSpeciesNormalArray()  const { return m_speciesNormalArray;  } 
    const std::map<std::string, sf::Texture>& getTextures() const;
    int getSpeciesId(const std::string& speciesName) const;
    const SpeciesSSBO& getSpeciesSSBO() const {
        return m_speciesSSBO;
    };
    const MaterialSSBO& getMaterialSSBO() const {
        return m_materialSSBO;
    };
};