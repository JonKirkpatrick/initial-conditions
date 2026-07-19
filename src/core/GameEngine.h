#pragma once

#include "scenes/Scene.h"
#include "core/Assets.h"
#include "core/InputBindings.h"
           
#include <memory>

using SceneMap = std::map<std::string, std::shared_ptr<Scene>>;

class GameEngine
{

protected:
           
    sf::RenderWindow        m_window;
    std::string             m_currentScene;
    std::string             m_previousScene = "MENU";
    std::string             m_oneBeforeThat;
    SceneMap                m_sceneMap;
    ActionMap               m_actionMap;
    MouseActionMap          m_mouseActionMap;
    size_t                  m_simulationSpeed = 1;
    bool                    m_running = true;
    sf::Clock               m_deltaClock;
    sf::Clock               m_elapsedTime;
    size_t                  m_totalKeyPresses = 0;
    sf::Keyboard::Scancode  m_lastKeyPressed = sf::Keyboard::Scancode::Unknown;
           
    void init(const std::string & path);
    void update();

    void sUserInput();
    std::shared_ptr<Scene> currentScene();

    
           
           
public:
    
    GameEngine(const std::string & path);
    void changeScene(const std::string & sceneName, std::shared_ptr<Scene> scene, bool endCurrentScene = false, bool forceNewScene = false);
    void loadDefaultBindings();
    std::string getPreviousScene() const;
    std::string getOneBeforeThat() const;
    size_t getTotalKeyPresses() const;
    sf::Keyboard::Scancode getLastKeyPressed() const;

    void quit();
    void run();
           
    sf::RenderWindow & window();
    void registerAction(sf::Keyboard::Scancode inputKey, std::string_view actionName);
    void registerAction(sf::Mouse::Button inputButton, std::string_view actionName);
    bool isRunning();
    void flushInput();
    const MouseActionMap& getMouseActionMap() const;
    const ActionMap& getActionMap() const;
    const sf::Clock& getElapsedClock() const;
    bool                    m_mouseCaptured = false;
    void setMouseCaptured(bool captured) {
        m_mouseCaptured = captured;
        m_window.setMouseCursorVisible(!captured);
        m_window.setMouseCursorGrabbed(captured);
    }
    bool isMouseCaptured() const {
        return m_mouseCaptured;
    }
};