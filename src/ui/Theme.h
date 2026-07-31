#pragma once

#include <SFML/Graphics/Color.hpp>

#include <string_view>

namespace Theme
{
    enum class ColorRole
    {
        Text_Body,
        MajorTitle,
        MinorTitle,
        Active,
        Disabled,
        Normal,
        Shadow,
        ShadowStrong,
        Sky,
        BestBrown,
        DkBestBrown,
        BackgroundBase,
        Cerulean,
        Tarmac,
        HudBase,
        HudOutline,
        HudKnob,
        HudText
    };

    const sf::Color& color(ColorRole role);
    const sf::Color& color(std::string_view token);
}
