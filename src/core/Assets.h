#pragma once

#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/OpenGL.hpp>
#include "audio/AudioManager.h"
#include "renderer/SpeciesSSBO.h"
#include "renderer/MaterialSSBO.h"
           
#include <map>
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <vector>
#include <string>
#include <memory>

/**
 * @brief Configuration data for individual animation rows within a spritesheet.
 */
struct RowConfigData {
    bool advanceRow;            ///< Whether frame indexing advances vertically to the next row.
    size_t xOffset;             ///< Horizontal pixel offset where the row starts.
    size_t frameWidth;          ///< Width of each individual animation frame in pixels.
    size_t frameHeight;         ///< Height of each individual animation frame in pixels.
    std::string action;         ///< Action name associated with this row (e.g., "WALK", "ATTACK").
    size_t frameCount;          ///< Total number of frames in this row configuration.
    std::vector<size_t> frames;  ///< Array of specific frame indices used by the animation.
    std::vector<size_t> offsets; ///< Frame offset metadata for precise sub-pixel positioning.
};

/**
 * @brief Describes a full spritesheet layout and its associated texture.
 */
struct SpriteSheetDescriptor {
    std::string textureName;                 ///< Identifier of the underlying texture asset.
    std::vector<RowConfigData> rowConfigs;   ///< Configuration entries for each animation row.
};

/**
 * @brief Pair of texture paths representing diffuse and normal maps for species assets.
 */
struct SpeciesTexturePaths
{
    std::string diffuse; ///< File path to the diffuse texture map.
    std::string normal;  ///< File path to the normal texture map.
};

/**
 * @brief Pair of texture paths representing diffuse and normal maps for terrain assets.
 */
struct TerrainTexturePaths
{
    std::string diffuse; ///< File path to the diffuse texture map.
    std::string normal;  ///< File path to the normal texture map.
};

/**
 * @brief Monolithic CPU/GPU floating-point heightfield buffer.
 * 
 * Holds heightmap elevation data in a flat CPU array for fast height queries 
 * and mirrors it to an OpenGL single-channel floating-point texture (`GL_R32F`)
 * for GPU sampling.
 * 
 * @note Serves as a temporary shim prior to tiled sampler2DArray implementation.
 */
struct HeightArray
{
    int width = 0;             ///< Width of the heightfield grid.
    int height = 0;            ///< Height of the heightfield grid.
    std::vector<float> data;   ///< Row-major float height storage (size == width * height).
    GLuint textureId = 0;      ///< OpenGL `GL_TEXTURE_2D` texture handle (`GL_R32F`).

    /**
     * @brief Samples the elevation at specific grid coordinates.
     * @param x Column index in range [0, width - 1].
     * @param y Row index in range [0, height - 1].
     * @return The height scalar value at specified coordinate.
     */
    float sample(int x, int y) const
    {
        assert(x >= 0 && x < width && y >= 0 && y < height);
        return data[static_cast<size_t>(y) * width + x];
    }
};

/**
 * @brief Centralized resource manager singleton for textures, sounds, shaders, and SSBOs.
 * 
 * The Assets class owns and manages all static and runtime resources including 
 * SFML media objects, OpenGL shaders, 2D texture arrays, skybox cubemaps, 
 * species registries, and SSBO buffers for rendering systems.
 */
