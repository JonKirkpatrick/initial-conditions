#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include "AudioManager.h"
           
#include <map>
#include <cassert>
#include <iostream>
#include <fstream>

class Animation;

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

class Assets
{
    std::map<std::string, sf::Texture>      m_textureMap;
    std::map<std::string, Animation>        m_animationMap;
    std::map<std::string, sf::Font>         m_fontMap;
    std::map<std::string, sf::SoundBuffer>  m_soundBufferMap;
    std::map<std::string, sf::Sound>        m_soundMap;
    std::map<std::string, std::unique_ptr<sf::Music>> m_musicMap;
    std::map<std::string, std::unique_ptr<sf::Shader>> m_shaderMap;

    void addTexture(const std::string& textureName, const std::string& path, bool smooth = false);
    void addAnimation(const std::string& animationName, const std::string& textureName, size_t frameCount = 1, size_t speed = 0);
    void addFont(const std::string& fontName, const std::string& path);
    void addSound(const std::string& soundName, const std::string& path);
    void addMusic(const std::string& musicName, const std::string& path);
    void addShader(const std::string& shaderName, const std::string& path);

    Assets() = default;

public:

    static Assets& Instance();
    Assets(const Assets&) = delete;
    Assets& operator=(const Assets&) = delete;

    void loadFromFile(const std::string& path);
    void buildAnimationsFromDescriptor(const SpriteSheetDescriptor& desc);

    const sf::Texture& getTexture(const std::string& textureName) const;
    const Animation& getAnimation(const std::string& animationName) const;
    const sf::Font& getFont(const std::string& fontName) const;
    sf::Sound& getSound(const std::string& soundName);
    sf::Music& getMusic(const std::string& musicName);
    sf::Shader& getShader(const std::string& shaderName);

    const std::map<std::string, sf::Texture>& getTextures() const;
    const std::map<std::string, Animation>& getAnimations() const;

};