#include "Assets.h"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <map>
#include <cassert>
#include <iostream>
#include <fstream>

Assets& Assets::Instance()
{
    static Assets assets;
    return assets;
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