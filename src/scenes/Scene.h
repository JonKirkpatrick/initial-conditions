/**
 * @file Scene.h
 * @brief Abstract base class defining the lifecycle, state, and input dispatch interface for all game scenes.
 * 
 * Provides foundational infrastructure for scene management, including frame-based simulation, 
 * entity component lifecycle tracking, input action mapping (`ActionMap`), pausing, and window dimension queries.
 */

#pragma once

#include "core/Action.hpp"
#include "core/InputBindings.h"
#include "ecs/EntityManager.hpp"
#include "ui/HUD.h"

#include <memory>

class GameEngine;
           
/**
 * @brief Abstract parent class representing a self-contained game state or screen (e.g., menus, gameplay levels).
 */
class Scene
{

protected: 
    
    GameEngine&     m_game;           ///< Reference to the host `GameEngine` driving execution.
    EntityManager   m_entityManager;  ///< Primary entity manager housing scene entities and components.
    ActionMap       m_actionMap;      ///< Key/input scancode mapping table binding inputs to named actions.
    bool            m_paused = false; ///< Pause state flag; halts frame logic updates when `true`.
    bool            m_hasEnded = false; ///< Termination flag indicating the scene should be popped or transitioned.
    size_t          m_currentFrame = 0; ///< Cumulative count of simulated frames elapsed in this scene.

    /** @brief Pure virtual method invoked when the scene is requested to end/terminate. */
    virtual void onEnd() = 0;

    /**
     * @brief Toggles or sets the scene pause state.
     * @param paused `true` to suspend scene frame updates, `false` to resume.
     */
    void setPaused(bool paused);
           
public:
           
    /**
     * @brief Constructs `Scene` attached to the owning `GameEngine`.
     * @param gameEngine Reference to the parent `GameEngine`.
     */
    Scene(GameEngine& gameEngine);

    /** @brief Virtual destructor ensuring proper cleanup of derived scene resources. */
    virtual ~Scene() = default;

    /** @brief Pure virtual frame update function evaluating scene logic, physics, and state updates. */
    virtual void update() = 0;

    /**
     * @brief Pure virtual handler for processing dispatched named actions.
     * @param action Active input action event payload to execute.
     */
    virtual void sDoAction(const Action & action) = 0;

    /** @brief Pure virtual method responsible for issuing draw calls for the scene. */
    virtual void sRender() = 0;

    /** @brief Pure virtual callback invoked when the scene gains active focus. */
    virtual void onEnter() = 0;

    /** @brief Pure virtual callback invoked when the scene loses active focus. */
    virtual void onExit() = 0;

    /**
     * @brief Dispatches an input action to `sDoAction` unless blocked by scene state constraints.
     * @param action Input action event payload to process.
     */
    virtual void doAction(const Action& action);

    /**
     * @brief Retrieves pointer to active Head-Up Display (HUD) if implemented by derived scene.
     * @return Pointer to scene `HUD` instance, or `nullptr` if no HUD exists.
     */
    virtual HUD* getHUD() const { return nullptr; }

    /**
     * @brief Advances scene simulation by a fixed number of logical frames.
     * @param frames Count of frame updates to step through.
     */
    void simulate(const size_t frames);

    /**
     * @brief Registers a keyboard scancode mapping to a named trigger action string.
     * @param inputKey SFML keyboard scancode to bind.
     * @param actionName String label assigned to the action event.
     */
    void registerAction(sf::Keyboard::Scancode inputKey, std::string_view actionName);

    /**
     * @brief Gets active viewport pixel width from game engine context.
     * @return Viewport width in pixels.
     */
    size_t width() const;

    /**
     * @brief Gets active viewport pixel height from game engine context.
     * @return Viewport height in pixels.
     */
    size_t height() const;

    /**
     * @brief Gets cumulative count of simulated frames elapsed in scene lifetime.
     * @return Current frame counter value.
     */
    size_t currentFrame() const;
           
    /**
     * @brief Checks whether the scene has requested termination.
     * @return `true` if scene has ended, `false` otherwise.
     */
    bool hasEnded() const;

    /**
     * @brief Gets reference to the active key/input binding map.
     * @return Constant reference to `m_actionMap`.
     */
    const ActionMap& getActionMap() const;
};