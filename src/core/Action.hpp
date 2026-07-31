#pragma once

#include <string>
#include <string_view>
#include <SFML/System/Vector2.hpp>

/**
 * @brief Represents an input action triggered by a user interface or input device.
 * 
 * The Action class decouples raw hardware inputs (keyboard, mouse, gamepads) 
 * from game logic by mapping input states to named semantic events (e.g., "JUMP", "MOVE").
 * It supports digital actions, positional events (mouse clicks), analog triggers, 
 * scroll wheel movements, and directional deltas (mouse look).
 */
class Action
{
    std::string     m_name       = "NONE";   ///< The identifier for the action (e.g., "JUMP", "PAUSE").
    std::string     m_type       = "NONE";   ///< The state or phase of the action (e.g., "START", "END").
    sf::Vector2i    m_position   = {0, 0};   ///< Screen or window position associated with the action (e.g., mouse coordinates).
    float           m_scrollDelta = 0.0f;    ///< Vertical or horizontal scroll offset.
    float           m_value      = 0.0f;     ///< Analog input magnitude (e.g., trigger pressure or axis value, usually [0.0, 1.0]).
    sf::Vector2f    m_delta      = {0.0f, 0.0f}; ///< Directional delta movement (e.g., mouse delta for camera control).

public:
    /**
     * @brief Constructs a default uninitialized Action ("NONE", "NONE").
     */
    Action() = default;

    /// @name Basic Digital Actions
    /// @{

    /**
     * @brief Constructs a basic digital action using string views.
     * @param name The semantic identifier for the action.
     * @param type The state/type of the action.
     */
    Action(std::string_view name, std::string_view type)
        : m_name(name), m_type(type) {}

    /**
     * @brief Constructs a basic digital action using string references.
     * @param name The semantic identifier for the action.
     * @param type The state/type of the action.
     */
    Action(const std::string& name, const std::string& type)
        : m_name(name), m_type(type) {}

    /// @}

    /// @name Positional Actions
    /// @{

    /**
     * @brief Constructs a position-based action using string views.
     * @param name The semantic identifier for the action.
     * @param type The state/type of the action.
     * @param pos Pixel coordinates associated with the action.
     */
    Action(std::string_view name, std::string_view type, const sf::Vector2i& pos)
        : m_name(name), m_type(type), m_position(pos) {}

    /**
     * @brief Constructs a position-based action using string references.
     * @param name The semantic identifier for the action.
     * @param type The state/type of the action.
     * @param pos Pixel coordinates associated with the action.
     */
    Action(const std::string& name, const std::string& type, const sf::Vector2i& pos)
        : m_name(name), m_type(type), m_position(pos) {}

    /// @}

    /// @name Scroll Actions
    /// @{

    /**
     * @brief Constructs a mouse wheel scroll action with position using string views.
     * @param name The semantic identifier for the action.
     * @param type The state/type of the action.
     * @param scrollDelta The scroll wheel movement delta.
     * @param pos The position of the cursor during the scroll event.
     */
    Action(std::string_view name, std::string_view type, float scrollDelta, const sf::Vector2i& pos)
        : m_name(name), m_type(type), m_scrollDelta(scrollDelta), m_position(pos) {}

    /**
     * @brief Constructs a mouse wheel scroll action with position using string references.
     * @param name The semantic identifier for the action.
     * @param type The state/type of the action.
     * @param scrollDelta The scroll wheel movement delta.
     * @param pos The position of the cursor during the scroll event.
     */
    Action(const std::string& name, const std::string& type, float scrollDelta, const sf::Vector2i& pos)
        : m_name(name), m_type(type), m_scrollDelta(scrollDelta), m_position(pos) {}

    /// @}

    /// @name Analog & Delta Actions
    /// @{

    /**
     * @brief Constructs an analog input action (e.g., joystick axes or triggers).
     * @param name The semantic identifier for the action.
     * @param type The state/type of the action.
     * @param value The scalar magnitude of the input.
     */
    Action(std::string_view name, std::string_view type, float value)
        : m_name(name), m_type(type), m_value(value) {}

    /**
     * @brief Constructs a relative movement action (e.g., mouse look or panning).
     * @param name The semantic identifier for the action.
     * @param type The state/type of the action.
     * @param delta The relative 2D offset vector.
     */
    Action(std::string_view name, std::string_view type, const sf::Vector2f& delta)
        : m_name(name), m_type(type), m_delta(delta) {}

    /// @}

    /// @name Accessors
    /// @{

    /**
     * @brief Gets the name identifier of the action.
     * @return Const reference to the action name string.
     */
    const std::string& name() const { return m_name; }

    /**
     * @brief Gets the type/phase of the action.
     * @return Const reference to the action type string.
     */
    const std::string& type() const { return m_type; }

    /**
     * @brief Gets the screen position associated with the action.
     * @return The 2D integer position vector.
     */
    sf::Vector2i pos() const { return m_position; }

    /**
     * @brief Gets the scroll delta value.
     * @return The scalar scroll movement delta.
     */
    float scrollDelta() const { return m_scrollDelta; }

    /**
     * @brief Gets the analog scalar input value.
     * @return The floating-point magnitude value.
     */
    float value() const { return m_value; }

    /**
     * @brief Gets the relative movement delta vector.
     * @return The 2D float delta vector.
     */
    sf::Vector2f delta() const { return m_delta; }

    /// @}
};