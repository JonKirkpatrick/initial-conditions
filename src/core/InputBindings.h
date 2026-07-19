#pragma once

#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include <map>
#include <string>
#include <string_view>

using ActionMap = std::map<sf::Keyboard::Scancode, std::string>;
using ReverseActionMap = std::map<std::string, sf::Keyboard::Scancode>;
using MouseActionMap = std::map<sf::Mouse::Button, std::string>;

namespace InputAction
{
    // Mouse actions
    inline constexpr std::string_view LeftClick         = "LEFT_CLICK";
    inline constexpr std::string_view MiddleClick       = "MIDDLE_CLICK";
    inline constexpr std::string_view RightClick        = "RIGHT_CLICK";
    inline constexpr std::string_view MouseMove         = "MOUSE_MOVE";
    inline constexpr std::string_view MouseWheelScroll  = "MOUSE_WHEEL_SCROLL";

    // Generic actions
    inline constexpr std::string_view Pause             = "PAUSE";
    inline constexpr std::string_view Quit              = "QUIT";
    inline constexpr std::string_view Up                = "UP";
    inline constexpr std::string_view Down              = "DOWN";
    inline constexpr std::string_view Play              = "PLAY";
    inline constexpr std::string_view ToggleHeadlight   = "TOGGLE_HEADLIGHT";
    inline constexpr std::string_view ToggleCursor      = "TOGGLE_CURSOR";      // toggle between mouse captured (look mode) and free cursor mode

    // Camera controls
    inline constexpr std::string_view CameraYawLeft     = "CAMERA_YAW_LEFT";
    inline constexpr std::string_view CameraYawRight    = "CAMERA_YAW_RIGHT";
    inline constexpr std::string_view CameraPitchUp     = "CAMERA_PITCH_UP";
    inline constexpr std::string_view CameraPitchDown   = "CAMERA_PITCH_DOWN";

    // Movement
    inline constexpr std::string_view MoveForward       = "MOVE_FORWARD";
    inline constexpr std::string_view MoveBackward      = "MOVE_BACKWARD";
    inline constexpr std::string_view MoveUp            = "MOVE_UP";
    inline constexpr std::string_view MoveDown          = "MOVE_DOWN";
    inline constexpr std::string_view MoveLeft          = "MOVE_LEFT";
    inline constexpr std::string_view MoveRight         = "MOVE_RIGHT";
    inline constexpr std::string_view Strafe            = "STRAFE";
    inline constexpr std::string_view Jump              = "JUMP";
    inline constexpr std::string_view Sprint            = "SPRINT";
    inline constexpr std::string_view Crouch            = "CROUCH";

    // Interaction
    inline constexpr std::string_view Interact          = "INTERACT";
    inline constexpr std::string_view Action            = "ACTION";

    // Debug / Testing
    inline constexpr std::string_view ToggleCapture     = "TOGGLE_CAPTURE";     // Custom action for toggling frame capture
    inline constexpr std::string_view ShowGui           = "SHOW_GUI";           // Toggle ImGui debug overlay
}