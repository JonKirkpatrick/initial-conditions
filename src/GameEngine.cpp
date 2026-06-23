#include <GL/glew.h>
#include "GameEngine.h"
#include "Assets.h"
#include "Scene_Menu.h"
#include "InputBindings.h"

#include "imgui.h"
#include "imgui-SFML.h"

GameEngine::GameEngine(const std::string & path)
{
    init(path);
}

void GameEngine::init(const std::string & path)
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    settings.depthBits = 24;
    settings.stencilBits = 8;

    loadDefaultBindings();
    std::cout << "Loading sprite sheets..." << std::endl;
    auto modes = sf::VideoMode::getFullscreenModes();
    if (modes.empty())
    {
        std::cerr << "No video modes available!" << std::endl;
        std::exit(1);
    }
    m_window.create(modes[1], "Initial Conditions", sf::Style::None, sf::State::Fullscreen, settings);
    m_window.setVerticalSyncEnabled(true);
    m_window.setKeyRepeatEnabled(false);
    m_window.setPosition(sf::Vector2i(0, 0));
    m_window.setSize(sf::Vector2u(modes[1].size.x, modes[1].size.y));
    m_window.setView(sf::View(sf::FloatRect(
        {0.f, 0.f},
        {float(m_window.getSize().x), float(m_window.getSize().y)}
    )));
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "GLEW init failed: " << glewGetErrorString(err) << std::endl;
    }
    std::cout << glGetString(GL_VERSION) << std::endl;

    Assets::Instance().loadFromFile(path);

    if (!ImGui::SFML::Init(m_window)) {}

    changeScene("MENU", std::make_shared<Scene_Menu>(*this));
}

void GameEngine::loadDefaultBindings()
{
    m_actionMap.clear();

    registerAction(sf::Keyboard::Scancode::P,       InputAction::Pause);
    registerAction(sf::Keyboard::Scancode::Escape,  InputAction::Quit);
    registerAction(sf::Keyboard::Scancode::F,       InputAction::ToggleCursor);     // This is for toggling mouse look on and off.
    registerAction(sf::Keyboard::Scancode::F12,     InputAction::ShowGui);          // This is to toggle ImGUI on and off.
    registerAction(sf::Keyboard::Scancode::A,       InputAction::MoveLeft);
    registerAction(sf::Keyboard::Scancode::D,       InputAction::MoveRight);
    registerAction(sf::Keyboard::Scancode::S,       InputAction::MoveBackward);
    registerAction(sf::Keyboard::Scancode::W,       InputAction::MoveForward);
    registerAction(sf::Keyboard::Scancode::C,       InputAction::Crouch);
    registerAction(sf::Keyboard::Scancode::H,       InputAction::ToggleHeadlight);  // Custom action for toggling headlight state
    registerAction(sf::Keyboard::Scancode::LControl,InputAction::Strafe);           // Hold for strafing (A/D for lateral movement instead of rotation)
    registerAction(sf::Keyboard::Scancode::Space,   InputAction::Jump);
    registerAction(sf::Keyboard::Scancode::LShift,  InputAction::Sprint);
    registerAction(sf::Keyboard::Scancode::E,       InputAction::Interact);
    registerAction(sf::Mouse::Button::Left,         InputAction::LeftClick);
    registerAction(sf::Mouse::Button::Middle,       InputAction::MiddleClick);
    registerAction(sf::Mouse::Button::Right,        InputAction::RightClick);
}

size_t GameEngine::getTotalKeyPresses() const
{
    return m_totalKeyPresses;
}

sf::Keyboard::Scancode GameEngine::getLastKeyPressed() const
{
    return m_lastKeyPressed;
}

std::shared_ptr<Scene> GameEngine::currentScene()
{
    return m_sceneMap[m_currentScene];
}

std::string GameEngine::getPreviousScene() const
{
    return m_previousScene;
}

std::string GameEngine::getOneBeforeThat() const
{
    return m_oneBeforeThat;
}

bool GameEngine::isRunning()
{ 
    return m_running && m_window.isOpen();
}

void GameEngine::registerAction(sf::Keyboard::Scancode inputKey, std::string_view actionName)
{
    m_actionMap[inputKey] = std::string(actionName);
}

void GameEngine::registerAction(sf::Mouse::Button inputButton, std::string_view actionName)
{
    m_mouseActionMap[inputButton] = std::string(actionName);
}

const MouseActionMap& GameEngine::getMouseActionMap() const
{
    return m_mouseActionMap;
}

const ActionMap& GameEngine::getActionMap() const
{
    return m_actionMap;
}

sf::RenderWindow & GameEngine::window()
{
    return m_window;
}

void GameEngine::run()
{
    while (isRunning())
    {
        update();
    }
}
           
