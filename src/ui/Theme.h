/**
 * @file Theme.h
 * @brief Global UI color theme definitions and color role resolution interfaces.
 * 
 * Provides centralized color management for user interfaces, HUD elements, 
 * typography states, and atmospheric accents using strongly-typed color roles or string tokens.
 */

#pragma once

#include <SFML/Graphics/Color.hpp>
#include <string_view>

/**
 * @namespace Theme
 * @brief Central namespace housing global visual style definitions and palette resolvers.
 */
namespace Theme
{
    /**
     * @enum ColorRole
     * @brief Semantic identifiers mapping abstract UI color roles to specific palette values.
     */
    enum class ColorRole
    {
        MajorTitle,     ///< Primary prominent header and title text color.
        MinorTitle,     ///< Secondary subtitle and section heading text color.
        Active,         ///< Highlight color for interactive, hovered, or active UI elements.
        Disabled,       ///< Low-contrast color for inactive or non-interactive UI elements.
        Normal,         ///< Default standard text and iconography color.
        Shadow,         ///< Soft shadow color for text drop-shadows and UI depth elevation.
        ShadowStrong,   ///< High-opacity deep shadow color for strong outline effects.
        Sky,            ///< Atmospheric celestial sky tint accent color.
        BestBrown,      ///< Primary rich brown earth tone accent color.
        DkBestBrown,    ///< Dark brown accent variant for shading or borders.
        BackgroundBase, ///< Primary background canvas fill color for panels and backdrops.
        Cerulean,       ///< Cerulean blue accent color for highlights or water UI indicators.
        Tarmac,         ///< Dark asphalt gray shade for grounding containers and bars.
        HudBase,        ///< Semi-transparent background fill for HUD overlays and widgets.
        HudOutline,     ///< Border and vector stroke color for HUD framing graphics.
        HudKnob,        ///< Active knob fill color for virtual joystick and slider controls.
        HudText         ///< High-legibility text and indicator readout color on HUD elements.
    };

    /**
     * @brief Retrieves the assigned SFML color for a specific semantic theme role.
     * @param role The `ColorRole` enum value specifying the target element style.
     * @return Constant reference to the mapped `sf::Color`.
     */
    const sf::Color& color(ColorRole role);

    /**
     * @brief Retrieves the assigned SFML color associated with a string token identifier.
     * @param token String view matching a named theme color key (e.g., `"MajorTitle"`, `"HudBase"`).
     * @return Constant reference to the resolved `sf::Color`.
     */
    const sf::Color& color(std::string_view token);
}