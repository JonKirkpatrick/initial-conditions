#include "HUD.h"
#include <SFML/System.hpp>
#include <cmath>
#include "Assets.h"
#include "Action.hpp"
#include "Theme.h"

HUD::HUD(const sf::Vector2u& windowSize)
{
    init(windowSize);
}

void HUD::init(const sf::Vector2u& windowSize)
{
    float refHeight = 1080.f;
    float scale = static_cast<float>(windowSize.y) / refHeight;

    // 2. Scale your radii
    float baseRadius = 100.f * scale; // Original was m_baseRadius
    float knobRadius = 40.f * scale;
    unsigned int smoothness = static_cast<unsigned int>(10 + baseRadius / 2.f);

    m_joystickBase.setRadius(baseRadius);
    m_joystickBase.setOrigin({baseRadius, baseRadius});
    m_joystickBase.setPointCount(smoothness);
    m_joystickBase.setFillColor(Theme::color("hud-base"));
    m_joystickBase.setOutlineThickness(4.f * scale);
    m_joystickBase.setOutlineColor(Theme::color("hud-outline"));
    
    m_joystickKnob.setRadius(knobRadius);
    m_joystickKnob.setOrigin({knobRadius, knobRadius});
    m_joystickKnob.setPointCount(smoothness);
    m_joystickKnob.setFillColor(Theme::color("hud-knob"));

    // 3. Position relative to window edges (e.g., Bottom-Right)
    m_basePos.x = windowSize.x * 0.85f; 
    m_basePos.y = windowSize.y * 0.85f;

    m_joystickBase.setPosition(m_basePos);
    m_joystickKnob.setPosition(m_basePos);
    
    // Store these scaled values for use in update() logic
    m_currentBaseRadius = baseRadius;
    m_currentKnobRadius = knobRadius;

    // Compass asset-based widget
    // Determine compass scale: full size for QHD+ (1440p+), 3/4 for smaller displays
    m_compassScale = (windowSize.y >= 1440) ? 1.0f : 0.75f;
    float compassDisplayW = 800.f * m_compassScale;  // Base: 800x160, scaled
    float compassDisplayH = 160.f * m_compassScale;
    
    m_compassDisc = std::make_unique<sf::Sprite>(Assets::Instance().getTexture("CompassDisc"));
    m_compassDisc->setScale({m_compassScale, m_compassScale});
    
    m_compassTape = std::make_unique<sf::Sprite>(Assets::Instance().getTexture("CompassTape"));
    m_compassTape->setScale({m_compassScale, m_compassScale});
    m_compassTape->setTextureRect(sf::IntRect({0, 0}, {720, 160}));  // 720px wide texture rect
    // Enable repeating so the texture wraps naturally as we pan the rect
    const_cast<sf::Texture&>(Assets::Instance().getTexture("CompassTape")).setRepeated(true);
    
    m_compassBezel = std::make_unique<sf::Sprite>(Assets::Instance().getTexture("CompassBezel"));
    m_compassBezel->setScale({m_compassScale, m_compassScale});
    
    // Center compass horizontally, position at top
    float compassX = (windowSize.x - compassDisplayW) * 0.5f;
    float compassY = 1.f * scale;
    
    m_compassDisc->setPosition({compassX, compassY});
    m_compassTape->setPosition({compassX + 40.f * m_compassScale, compassY});  // Tape offset for layering
    m_compassBezel->setPosition({compassX, compassY});

    m_minimapRing = std::make_unique<sf::Sprite>(Assets::Instance().getTexture("MapRing"));
    m_headlightIcon = std::make_unique<sf::Sprite>(Assets::Instance().getTexture("HLOff"));
    float headlightY = 300.f;
    float headlightX = windowSize.x - 150.f;
    m_headlightIcon->setPosition({headlightX, headlightY});

}

void HUD::update(sf::RenderWindow& window, const HUD_Data& data)
{
    std::vector<Action> actions;
    sf::Vector2i mouseI = sf::Mouse::getPosition(window);
    sf::Vector2f mousePos = window.mapPixelToCoords(mouseI);
    m_yawDeg = data.cameraYaw;
    m_headlightState = data.headlightState;
    m_headlightEnabled = data.headlightEnabled;
    m_minimapTex = data.minimapTex;
    m_playerWorldPos = data.position;
    m_homeWorldPos = data.homeLocation;
    updateHeadlightIcon();
    
    sf::Vector2f output(0.f, 0.f);

    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
    {
        if (!m_isDragging)
        {
            sf::Vector2f diff = mousePos - m_joystickKnob.getPosition();
            float distSq = diff.x * diff.x + diff.y * diff.y;
            if (distSq < m_currentKnobRadius * m_currentKnobRadius) { m_isDragging = true; }
        }

        if (m_isDragging)
        {
            sf::Vector2f diff = mousePos - m_basePos;
            float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

            if (dist <= m_currentBaseRadius)
            {
                m_joystickKnob.setPosition(mousePos);
                output = diff / m_currentBaseRadius; // Value between 0 and 1
            }
            else
            {
                sf::Vector2f unitDir = diff / dist;
                m_joystickKnob.setPosition(m_basePos + unitDir * m_currentBaseRadius);
                output = unitDir; // Value is 1 (max tilt)
            }
        }
    }
    else
    {
        m_isDragging = false;
        m_joystickKnob.setPosition(m_basePos);
    }
}

