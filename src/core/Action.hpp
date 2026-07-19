#pragma once

#include <string>
#include <string_view>
#include <SFML/System/Vector2.hpp>

class Action
{
    std::string     m_name       = "NONE";
    std::string     m_type       = "NONE";
    sf::Vector2i    m_position   = {0, 0};
    float           m_scrollDelta = 0.0f;
    float           m_value      = 0.0f;
    sf::Vector2f    m_delta      = {0.0f, 0.0f};

public:
    Action() = default;

    // Basic actions
    Action(std::string_view name, std::string_view type)
        : m_name(name), m_type(type) {}

    Action(const std::string& name, const std::string& type)
        : m_name(name), m_type(type) {}

    // Position-based actions (mouse, etc.)
    Action(std::string_view name, std::string_view type, const sf::Vector2i& pos)
        : m_name(name), m_type(type), m_position(pos) {}

    Action(const std::string& name, const std::string& type, const sf::Vector2i& pos)
        : m_name(name), m_type(type), m_position(pos) {}

    // Scroll + position
    Action(std::string_view name, std::string_view type, float scrollDelta, const sf::Vector2i& pos)
        : m_name(name), m_type(type), m_scrollDelta(scrollDelta), m_position(pos) {}

    Action(const std::string& name, const std::string& type, float scrollDelta, const sf::Vector2i& pos)
        : m_name(name), m_type(type), m_scrollDelta(scrollDelta), m_position(pos) {}

    // Analog value (triggers, axes, etc.)
    Action(std::string_view name, std::string_view type, float value)
        : m_name(name), m_type(type), m_value(value) {}

    // Delta / mouse look
    Action(std::string_view name, std::string_view type, const sf::Vector2f& delta)
        : m_name(name), m_type(type), m_delta(delta) {}

    // Getters
    const std::string& name()       const { return m_name; }
    const std::string& type()       const { return m_type; }
    sf::Vector2i       pos()        const { return m_position; }
    float              scrollDelta()const { return m_scrollDelta; }
    float              value()      const { return m_value; }
    sf::Vector2f       delta()      const { return m_delta; }
};