class Assets
{
    std::map<std::string, sf::Texture>                  m_textureMap;          ///< Map of loaded 2D textures keyed by name.
    std::map<std::string, HeightArray>                  m_heightArrayMap;      ///< Map of loaded CPU/GPU heightfields.
    std::map<std::string, sf::Font>                     m_fontMap;             ///< Map of loaded UI fonts.
    std::map<std::string, sf::SoundBuffer>              m_soundBufferMap;      ///< Map of loaded audio buffers.
    std::map<std::string, sf::Sound>                    m_soundMap;            ///< Map of active sound emitters.
    std::map<std::string, std::unique_ptr<sf::Music>>   m_musicMap;            ///< Map of streaming music tracks.
    std::map<std::string, std::unique_ptr<sf::Shader>>  m_shaderMap;           ///< Map of SFML high-level shaders.
    std::map<std::string, GLuint>                       m_glProgramMap;        ///< Map of compiled OpenGL program object handles.
    std::vector<SpeciesTexturePaths>                    m_speciesTexturePaths; ///< File path registry for species texture arrays.
    std::vector<TerrainTexturePaths>                    m_terrainTexturePaths; ///< File path registry for terrain texture arrays.
    GLuint                                              m_terrainDiffuseArray = 0; ///< GL handle for terrain diffuse 2D texture array.
    GLuint                                              m_terrainNormalArray = 0;  ///< GL handle for terrain normal 2D texture array.
    SpeciesSSBO                                         m_speciesSSBO;         ///< Shader Storage Buffer Object for species data.
    MaterialSSBO                                        m_materialSSBO;        ///< Shader Storage Buffer Object for material data.
    GLuint                                              m_speciesDiffuseArray = 0; ///< GL handle for species diffuse 2D texture array.
    GLuint                                              m_speciesNormalArray = 0;  ///< GL handle for species normal 2D texture array.
    std::map<std::string, int>                          m_speciesIdMap;        ///< Maps species string identifiers to numeric IDs.
    std::vector<SpeciesData>                            m_speciesRegistry;     ///< Master array of defined species data.

    void addTexture(const std::string& textureName, const std::string& path, bool smooth = false);
    void addHeightArray(const std::string& heightArrayName, const std::string& path);
    void buildSpeciesTextureArrays();
    void buildTerrainTextureArrays();
    void releaseSpeciesTextures();
    void releaseTerrainTextures();
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
    std::map<std::string, GLuint> m_cubemapMap; ///< Map of loaded GL cubemap handles.

public:

    /**
     * @brief Target mapping specification for loading cube map texture faces.
     */
    struct CubeFaceSpec {
        const char* suffix; ///< File name suffix for the target face (e.g., "_rt", "_lf").
        GLenum target;      ///< OpenGL cube map target enum (e.g., `GL_TEXTURE_CUBE_MAP_POSITIVE_X`).
    };

    /**
     * @brief Reads a 16-bit half-float binary raster file into host memory.
     * @param path File path to the raw binary file.
     * @param[out] width Received pixel grid width.
     * @param[out] height Received pixel grid height.
     * @param[out] pixels Destination buffer for raw 16-bit unsigned pixel values.
     * @return `true` if file read succeeded, `false` otherwise.
     */
    bool readRawHalfFile(const std::filesystem::path& path, int& width, int& height, std::vector<uint16_t>& pixels);

    /**
     * @brief Reads a 32-bit float binary raster file into host memory.
     * @param path File path to the raw binary file.
     * @param[out] width Received pixel grid width.
     * @param[out] height Received pixel grid height.
     * @param[out] values Destination buffer for 32-bit floating point values.
     * @return `true` if file read succeeded, `false` otherwise.
     */
    bool readRawFloatFile(const std::filesystem::path& path, int& width, int& height, std::vector<float>& values);

    /**
     * @brief Gets the thread-safe global instance of the Assets manager.
     * @return Reference to the global Assets singleton instance.
     */
    static Assets& Instance();

    Assets(const Assets&) = delete;
    Assets& operator=(const Assets&) = delete;

    /// @name Asset Initialization & Loading
    /// @{

    /**
     * @brief Loads assets from a master configuration asset file.
     * @param path Path to asset specification file.
     */
    void loadFromFile(const std::string& path);

    /**
     * @brief Parses and registers species definitions from a JSON configuration file.
     * @param path Path to species JSON file.
     */
    void loadFromSpeciesJSON(const std::string& path);

    /**
     * @brief Parses and registers material definitions from a JSON configuration file.
     * @param path Path to material JSON file.
     */
    void loadFromMaterialJSON(const std::string& path);

    /**
     * @brief Flushes pending species registrations into GPU SSBO structures and builds texture arrays.
     */
    void finalizeSpeciesBuffer();

    /// @}

    /// @name Asset Loaders & Accessors
    /// @{

