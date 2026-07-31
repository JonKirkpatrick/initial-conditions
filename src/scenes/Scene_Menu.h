/**
 * @file Scene_Menu.h
 * @brief Main menu scene implementation for application navigation and scene selection.
 * 
 * Provides interactive main menu functionality featuring keyboard/mouse selection,
 * hover time tracking, background texture rendering, and scene transition routing.
 */

#pragma once

#include "scenes/Scene.h"
#include "ecs/EntityManager.hpp"

#include <memory>
#include <deque>
#include <vector>
#include <optional>
#include <map>
#include <SFML/Graphics.hpp>
           
/**
 * @brief Main menu scene managing menu items, navigation states, and scene switching.
 */
class Scene_Menu : public Scene
{

protected:
    std::string                 m_title;            ///< Title text displayed at the top of the menu.
    std::vector<std::string>    m_menuStrings;      ///< List of selectable menu item labels.
    sf::Texture                 m_menuBackground;   ///< Background texture displayed behind menu items.
    size_t                      m_selectedMenuIndex = 0;   ///< Index of the currently highlighted/selected menu item.
    std::optional<size_t>       m_hoveredMenuIndex;        ///< Index of the menu item currently under the mouse cursor, if any.
    std::optional<float>        m_hoverStartTime;   ///< Timestamp (in seconds) when mouse hover began on the current menu item.
    std::map<std::string, std::shared_ptr<Scene>> m_sceneMap; ///< Map binding menu string identifiers to target `Scene` instances.
    
    /** @brief Initializes menu choices, bindings, assets, and UI element positions. */
    void init();

    /** @brief Updates frame logic including cursor positioning and hover animations. */
    void update() override;

    /** @brief Lifecycle callback invoked when the scene terminates. */
    void onEnd() override;

    /** @brief Lifecycle callback invoked when entering the menu scene. */
    void onEnter() override;

    /** @brief Lifecycle callback invoked when exiting the menu scene. */
    void onExit() override;

    /**
     * @brief Processes user input actions (keyboard navigation, clicks, triggers).
     * @param action Input action event dispatched by the game engine.
     */
    void sDoAction(const Action& action) override;

    /**
     * @brief Performs point-in-bounds hit testing for mouse selection against menu items.
     * @param mousePosition Current 2D window coordinate of the mouse cursor in pixels.
     * @return Index of hit menu item if mouse overlaps an entry, otherwise `std::nullopt`.
     */
    std::optional<size_t> hitTestMenuIndex(const sf::Vector2i& mousePosition) const;

    /** @brief Triggers execution or scene transition for the currently selected menu item. */
    void activateSelectedMenuItem();

public:

    /**
     * @brief Constructs `Scene_Menu` bound to the active game engine context.
     * @param gameEngine Reference to the primary `GameEngine` host.
     */
    Scene_Menu(GameEngine& gameEngine);

    /** @brief Renders background artwork, title text, and menu item options. */
    void sRender() override;

};