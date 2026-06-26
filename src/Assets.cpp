#include <GL/glew.h>
#include "Assets.h"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/OpenGL.hpp>
#include <map>
#include <cmath>
#include <cassert>
#include <iostream>
#include <fstream>

static constexpr std::array<Assets::CubeFaceSpec, 6> SKY_CUBEMAP_FACES = {{
    {"right",  GL_TEXTURE_CUBE_MAP_POSITIVE_X},
    {"left",   GL_TEXTURE_CUBE_MAP_NEGATIVE_X},
    {"up",     GL_TEXTURE_CUBE_MAP_POSITIVE_Y},
    {"down",   GL_TEXTURE_CUBE_MAP_NEGATIVE_Y},
    {"back",   GL_TEXTURE_CUBE_MAP_POSITIVE_Z},
    {"front",  GL_TEXTURE_CUBE_MAP_NEGATIVE_Z},
}};

Assets& Assets::Instance()
{
    static Assets assets;
    return assets;
}

void Assets::addCubemap(const std::string& name, const std::filesystem::path& folderPath)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width = 0, height = 0;
    std::vector<uint16_t> pixels;

    // Loop through the 6 specifications, load them, and upload them to the GPU
    for (const auto& face : SKY_CUBEMAP_FACES)
    {
        // Construct the filename dynamically (e.g., "path/to/folder/right.raw")
        std::filesystem::path facePath = folderPath / (std::string(face.suffix) + ".raw");

        if (!readRawHalfFile(facePath, width, height, pixels))
        {
            std::cerr << "Failed to load cubemap face: " << facePath << std::endl;
            glDeleteTextures(1, &textureID);
            return;
        }

        // Upload 16-bit half-float RGB texture data directly to OpenGL
        glTexImage2D(
            face.target, 0, GL_RGB16F, width, height, 0, 
            GL_RGB, GL_HALF_FLOAT, pixels.data()
        );
    }

    // Set cubemap filtering parameters
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Save it in our map!
    m_cubemapMap[name] = textureID;
}

GLuint Assets::getCubemap(const std::string& name) const
{
    assert(m_cubemapMap.find(name) != m_cubemapMap.end());
    return m_cubemapMap.at(name);
}

bool Assets::readRawHalfFile(const std::filesystem::path& path,
                        int& width,
                        int& height,
                        std::vector<uint16_t>& pixels) // Using uint16_t for 16-bit half-floats
{
    // 1. Open the file and check size
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        std::cerr << "Could not open RAW file: " << path << std::endl;
        return false;
    }

    const std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // 2. Calculate dimensions assuming 3 channels (RGB) of 16-bit (2 bytes) data
    // Total bytes = width * height * 3 channels * 2 bytes per channel
    const size_t totalPixels = static_cast<size_t>(fileSize) / (3 * sizeof(uint16_t));
    
    // Assuming square cubemap faces (width == height)
    width = static_cast<int>(std::sqrt(totalPixels));
    height = width;

    // Sanity check to ensure the file size matches a perfect square RGB layout
    if (static_cast<size_t>(width) * static_cast<size_t>(height) * 3 * sizeof(uint16_t) != static_cast<size_t>(fileSize))
    {
        std::cerr << "Error: RAW file size does not match expected square 3-channel layout: " << path << std::endl;
        return false;
    }

    // 3. Read the payload
    pixels.resize(totalPixels * 3);
    file.read(reinterpret_cast<char*>(pixels.data()), fileSize);
    
    if (!file)
    {
        std::cerr << "Could not read RAW pixel payload: " << path << std::endl;
        return false;
    }

    return true;
}

void Assets::addTexture(const std::string& textureName, const std::string& path, bool smooth)
{
    m_textureMap[textureName] = sf::Texture();

    if (!m_textureMap[textureName].loadFromFile(path))
    {
        std::cerr << "Could not load texture file: " << path << std::endl;
        m_textureMap.erase(textureName);
    }
    else
    {
        m_textureMap[textureName].setSmooth(smooth);
    }
}

