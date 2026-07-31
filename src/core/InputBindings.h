#pragma once

#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include <map>
#include <string>
#include <string_view>

/**
 * @brief Map alias associating physical keyboard key scancodes with semantic action names.
 */
using ActionMap = std::map<sf::Keyboard::Scancode, std::string>;

/**
 * @brief Map alias associating semantic action names back to physical keyboard key scancodes.
 */
using ReverseActionMap = std::map<std::string, sf::Keyboard::Scancode>;

/**
 * @brief Map alias associating mouse buttons with semantic action names.
 */
using MouseActionMap = std::map<sf::Mouse::Button, std::string>;

/**
 * @brief Collection of standard semantic string identifiers for input action binding.
 * 
 * Provides compile-time string view constants used to decouple hardware-level key or button 
 * presses from engine logic. Scenes subscribe to these predefined action identifiers.
 */
namespace InputAction
{
    /// @name Mouse Actions
    /// @{

    /** @brief Action triggered on primary/left mouse button press (`"LEFT_CLICK"`). */
    inline constexpr std::string_view LeftClick         = "LEFT_CLICK";

    /** @brief Action triggered on middle mouse button/wheel press (`"MIDDLE_CLICK"`). */
    inline constexpr std::string_view MiddleClick       = "MIDDLE_CLICK";

    /** @brief Action triggered on secondary/right mouse button press (`"RIGHT_CLICK"`). */
    inline constexpr std::string_view RightClick        = "RIGHT_CLICK";

    /** @brief Action triggered on mouse cursor movement (`"MOUSE_MOVE"`). */
    inline constexpr std::string_view MouseMove         = "MOUSE_MOVE";

    /** @brief Action triggered on vertical or horizontal mouse wheel scroll (`"MOUSE_WHEEL_SCROLL"`). */
    inline constexpr std::string_view MouseWheelScroll  = "MOUSE_WHEEL_SCROLL";

    /// @}

    /// @name Generic & Navigation Actions
    /// @{

    /** @brief Action to pause current gameplay/simulation (`"PAUSE"`). */
    inline constexpr std::string_view Pause             = "PAUSE";

    /** @brief Action to terminate application or exit current state (`"QUIT"`). */
    inline constexpr std::string_view Quit              = "QUIT";

    /** @brief Generic menu navigation UP action (`"UP"`). */
    inline constexpr std::string_view Up                = "UP";

    /** @brief Generic menu navigation DOWN action (`"DOWN"`). */
    inline constexpr std::string_view Down              = "DOWN";

    /** @brief Action to start or resume gameplay (`"PLAY"`). */
    inline constexpr std::string_view Play              = "PLAY";

    /** @brief Action to toggle player/entity headlight state (`"TOGGLE_HEADLIGHT"`). */
    inline constexpr std::string_view ToggleHeadlight   = "TOGGLE_HEADLIGHT";

    /** 
     * @brief Action to toggle between mouse captured (camera look mode) and free cursor mode (`"TOGGLE_CURSOR"`).
     */
    inline constexpr std::string_view ToggleCursor      = "TOGGLE_CURSOR";

    /// @}

    /// @name Camera Control Actions
    /// @{

    /** @brief Action to pan camera yaw to the left (`"CAMERA_YAW_LEFT"`). */
    inline constexpr std::string_view CameraYawLeft     = "CAMERA_YAW_LEFT";

    /** @brief Action to pan camera yaw to the right (`"CAMERA_YAW_RIGHT"`). */
    inline constexpr std::string_view CameraYawRight    = "CAMERA_YAW_RIGHT";

    /** @brief Action to tilt camera pitch upward (`"CAMERA_PITCH_UP"`). */
    inline constexpr std::string_view CameraPitchUp     = "CAMERA_PITCH_UP";

    /** @brief Action to tilt camera pitch downward (`"CAMERA_PITCH_DOWN"`). */
    inline constexpr std::string_view CameraPitchDown   = "CAMERA_PITCH_DOWN";

    /// @}

    /// @name Player & Entity Movement Actions
    /// @{

    /** @brief Directional forward movement action (`"MOVE_FORWARD"`). */
    inline constexpr std::string_view MoveForward       = "MOVE_FORWARD";

    /** @brief Directional backward movement action (`"MOVE_BACKWARD"`). */
    inline constexpr std::string_view MoveBackward      = "MOVE_BACKWARD";

    /** @brief Vertical upward movement action (`"MOVE_UP"`). */
    inline constexpr std::string_view MoveUp            = "MOVE_UP";

    /** @brief Vertical downward movement action (`"MOVE_DOWN"`). */
    inline constexpr std::string_view MoveDown          = "MOVE_DOWN";

    /** @brief Lateral left movement action (`"MOVE_LEFT"`). */
    inline constexpr std::string_view MoveLeft          = "MOVE_LEFT";

    /** @brief Lateral right movement action (`"MOVE_RIGHT"`). */
    inline constexpr std::string_view MoveRight         = "MOVE_RIGHT";

    /** @brief Lateral strafe modifier action (`"STRAFE"`). */
    inline constexpr std::string_view Strafe            = "STRAFE";

    /** @brief Vertical jump action (`"JUMP"`). */
    inline constexpr std::string_view Jump              = "JUMP";

    /** @brief Sprint speed boost modifier action (`"SPRINT"`). */
    inline constexpr std::string_view Sprint            = "SPRINT";

    /** @brief Crouch / stance lower modifier action (`"CROUCH"`). */
    inline constexpr std::string_view Crouch            = "CROUCH";

    /// @}

    /// @name World Interaction Actions
    /// @{

    /** @brief World object interaction action (`"INTERACT"`). */
    inline constexpr std::string_view Interact          = "INTERACT";

    /** @brief Generic contextual action (`"ACTION"`). */
    inline constexpr std::string_view Action            = "ACTION";

    /// @}

    /// @name Debug & Overlay Actions
    /// @{

    /** @brief Action to trigger frame capture mode (`"TOGGLE_CAPTURE"`). */
    inline constexpr std::string_view ToggleCapture     = "TOGGLE_CAPTURE";

    /** @brief Action to toggle visibility of ImGui debug overlay (`"SHOW_GUI"`). */
    inline constexpr std::string_view ShowGui           = "SHOW_GUI";

    /// @}
}