#pragma once

#include "scenes/Scene.h"
#include "core/Assets.h"
#include "core/InputBindings.h"
           
#include <map>
#include <memory>
#include <string>
#include <string_view>

/**
 * @brief Map alias connecting unique string identifiers to shared Scene instances.
 */
using SceneMap = std::map<std::string, std::shared_ptr<Scene>>;

/**
 * @brief Core engine class managing the main loop, window rendering, input dispatch, and scene switching.
 * 
 * The GameEngine serves as the central orchestrator for the application. It owns the main 
 * `sf::RenderWindow`, drives the primary frame loop, routes keyboard and mouse events 
 * to semantic Action bindings, and manages active and historical game scenes.
 */
class GameEngine
{

protected:
    sf::RenderWindow        m_window;           ///< The main SFML rendering window.
    std::string             m_currentScene;     ///< Key identifier for the currently active scene.
    std::string             m_previousScene = "MENU"; ///< Key identifier for the immediately preceding scene.
    std::string             m_oneBeforeThat;    ///< Key identifier for the scene two transitions prior.
    SceneMap                m_sceneMap;         ///< Registry storing all loaded scene instances by name.
    ActionMap               m_actionMap;        ///< Mapping of keyboard scancodes to action names.
    MouseActionMap          m_mouseActionMap;   ///< Mapping of mouse buttons to action names.
    size_t                  m_simulationSpeed = 1; ///< Multiplier tick count for simulation updates per frame.
    bool                    m_running = true;   ///< Flag controlling the lifetime of the main loop.
    sf::Clock               m_deltaClock;       ///< Clock measuring frame delta time.
    sf::Clock               m_elapsedTime;      ///< Total runtime clock since initialization.
    size_t                  m_totalKeyPresses = 0; ///< Lifetime counter of physical key presses.
    sf::Keyboard::Scancode  m_lastKeyPressed = sf::Keyboard::Scancode::Unknown; ///< Most recently pressed key scancode.
           
    /**
     * @brief Initializes engine subsystems and loads configuration options from disk.
     * @param path File path to the engine configuration file.
     */
    void init(const std::string & path);

    /**
     * @brief Per-frame system update step driving input processing, logic updates, and rendering.
     */
    void update();

    /**
     * @brief Polls window events, processes input, and routes actions to the current scene.
     */
    void sUserInput();

    /**
     * @brief Retrieves a pointer to the currently active Scene.
     * @return Shared pointer to active `Scene`, or `nullptr` if no active scene exists.
     */
    std::shared_ptr<Scene> currentScene();

public:
    /**
     * @brief Constructs the GameEngine and initializes subsystems using the specified config file.
     * @param path File path to initialization asset/config file.
     */
    GameEngine(const std::string & path);

    /// @name Scene Management
    /// @{

    /**
     * @brief Changes the active scene or registers a new scene instance.
     * @param sceneName Identifier name of the scene to switch to.
     * @param scene Shared pointer to the new Scene object.
     * @param endCurrentScene If `true`, informs the active scene to terminate before switching.
     * @param forceNewScene If `true`, replaces any existing scene with the same name in the scene map.
     */
    void changeScene(const std::string & sceneName, std::shared_ptr<Scene> scene, bool endCurrentScene = false, bool forceNewScene = false);

    /**
     * @brief Gets the name of the previous scene.
     * @return String identifier of the previous scene.
     */
    std::string getPreviousScene() const;

    /**
     * @brief Gets the name of the scene active prior to the previous scene.
     * @return String identifier of the target historical scene.
     */
    std::string getOneBeforeThat() const;

    /// @}

    /// @name Lifecycle & Execution Control
    /// @{

    /**
     * @brief Starts the main game loop. Blocks until the application is closed or quit() is called.
     */
    void run();

    /**
     * @brief Signals the engine to shut down and break out of the main loop.
     */
    void quit();

    /**
     * @brief Checks if the engine loop is currently running.
     * @return `true` if running, `false` if shut down pending.
     */
    bool isRunning();

    /// @}

    /// @name Input Handling & Registration
    /// @{

    /**
     * @brief Binds a keyboard key scancode to an abstract action name.
     * @param inputKey The physical keyboard key scancode.
     * @param actionName The semantic action identifier string.
     */
    void registerAction(sf::Keyboard::Scancode inputKey, std::string_view actionName);

    /**
     * @brief Binds a mouse button to an abstract action name.
     * @param inputButton The SFML mouse button enum.
     * @param actionName The semantic action identifier string.
     */
    void registerAction(sf::Mouse::Button inputButton, std::string_view actionName);

    /**
     * @brief Clears pending input action queues and resets press state accumulators.
     */
    void flushInput();

    /**
     * @brief Loads the default set of key and mouse bindings.
     */
    void loadDefaultBindings();

    /**
     * @brief Retrieves the active mouse action map.
     * @return Const reference to the `MouseActionMap`.
     */
    const MouseActionMap& getMouseActionMap() const;

    /**
     * @brief Retrieves the active keyboard action map.
     * @return Const reference to the `ActionMap`.
     */
    const ActionMap& getActionMap() const;

    /**
     * @brief Retrieves total physical key press count since program start.
     * @return Lifetime key press count.
     */
    size_t getTotalKeyPresses() const;

    /**
     * @brief Gets the scancode of the most recently pressed key.
     * @return `sf::Keyboard::Scancode` of last pressed key.
     */
    sf::Keyboard::Scancode getLastKeyPressed() const;

    /// @}

    /// @name Window & Hardware State Accessors
    /// @{

    /**
     * @brief Accesses the main SFML window reference.
     * @return Non-const reference to the main `sf::RenderWindow`.
     */
    sf::RenderWindow & window();

    /**
     * @brief Gets the total time elapsed since engine initialization.
     * @return Const reference to internal clock measuring lifetime runtime.
     */
    const sf::Clock& getElapsedClock() const;

    bool m_mouseCaptured = false; ///< Tracks whether the cursor is constrained to the window bounds.

    /**
     * @brief Enables or disables mouse capture, constraining cursor to window and hiding cursor icon.
     * @param captured `true` to lock and hide cursor, `false` to release and show it.
     */
    void setMouseCaptured(bool captured) {
        m_mouseCaptured = captured;
        m_window.setMouseCursorVisible(!captured);
        m_window.setMouseCursorGrabbed(captured);
    }

    /**
     * @brief Checks if the mouse cursor is currently captured/grabbed.
     * @return `true` if mouse is locked to window, `false` otherwise.
     */
    bool isMouseCaptured() const {
        return m_mouseCaptured;
    }

    /// @}
};