void HUD::updateHeadlightIcon()
{
    const char* textureName = "HLAuto";
    switch (m_headlightState)
    {
    case 0: textureName = "HLOff"; break;
    case 1: textureName = "HLOn"; break;
    case 2: 
    default:
        textureName = m_headlightEnabled ? "HLOn" : "HLAuto";
        break;
    }
    m_headlightIcon->setTexture(Assets::Instance().getTexture(textureName));
}

void HUD::drawMiniMap(sf::RenderWindow& window)
{
    if (!m_minimapTex) { return; }

    sf::Vector2u winSize = window.getSize();
    float scale = static_cast<float>(winSize.y) / 1080.f;

    float mapRadius = 92.f * scale;
    unsigned int smoothness = static_cast<unsigned int>(10 + mapRadius / 2.f);
    sf::Vector2f mapCenter(winSize.x - (mapRadius + 28.f * scale), (mapRadius + 28.f * scale));

    sf::CircleShape frame(mapRadius + 4.f * scale);
    frame.setPointCount(smoothness);
    frame.setOrigin({mapRadius + 4.f * scale, mapRadius + 4.f * scale});
    frame.setPosition(mapCenter);
    frame.setFillColor(sf::Color(244, 238, 224, 235));
    frame.setOutlineThickness(2.f * scale);
    frame.setOutlineColor(sf::Color(38, 40, 42, 220));
    window.draw(frame);

    sf::CircleShape mapCircle(mapRadius);
    mapCircle.setPointCount(smoothness);
    mapCircle.setOrigin({mapRadius, mapRadius});
    mapCircle.setPosition(mapCenter);
    mapCircle.setRotation(sf::degrees(-m_yawDeg - 180.f));
    mapCircle.setTexture(m_minimapTex, true);
    mapCircle.setFillColor(sf::Color::White);
    window.draw(mapCircle);

    sf::CircleShape playerDot(3.5f * scale);
    playerDot.setOrigin({3.5f * scale, 3.5f * scale});
    playerDot.setPosition(mapCenter);
    playerDot.setFillColor(sf::Color(255, 248, 220, 240));
    playerDot.setOutlineThickness(1.5f * scale);
    playerDot.setOutlineColor(sf::Color(30, 34, 28, 220));
    window.draw(playerDot);

    // Optional: Add a ring overlay for aesthetics
    sf::Sprite ring(*m_minimapRing);
    ring.setOrigin({150.f, 150.f});
    ring.setPosition(mapCenter);
    window.draw(ring);
}

void HUD::drawHeadlightWidget(sf::RenderWindow& window)
{
    if (m_headlightIcon)
    {
        window.draw(*m_headlightIcon);
    }
}

sf::Vector2f HUD::getJoystickAxis() const
{
    if (!m_isDragging) return { 0.f, 0.f };
    sf::Vector2f diff = m_joystickKnob.getPosition() - m_basePos;
    return diff / m_currentBaseRadius; // Normalize to range [-1, 1]
}

void HUD::drawCompass(sf::RenderWindow& window, float yawDeg)
{
    // Tape metrics:
    // - Total tape width: 3452 pixels
    // - 360 degrees total
    // - Pixels per degree: 3452 / 360 ≈ 9.5889
    // - Visible window: 720 pixels (centered on current yaw)
    
    const float TAPE_TOTAL_WIDTH = 3452.f;
    const float TAPE_TEXTURE_WIDTH = 720.f;
    const float DEGREES_PER_FULL_TAPE = 360.f;
    const float PIXELS_PER_DEGREE = TAPE_TOTAL_WIDTH / DEGREES_PER_FULL_TAPE;
    const float ZERO_YAW_TAPE_LEFT = 1629.f;
    
    // Normalize yaw to [0, 360)
    float heading = std::fmod(yawDeg, 360.f);
    if (heading < 0.f) heading += 360.f;
    
    // Anchor the tape at the calibrated zero-yaw pixel, then flip direction and phase by 180 degrees.
    float textureRectLeft = ZERO_YAW_TAPE_LEFT + (heading + 180.f) * PIXELS_PER_DEGREE;
    
    // Set texture rect for tape layer (texture repeating handles wrapping)
    m_compassTape->setTextureRect(sf::IntRect(
        {static_cast<int>(textureRectLeft), 0},
        {720, 160}
    ));
    
    // Draw layers: disc (base) → tape (middle) → bezel (top)
    window.draw(*m_compassDisc);
    window.draw(*m_compassTape);
    window.draw(*m_compassBezel);
}

void HUD::render(sf::RenderWindow& window, bool secondPass)
{
    // Joystick — only when visible
    if (m_joystickVisible) {
        window.draw(m_joystickBase);
        window.draw(m_joystickKnob);
    }

    drawHeadlightWidget(window);
    drawMiniMap(window);
    drawCompass(window, m_yawDeg);
}