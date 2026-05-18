#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <array>
#include <memory>
#include "Assets.h"
#include "Theme.h"

struct HUD_Data
{
    sf::Vector3f position;
    float cameraYaw;
    sf::Vector2f homeLocation;
    float relativeHomeAngle;
    int headlightState = 0;
    bool headlightEnabled = false;
    const sf::Texture* minimapTex = nullptr;
};


class HUD 

{
public:

    HUD(const sf::Vector2u& windowSize);

    void update(sf::RenderWindow& window, const HUD_Data& hudData);
    sf::Vector2f getJoystickAxis() const;
    void render(sf::RenderWindow& window, bool secondPass = false);

private:

    void init(const sf::Vector2u& windowSize);
    void drawHeadingTape(sf::RenderWindow& window, float yaw);
    void drawMiniMap(sf::RenderWindow& window);
    void drawHeadlightWidget(sf::RenderWindow& window);
    void drawCompass(sf::RenderWindow& window, float yawDeg);
    void updateHeadlightIcon();

    sf::RectangleShape m_tapeBg;
    float m_yawDeg;
    int m_headlightState = 0;
    bool m_headlightEnabled = false;
    const sf::Texture* m_minimapTex = nullptr;
    sf::Vector3f m_playerWorldPos{0.f, 0.f, 0.f};
    sf::Vector2f m_homeWorldPos{0.f, 0.f};

    // Compass layers
    std::unique_ptr<sf::Sprite> m_compassDisc;
    std::unique_ptr<sf::Sprite> m_compassTape;
    std::unique_ptr<sf::Sprite> m_compassBezel;
    float m_compassScale = 1.0f;

    // Mini Map Layers
    std::unique_ptr<sf::Sprite> m_minimapRing;

    // Headlamp Widget
    std::unique_ptr<sf::Sprite> m_headlightIcon;

    sf::CircleShape m_joystickBase;
    sf::CircleShape m_joystickKnob;
    
    bool            m_isDragging = false;
    bool            m_joystickVisible = false;
    sf::Vector2f    m_basePos;
    float           m_currentBaseRadius;
    float           m_currentKnobRadius;
};