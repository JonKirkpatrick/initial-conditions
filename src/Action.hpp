#pragma once

#include "Vec2.hpp"

#include <string>
#include <string_view>
#include <sstream>

class Action
{
    std::string m_name = "NONE";
    std::string m_type = "NONE";
    Vec2i m_position;
    float m_scrollDelta;
    float m_value = 0.f;
    Vec2f m_delta;
           
public:

    Action() = default;

    Action(std::string_view name, std::string_view type)
        : m_name(name)
        , m_type(type) { }

    Action(std::string_view name, std::string_view type, const Vec2i& pos)
        : m_name(name)
        , m_type(type)
        , m_position(pos) { }

    Action(std::string_view name, std::string_view type, const float& scrollDelta, const Vec2i& pos)
        : m_name(name)
        , m_type(type)
        , m_scrollDelta(scrollDelta)
        , m_position(pos) { }

    Action(const std::string& name, const std::string& type)
        : m_name(name)
        , m_type(type) { }
           
    Action(const std::string& name, const std::string& type, const Vec2i& pos)
        : m_name(name)
        , m_type(type)
        , m_position(pos) { }
           
    Action(const std::string& name, const std::string& type, const float& scrollDelta, const Vec2i& pos)
        : m_name(name)
        , m_type(type)
        , m_scrollDelta(scrollDelta)
        , m_position(pos) { }

    Action(std::string_view name, std::string_view type, const float& value)
        : m_name(name)
        , m_type(type)
        , m_value(value) { }

    Action(std::string_view name, std::string_view type, const Vec2f& delta)
        : m_name(name)
        , m_type(type)
        , m_delta(delta) { }
        
    const std::string& name() const
    {
        return m_name;
    }

    const std::string& type() const
    {
        return m_type;
    }

    Vec2i pos() const
    {
        return m_position;
    }

    float scrollDelta() const
    {
        return m_scrollDelta;
    }

    float value() const
    {
        return m_value;
    }

    Vec2f delta() const
    {
        return m_delta;
    }
};