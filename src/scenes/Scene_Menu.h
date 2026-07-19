#pragma once

#include "scenes/Scene.h"
#include "ecs/EntityManager.hpp"

#include <memory>
#include <deque>
#include <vector>
#include <optional>
#include <SFML/Graphics.hpp>
           
class Scene_Menu : public Scene
{

protected:
    std::string                 m_title;
    std::vector<std::string>    m_menuStrings;
    sf::Texture                 m_menuBackground;
    size_t                      m_selectedMenuIndex = 0;
    std::optional<size_t>       m_hoveredMenuIndex;
    std::optional<float>        m_hoverStartTime; // Time when mouse started hovering over a menu item
    std::map<std::string, std::shared_ptr<Scene>> m_sceneMap;
    
    void init();
    void update();
    void onEnd();
    void onEnter();
    void onExit();
    void sDoAction(const Action& action);
    std::optional<size_t> hitTestMenuIndex(const sf::Vector2i& mousePosition) const;
    void activateSelectedMenuItem();

public:

    Scene_Menu(GameEngine& gameEngine);
    void sRender();

};