#pragma once

#include "Action.hpp"
#include "InputBindings.h"
#include "EntityManager.hpp"
#include "HUD.h"

#include <memory>

class GameEngine;
           
class Scene
{

protected: 
    
    GameEngine&     m_game;
    EntityManager   m_entityManager;
    ActionMap       m_actionMap;
    bool            m_paused = false;
    bool            m_hasEnded = false;
    size_t          m_currentFrame = 0;

    virtual void onEnd() = 0;
    void setPaused(bool paused);
           
public:
           
    Scene(GameEngine& gameEngine);
    virtual ~Scene() = default;

    virtual void update() = 0;
    virtual void sDoAction(const Action & action) = 0;
    virtual void sRender() = 0;
    virtual void onEnter() = 0;
    virtual void onExit() = 0;
    virtual void doAction(const Action& action);
    virtual HUD* getHUD() const { return nullptr; }
    void simulate(const size_t frames);
    void registerAction(sf::Keyboard::Scancode inputKey, std::string_view actionName);

    size_t width() const;
    size_t height() const;
    size_t currentFrame() const;
           
    bool hasEnded() const;
    const ActionMap& getActionMap() const;
};