void Assets::addFont(const std::string& fontName, const std::string& path)
{
    m_fontMap[fontName] = sf::Font();
    if (!m_fontMap[fontName].openFromFile(path))
    {
        std::cerr << "Could not load font file: " << path << std::endl;
        m_fontMap.erase(fontName);
    }
}

void Assets::loadFromFile(const std::string& path)
{
    std::ifstream file(path);
    std::string str;
    while (file.good())
    {
        file >> str;
           
        if (str == "Texture")
        {
            std::string name, path;
            file >> name >> path;
            addTexture(name, path, false);
        }
        else if (str == "Font")
        {
            std::string name, path;
            file >> name >> path;
            addFont(name, path);
        }
        else if (str == "Sound")
        {
            std::string name, path;
            file >> name >> path;
            addSound(name, path);
        }
        else if (str == "Music")
        {
            std::string name, path;
            file >> name >> path;
            addMusic(name, path);
        }
        else if (str == "Shader")
        {
            std::string name, path;
            file >> name >> path;
            addShader(name, path);
            std::cout << "Loaded shader: " << name << " from " << path << std::endl;
        }
        else if (str == "Species")
        {
            std::string name;
            SpeciesData data;
            
            file >> name;
            // Read Iris configuration
            file >> data.irisColourAndRadius.x >> data.irisColourAndRadius.y 
                >> data.irisColourAndRadius.z >> data.irisColourAndRadius.w;
                
            // Read Sclera configuration
            file >> data.scleraColour.x >> data.scleraColour.y 
                >> data.scleraColour.z >> data.scleraColour.w;
                
            // Read Tapetum configuration
            file >> data.tapetumColourAndPresence.x >> data.tapetumColourAndPresence.y 
                >> data.tapetumColourAndPresence.z >> data.tapetumColourAndPresence.w;

            addSpecies(name, data);
            std::cout << "Loaded species parameters for: " << name << std::endl;
        } 
        else if (str == "Cubemap")
        {
            std::string name, folderPath;
            file >> name >> folderPath;
            addCubemap(name, folderPath);
            std::cout << "Loaded cubemap: " << name << " from " << folderPath << std::endl;
        }
        else if (str == "GLProgram")
        {
            std::string name, vertPath, fragPath;
            file >> name >> vertPath >> fragPath;
            addGLProgram(name, vertPath, fragPath);
            std::cout << "Loaded GL program: " << name << std::endl;
        }
        else
        {
            std::cerr << "Unknown Asset Type: " << str << std::endl;
        }
    }
}

const sf::Texture& Assets::getTexture(const std::string& textureName) const
{
    assert(m_textureMap.find(textureName) != m_textureMap.end());
    return m_textureMap.at(textureName);
}
           
const sf::Font& Assets::getFont(const std::string& fontName) const
{
    assert(m_fontMap.find(fontName) != m_fontMap.end());
    return m_fontMap.at(fontName);
}

const std::map<std::string, sf::Texture>& Assets::getTextures() const
{
    return m_textureMap;
}

void Assets::addSound(const std::string& soundName, const std::string& path)
{
    auto [bufIt, bufInserted] = m_soundBufferMap.emplace(soundName, sf::SoundBuffer{});
           
    if (!bufIt->second.loadFromFile(path))
    {
        std::cerr << "Could not load sound file: " << path << std::endl;
        if (bufInserted) { m_soundBufferMap.erase(bufIt); }
        return;
    }

    auto [sndIt, sndInserted] =
        m_soundMap.emplace(std::piecewise_construct,
            std::forward_as_tuple(soundName),
            std::forward_as_tuple(bufIt->second));

    AudioManager::Instance().sfx.registerSound(&sndIt->second);
}

void Assets::addMusic(const std::string& name, const std::string& path)
{
    auto m = std::make_unique<sf::Music>();
    if (!m->openFromFile(path)) return;
    AudioManager::Instance().music.registerMusic(m.get());
    m_musicMap.emplace(name, std::move(m));
}

