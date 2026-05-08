#include "Theme.h"

#include <array>
#include <unordered_map>

namespace
{
    using Theme::ColorRole;

    constexpr size_t toIndex(ColorRole role)
    {
        return static_cast<size_t>(role);
    }

    const std::array<sf::Color, 18> palette = {
        sf::Color(247, 192, 55),       // TextBody
        sf::Color(247, 192, 55),       // MajorTitle
        sf::Color(238, 186, 80),       // MinorTitle
        sf::Color(119, 251, 0),        // Active
        sf::Color(100, 100, 100),      // Disabled
        sf::Color(238, 186, 80),       // Normal
        sf::Color(0, 0, 0, 128),       // Shadow
        sf::Color(0, 0, 0, 170),       // ShadowStrong
        sf::Color(242, 254, 255),      // Sky
        sf::Color(93,64,55,255),       // BestBrown
        sf::Color(63,34,25,255),       // DkBestBrown
        sf::Color(13,20,64,255),       // BackgroundBase
        sf::Color(35,183,217,255),     // Cerulean
        sf::Color(89, 82, 72, 0),      // Tarmac
        sf::Color(60, 60, 60, 128),    // HudBase
        sf::Color(200, 200, 200, 255), // HudOutline
        sf::Color(200, 200, 200, 255), // HudKnob
        sf::Color(64,64,64,255)        // HudText
    };

    const std::unordered_map<std::string_view, ColorRole> tokenMap = {
        { "text-body", ColorRole::TextBody },
        { "major-title", ColorRole::MajorTitle },
        { "minor-title", ColorRole::MinorTitle },
        { "active", ColorRole::Active },
        { "disabled", ColorRole::Disabled },
        { "normal", ColorRole::Normal },
        { "shadow", ColorRole::Shadow },
        { "shadow-strong", ColorRole::ShadowStrong },
        { "sky", ColorRole::Sky },
        { "best-brown", ColorRole::BestBrown },
        { "dk-best-brown", ColorRole::DkBestBrown },
        { "background-base", ColorRole::BackgroundBase },
        { "cerulean", ColorRole::Cerulean },
        { "tarmac", ColorRole::Tarmac },
        { "hud-base", ColorRole::HudBase },
        { "hud-outline", ColorRole::HudOutline },
        { "hud-knob", ColorRole::HudKnob },
        { "hud-text", ColorRole::HudText }
    };
}

const sf::Color& Theme::color(ColorRole role)
{
    return palette[toIndex(role)];
}

const sf::Color& Theme::color(std::string_view token)
{
    auto it = tokenMap.find(token);
    if (it == tokenMap.end())
    {
        return color(ColorRole::TextBody);
    }

    return color(it->second);
}
