/**
 * @file HUD.h
 * @brief Heads-Up Display (HUD) data container and rendering manager.
 * 
 * Defines data structures and UI visual layer components including compass indicators,
 * heading tapes, minimap overlays, headlight state widgets, and on-screen touch/mouse joysticks.
 */

#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <array>
#include <memory>
#include "core/Assets.h"
#include "ui/Theme.h"

/**
 * @brief Telemetry and state snapshot passed from the scene/engine to feed HUD displays.
 */
struct HUD_Data
{
    sf::Vector3f position;           ///< 3D world position of player entity.
    sf::Vector2i mousePos;           ///< Current screen-space pixel coordinates of mouse/touch pointer.
    bool leftMousePressed;          ///< State flag for primary mouse button or active touch press.
    float cameraYaw;                 ///< Current camera orientation yaw angle (in degrees or radians).
    sf::Vector2f homeLocation;       ///< 2D world coordinates of home/origin marker location.
    float relativeHomeAngle;         ///< Bearing angle from current player position pointing toward home.
    int headlightState = 0;          ///< Current headlight mode integer index (e.g., off, low, high beam).
    bool headlightEnabled = false;   ///< Toggle flag indicating if player lights are actively turned on.
    const sf::Texture* minimapTex = nullptr; ///< Pointer to rendered dynamic minimap texture target, if available.
};

/**
 * @brief Heads-Up Display overlay manager handling tactical indicators, navigation elements, and controls.
 */
class HUD 
{
public:

    /**
     * @brief Constructs HUD layout configured for the current display resolution.
     * @param windowSize Target window width and height dimensions in pixels.
     */
    HUD(const sf::Vector2u& windowSize);

    /**
     * @brief Updates active HUD elements, touch joystick drag states, and dynamic textures.
     * @param window SFML render window target used for input coordinate conversions.
     * @param hudData Fresh frame snapshot containing entity telemetry and UI state flags.
     */
    void update(sf::RenderWindow& window, const HUD_Data& hudData);

    /**
     * @brief Retrieves normalized input displacement vector from virtual joystick.
     * @return 2D vector offset normalized between [-1.0, 1.0] per axis.
     */
    sf::Vector2f getJoystickAxis() const;

    /**
     * @brief Draws HUD visual overlays onto the target window frame.
     * @param window Target SFML window to draw into.
     * @param secondPass Flag enabling multi-pass rendering for compositing or post-effects.
     */
    void render(sf::RenderWindow& window, bool secondPass = false);

private:

    /**
     * @brief Configures sprite alignments, textures, and positioning relative to viewport size.
     * @param windowSize Resolution bounds used to anchor HUD elements.
     */
    void init(const sf::Vector2u& windowSize);

    /**
     * @brief Renders top-screen directional tape scrolling with player yaw.
     * @param window Target SFML render window.
     * @param yaw Active camera view orientation angle in degrees.
     */
    void drawHeadingTape(sf::RenderWindow& window, float yaw);

    /**
     * @brief Renders circular map view centered around player coordinates.
     * @param window Target SFML render window.
     */
    void drawMiniMap(sf::RenderWindow& window);

    /**
     * @brief Renders headlamp activation icon and current lighting mode state.
     * @param window Target SFML render window.
     */
    void drawHeadlightWidget(sf::RenderWindow& window);

    /**
     * @brief Renders rotating compass rose and bezel housing.
     * @param window Target SFML render window.
     * @param yawDeg Camera view direction in degrees.
     */
    void drawCompass(sf::RenderWindow& window, float yawDeg);

    /** @brief Refreshes headlight sprite sub-rect region or color state based on `m_headlightState`. */
    void updateHeadlightIcon();

    sf::RectangleShape m_tapeBg;            ///< Background backdrop ribbon for heading tape.
    float m_yawDeg = 0.0f;                   ///< Cached yaw orientation angle in degrees.
    int m_headlightState = 0;                ///< Cached light mode index.
    bool m_headlightEnabled = false;         ///< Cached headlight state flag.
    const sf::Texture* m_minimapTex = nullptr; ///< Direct reference to minimap snapshot buffer.
    sf::Vector3f m_playerWorldPos{0.f, 0.f, 0.f}; ///< Cached 3D world position of local player.
    sf::Vector2f m_homeWorldPos{0.f, 0.f};   ///< Cached 2D world position of home objective marker.

    // Compass layers
    std::unique_ptr<sf::Sprite> m_compassDisc;  ///< Rotating internal disc carrying directional markings.
    std::unique_ptr<sf::Sprite> m_compassTape;  ///< Dynamic sliding tape texture overlay.
    std::unique_ptr<sf::Sprite> m_compassBezel; ///< Static outer framing ring for compass housing.
    float m_compassScale = 1.0f;                 ///< Scaling multiplier applied to compass visual components.

    // Mini Map Layers
    std::unique_ptr<sf::Sprite> m_minimapRing;  ///< Outer border mask/bezel framing the circular minimap.

    // Headlamp Widget
    std::unique_ptr<sf::Sprite> m_headlightIcon;///< Visual icon displaying active light mode status.

    // Virtual Joystick Controls
    sf::CircleShape m_joystickBase;          ///< Base circular perimeter shape for virtual analog stick.
    sf::CircleShape m_joystickKnob;          ///< Movable thumb knob indicator for virtual analog stick.
    
    bool            m_isDragging = false;    ///< `true` if cursor/finger is actively manipulating joystick knob.
    bool            m_joystickVisible = false;///< `true` if virtual joystick overlay is displayed on-screen.
    sf::Vector2f    m_basePos;               ///< Screen-space center coordinates of joystick base.
    float           m_currentBaseRadius;     ///< Pixel radius of joystick outer constraint ring.
    float           m_currentKnobRadius;     ///< Pixel radius of central joystick draggable knob.
};