    /**
     * @brief Retrieves a 2D texture by name.
     * @param textureName Registered texture identifier.
     * @return Const reference to loaded `sf::Texture`.
     */
    const sf::Texture& getTexture(const std::string& textureName) const;

    /**
     * @brief Retrieves a HeightArray by name.
     * @param heightArrayName Registered heightfield identifier.
     * @return Const reference to `HeightArray`.
     */
    const HeightArray& getHeightArray(const std::string& heightArrayName) const;

    /**
     * @brief Retrieves a font by name.
     * @param fontName Registered font identifier.
     * @return Const reference to `sf::Font`.
     */
    const sf::Font& getFont(const std::string& fontName) const;

    /**
     * @brief Retrieves a sound instance by name.
     * @param soundName Registered sound identifier.
     * @return Reference to `sf::Sound`.
     */
    sf::Sound& getSound(const std::string& soundName);

    /**
     * @brief Retrieves a music stream by name.
     * @param musicName Registered music track identifier.
     * @return Reference to `sf::Music`.
     */
    sf::Music& getMusic(const std::string& musicName);

    /**
     * @brief Retrieves an SFML shader wrapper by name.
     * @param shaderName Registered shader identifier.
     * @return Reference to `sf::Shader`.
     */
    sf::Shader& getShader(const std::string& shaderName);

    /**
     * @brief Loads 6 cube face textures from a folder and registers a GL Cubemap texture handle.
     * @param name Name identifier to assign to the cubemap.
     * @param folderPath Path to folder containing cube face images.
     */
    void addCubemap(const std::string& name, const std::filesystem::path& folderPath);

    /**
     * @brief Retrieves an OpenGL Cubemap handle by name.
     * @param name Registered cubemap identifier.
     * @return OpenGL texture handle (`GLuint`).
     */
    GLuint getCubemap(const std::string& name) const;

    /**
     * @brief Retrieves a compiled OpenGL shader program handle by name.
     * @param name Registered GL program identifier.
     * @return OpenGL program handle (`GLuint`).
     */
    GLuint getGLProgram(const std::string& name) const;

    /// @}

    /// @name GPU Buffer & Array Handles
    /// @{

    /**
     * @brief Gets the OpenGL texture handle for the species diffuse 2D texture array.
     * @return `GLuint` handle to `GL_TEXTURE_2D_ARRAY`.
     */
    GLuint getSpeciesDiffuseArray() const { return m_speciesDiffuseArray; }

    /**
     * @brief Gets the OpenGL texture handle for the species normal 2D texture array.
     * @return `GLuint` handle to `GL_TEXTURE_2D_ARRAY`.
     */
    GLuint getSpeciesNormalArray()  const { return m_speciesNormalArray;  } 

    /**
     * @brief Gets the OpenGL texture handle for the terrain diffuse 2D texture array.
     * @return `GLuint` handle to `GL_TEXTURE_2D_ARRAY`.
     */
    GLuint getTerrainDiffuseArray() const { return m_terrainDiffuseArray; }

    /**
     * @brief Gets the OpenGL texture handle for the terrain normal 2D texture array.
     * @return `GLuint` handle to `GL_TEXTURE_2D_ARRAY`.
     */
    GLuint getTerrainNormalArray()  const { return m_terrainNormalArray;  }

    /**
     * @brief Accesses the internal texture map container.
     * @return Const reference to map of loaded textures.
     */
    const std::map<std::string, sf::Texture>& getTextures() const;

    /**
     * @brief Queries the numeric identifier for a registered species name.
     * @param speciesName Registered string identifier of the species.
     * @return Integer ID corresponding to the species.
     */
    int getSpeciesId(const std::string& speciesName) const;

    /**
     * @brief Accesses the species SSBO wrapper.
     * @return Const reference to `SpeciesSSBO`.
     */
    const SpeciesSSBO& getSpeciesSSBO() const {
        return m_speciesSSBO;
    };

    /**
     * @brief Accesses the material SSBO wrapper.
     * @return Const reference to `MaterialSSBO`.
     */
    const MaterialSSBO& getMaterialSSBO() const {
        return m_materialSSBO;
    };

    /// @}
};