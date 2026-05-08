#pragma once

#include "Assets.h"
#include "Vec2.hpp"

#include <vector>
#include <SFML/Graphics.hpp>
           
class Animation
{

    std::string m_name = "none";            // name of the animation
    std::string m_textureName = "none";     // name of the texture to get the frames from

    size_t      m_frameWidth;               // width of each frame
    size_t      m_frameHeight;              // height of each frame

    Vec2f       m_originOffset;             // origin offset for drawing
    size_t      m_currentFrame = 0;         // the current frame of animation being played
    size_t      m_frameCount;               // total number of frames in the animation
    size_t      m_frameTimer = 0;           // timer to track frame duration

    std::vector<size_t> m_frameDurations;   // duration of each frame
    std::vector<int> m_frameOffsets;        // vertical offset for each frame

    bool        idle = true;                // is the animation idle or playing
    bool        finishLoop = false;         // flag to indicate if the animation should finish its loop

    sf::IntRect m_textureRect;              // sub-rectangle to draw

public:
           
    Animation() = default;

    Animation(const std::string& name, const std::string& textureName)
        : m_name(name), m_textureName(textureName) { }

    void setFrameSize(size_t width, size_t height)
    {
        m_frameWidth = width;
        m_frameHeight = height;

        m_textureRect.size.x = int(width);
        m_textureRect.size.y = int(height);
    }

    void setFrameSpeed(size_t frame, size_t speed)
    {
        if (frame < m_frameDurations.size())
            m_frameDurations[frame] = speed;
    }

    void setOffset(size_t frame, size_t offset)
    {
        if (frame < m_frameOffsets.size())
            m_frameOffsets[frame] = offset;
    }

    void setOriginOffset(size_t offsetX, size_t offsetY)
    {
        m_originOffset = Vec2f(float(offsetX), float(offsetY));
        m_textureRect.position.x = int(m_originOffset.x);
        m_textureRect.position.y = int(m_originOffset.y);
    }

    void setAnimationSpeed(size_t speed) 
    {  
        for (size_t i = 0; i < m_frameDurations.size(); ++i)
        {
            m_frameDurations[i] = speed;
        }
    }

    void setFrameCount(size_t frameCount) 
    {
        m_frameCount = frameCount;
        m_frameDurations.resize(frameCount, 0);
        m_frameOffsets.resize(frameCount, 0);
    }

    void update()
    {
        if (m_frameCount < 1) { return; }
        
        size_t currentSpeed = m_frameDurations[m_currentFrame];
        
        if (!idle && ++m_frameTimer >= currentSpeed)
        {
            m_frameTimer = 0;
            m_currentFrame = (m_currentFrame + 1) % m_frameCount;
            m_textureRect.position.x = int(m_originOffset.x + m_currentFrame * m_textureRect.size.x);
            m_textureRect.position.y = int(m_originOffset.y);
            if (m_currentFrame == 0 && finishLoop)
            {
                idle = true;
                finishLoop = false;
            }
        }
    }

    void finish()
    {
        finishLoop = true;
    }

    bool finishTriggered()
    {
        return finishLoop;
    }

    void play() { idle = false; }
    void pause() { idle = true; }

    bool isPlaying() const { return !idle; }

    bool hasEnded() const
    {
        return (m_currentFrame >= m_frameCount - 1);
    }

    const std::string& getName() const { return m_name; }

    const std::string& getTextureName() const { return m_textureName; }

    const sf::IntRect& getRect() const { return m_textureRect; }

    sf::Sprite getSprite() const
    {
        sf::Sprite sprite(Assets::Instance().getTexture(m_textureName));

        sprite.setTextureRect(m_textureRect);
        sprite.setOrigin({ 
            m_textureRect.size.x / 2.0f, m_textureRect.size.y / 2.0f - 
            m_frameOffsets[m_currentFrame] 
        });

        return sprite;
    }

    size_t getAnimDuration() const
    {
        size_t total = 0;
        for (size_t i = 0; i < m_frameCount; ++i)
        {
            total += m_frameDurations[i];
        }
        return total;
    }

    size_t getCurrentFrame() const { return m_currentFrame; }

    void setCurrentFrame(size_t frame)
    {
        if (frame < m_frameCount)
        {
            m_currentFrame = frame;
            m_textureRect.position.x = int(m_originOffset.x + m_currentFrame * m_textureRect.size.x);
            m_textureRect.position.y = int(m_originOffset.y);
        }
    }
};