void Assets::addShader(const std::string& shaderName, const std::string& path)
{
    auto shader = std::make_unique<sf::Shader>();
    
    // Preprocess #include directives
    std::string processedSource = preprocessShaderIncludes(path);
    
    if (!shader->loadFromMemory(processedSource, sf::Shader::Type::Fragment)) return;
    m_shaderMap.emplace(shaderName, std::move(shader));
}

std::string Assets::preprocessShaderIncludes(const std::string& filePath)
{
    std::ifstream file(filePath);
    std::string result;
    std::string line;
    std::string baseDir = filePath.substr(0, filePath.find_last_of("/\\") + 1);
    
    while (std::getline(file, line)) {
        // Check for #include "filename"
        size_t includePos = line.find("#include");
        if (includePos != std::string::npos) {
            size_t quoteStart = line.find('"', includePos);
            size_t quoteEnd = line.find('"', quoteStart + 1);
            
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                std::string includePath = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                std::string fullIncludePath = baseDir + includePath;
                
                // Read the included file
                std::ifstream includeFile(fullIncludePath);
                if (includeFile.good()) {
                    std::string includedContent((std::istreambuf_iterator<char>(includeFile)),
                                                 std::istreambuf_iterator<char>());
                    result += includedContent + "\n";
                    continue;
                }
            }
        }
        
        result += line + "\n";
    }
    
    return result;
}

void Assets::addGLProgram(const std::string& name,
                           const std::string& vertPath,
                           const std::string& fragPath)
{
    auto compile = [&](GLenum type, const std::string& path) -> GLuint {
        std::string src = preprocessShaderIncludes(path);
        const char* c = src.c_str();
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &c, nullptr);
        glCompileShader(s);
        GLint ok;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(s, 1024, nullptr, log);
            std::cerr << "Shader compile error (" << path << "):\n" 
                      << log << std::endl;
        }
        return s;
    };

    GLuint vert = compile(GL_VERTEX_SHADER,   vertPath);
    GLuint frag = compile(GL_FRAGMENT_SHADER, fragPath);

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, 1024, nullptr, log);
        std::cerr << "Program link error (" << name << "):\n" 
                  << log << std::endl;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    m_glProgramMap[name] = program;
}

void Assets::addSpecies(const std::string& name, const SpeciesData& data)
{
    // The current size of the registry vector naturally becomes the ID (0, 1, 2...)
    int assignedId = static_cast<int>(m_speciesRegistry.size());
    m_speciesIdMap[name] = assignedId;
    m_speciesRegistry.push_back(data);
}

int Assets::getSpeciesId(const std::string& name) const
{
    auto it = m_speciesIdMap.find(name);
    assert(it != m_speciesIdMap.end() && "Species name not found!");
    return it->second;
}

void Assets::finalizeSpeciesBuffer()
{
    if (!m_speciesRegistry.empty())
    {
        m_speciesSSBO.upload(m_speciesRegistry);
        std::cout << "Successfully generated Species SSBO with " 
                  << m_speciesRegistry.size() << " entries.\n";
        
        // Optional: Clear out the CPU vector if you don't intend to use updateOne() 
        // down the line, saving a bit of host memory.
    }
}

GLuint Assets::getGLProgram(const std::string& name) const
{
    assert(m_glProgramMap.find(name) != m_glProgramMap.end());
    return m_glProgramMap.at(name);
}

sf::Sound& Assets::getSound(const std::string& soundName)
{
    assert(m_soundMap.find(soundName) != m_soundMap.end());
    return m_soundMap.at(soundName);
}

sf::Music& Assets::getMusic(const std::string& musicName)
{
    assert(m_musicMap.find(musicName) != m_musicMap.end());
    return *(m_musicMap.at(musicName));
}

sf::Shader& Assets::getShader(const std::string& shaderName)
{
    assert(m_shaderMap.find(shaderName) != m_shaderMap.end());
    return *(m_shaderMap.at(shaderName));
}