void GameEngine::sUserInput()
{
    while (auto event = m_window.pollEvent())
    {
        // pass the event to imgui to be parsed
        ImGui::SFML::ProcessEvent(m_window, *event);
           
        // this event triggers when the window is closed
        if (event->is<sf::Event::Closed>())
        {
            m_window.close();
            std::exit(0);
        }
           
        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            m_totalKeyPresses++;
            m_lastKeyPressed = keyPressed->scancode;

            const auto& actionMap = getActionMap();
            auto it = actionMap.find(keyPressed->scancode);
            if (it != actionMap.end())
            {
                currentScene()->doAction(Action(it->second, "START"));
            }
        }

        if (const auto* keyPressed = event->getIf<sf::Event::KeyReleased>())
        {
            const auto& actionMap = getActionMap();
            auto it = actionMap.find(keyPressed->scancode);
            if (it != actionMap.end())
            {
                currentScene()->doAction(Action(it->second, "END"));
            }
        }

        // if the mouse is over an ImGui window, don't process this mouse event
        if (ImGui::GetIO().WantCaptureMouse) { continue; }

        // process mouse events
        if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
        {
            const auto& mouseActionMap = getMouseActionMap();
            auto it = mouseActionMap.find(mb->button);
            if (it != mouseActionMap.end())
            {
                currentScene()->doAction(Action(it->second, "START", mb->position));
            }
        }
           
        if (const auto* mb = event->getIf<sf::Event::MouseButtonReleased>())
        {
            const auto& mouseActionMap = getMouseActionMap();
            auto it = mouseActionMap.find(mb->button);
            if (it != mouseActionMap.end())
            {
                currentScene()->doAction(Action(it->second, "END", mb->position));
            }
        }

        if (const auto* mm = event->getIf<sf::Event::MouseMoved>())
        {
            if (m_mouseCaptured) {
                sf::Vector2u size = m_window.getSize();
                sf::Vector2i center(size.x / 2, size.y / 2);
                sf::Vector2f delta(mm->position.x - center.x, mm->position.y - center.y);
                if (delta.x != 0 || delta.y != 0) {
                    currentScene()->doAction(Action(InputAction::MouseMove, "LOOK", delta));
                    sf::Mouse::setPosition(center, m_window);
                }
            } else {
                currentScene()->doAction(Action(InputAction::MouseMove, "START",
                                                sf::Vector2i(mm->position.x, mm->position.y)));
            }
        }

        if (const auto* mm = event->getIf<sf::Event::MouseWheelScrolled>())
        {
            const sf::Vector2i mpos(mm->position.x, mm->position.y);
            currentScene()->doAction(Action(InputAction::MouseWheelScroll, "START", mm->delta, mpos));
        }
    }
    if (currentScene() && currentScene()->getHUD())
    {
        sf::Vector2f joystick = currentScene()->getHUD()->getJoystickAxis();
        currentScene()->doAction(Action(InputAction::CameraYawRight, "ANALOG", joystick.x));
        currentScene()->doAction(Action(InputAction::MoveUp, "ANALOG", -joystick.y));
    }
}

void GameEngine::flushInput()
{
    for (auto& [scancode, actionName] : getActionMap())
    {
        currentScene()->doAction(Action(actionName, "END"));
    }
}


void GameEngine::changeScene(const std::string& sceneName, 
    std::shared_ptr<Scene> scene, 
    bool endCurrentScene, 
    bool forceNewScene)
{
    if (scene) // passed in a scene pointer
    {
        m_oneBeforeThat = m_previousScene;
        m_previousScene = m_currentScene;
        if (forceNewScene && m_sceneMap.find(sceneName) != m_sceneMap.end())
        {
            if (m_currentScene == sceneName)
            {
                m_sceneMap[sceneName]->onExit();
            }
            
            m_sceneMap.erase(sceneName); 
        }        
        if (m_sceneMap.find(sceneName) == m_sceneMap.end()) // scene not in scenemap
        {
            m_sceneMap[sceneName] = scene;
        }
    }
    else // didn't pass in scene pointer
    {
        if (m_sceneMap.find(sceneName) == m_sceneMap.end())
        {
            std::cerr << "Warning: Scene does not exist: " << sceneName << std::endl;
            return;
        }
    }

    if (!m_currentScene.empty()) // if we have a current scene, call onExit on it
    {
        auto it = m_sceneMap.find(m_currentScene);
        if (it != m_sceneMap.end())
        {
            it->second->onExit();
        }
    }

    if (endCurrentScene && !m_currentScene.empty())  // if requested to do so, remove current scene from scenemap
    {
        m_sceneMap.erase(m_currentScene);
    }

    m_currentScene = sceneName;

    auto newIt = m_sceneMap.find(m_currentScene);   // call onEnter on new current scene
    if (newIt != m_sceneMap.end())
    {
        flushInput();
        newIt->second->onEnter();
    }
}

void GameEngine::update()
{
    ImGui::SFML::Update(m_window, m_deltaClock.restart());

    if (!isRunning()) { return; }
    
    if (m_sceneMap.empty()) { return; }
    
    sUserInput();
    currentScene()->simulate(m_simulationSpeed);
    currentScene()->sRender();
    ImGui::SFML::Render(m_window);
    m_window.display();
}

const sf::Clock& GameEngine::getElapsedClock() const
{
    return m_elapsedTime;
}
           
void GameEngine::quit()
{
    m_running = false;
}