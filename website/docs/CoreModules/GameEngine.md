# High-Level Architecture: `GameEngine` Management & Input Binding Subsystems

The core application execution cycle relies on two closely connected components: the `GameEngine` orchestrator and the `InputAction` string namespace. Working alongside the `Action` class, these subsystems manage hardware event polling, mapping keyboard scancodes and mouse buttons to semantic action strings, running scene stack transitions, driving main-loop tick rates, and integrating UI rendering via ImGui-SFML.

---

## 1. Key Subsystems & Core Structures

### Semantic Action Constants (`InputAction`)

The `InputAction` namespace provides compile-time string view constants (`constexpr std::string_view`) that serve as the universal identifiers across input maps and scene handlers:

* **Mouse Actions**: `LeftClick`, `RightClick`, `MiddleClick`, `MouseMove`, `MouseWheelScroll`.


* **Movement & Stance**: `MoveForward`, `MoveBackward`, `MoveLeft`, `MoveRight`, `MoveUp`, `MoveDown`, `Strafe`, `Jump`, `Sprint`, `Crouch`.


* **Camera & Viewport**: `CameraYawLeft`, `CameraYawRight`, `CameraPitchUp`, `CameraPitchDown`, `ToggleCursor`.


* **System & UI**: `Pause`, `Quit`, `Interact`, `ToggleHeadlight`, `ShowGui`.



### Action Mapping Aliases

Input bindings link physical hardware triggers to `InputAction` strings using standard standard map containers:

* **`ActionMap`**: Maps physical keyboard scancodes (`sf::Keyboard::Scancode`) to semantic action string names.


* **`MouseActionMap`**: Maps physical mouse buttons (`sf::Mouse::Button`) to semantic action string names.



---

## 2. Default Input Configuration & Registration

During engine initialization (`init`), `loadDefaultBindings` sets up the initial default key and mouse mappings:

```cpp
// Sample keybinding initialization in GameEngine::loadDefaultBindings()
registerAction(sf::Keyboard::Scancode::W,       InputAction::MoveForward);    // "MOVE_FORWARD"
registerAction(sf::Keyboard::Scancode::Space,   InputAction::Jump);           // "JUMP"
registerAction(sf::Keyboard::Scancode::F,       InputAction::ToggleCursor);   // "TOGGLE_CURSOR"
registerAction(sf::Keyboard::Scancode::F12,     InputAction::ShowGui);        // "SHOW_GUI"
registerAction(sf::Mouse::Button::Left,         InputAction::LeftClick);      // "LEFT_CLICK"

```

The engine provides `registerAction` overloads for both `sf::Keyboard::Scancode` and `sf::Mouse::Button`, allowing bindings to be modified at runtime.

---

## 3. Input Dispatch (`sUserInput`) & Event Lifecycle

Input event processing occurs during `GameEngine::sUserInput()` at the start of each frame:

```
┌─────────────────────────────────────────────────────────────┐
│                 sf::RenderWindow::pollEvent()               │
└──────────────┬──────────────────────────────┬───────────────┘
               │                              │
               ▼                              ▼
    ┌──────────────────────┐      ┌──────────────────────┐
    │  ImGui::SFML Process │      │   Window Close Check │
    └──────────────────────┘      └──────────────────────┘
               │
               ▼
    ┌────────────────────────────────────────────────────────┐
    │                Hardware Event Matching                 │
    ├──────────────────────────┬─────────────────────────────┤
    │  Key Pressed/Released    │ Matches scancode in         │
    │                          │ ActionMap -> START/END      │
    ├──────────────────────────┼─────────────────────────────┤
    │  Mouse Button Press/Rel  │ Skipped if ImGui mouse      │
    │                          │ captured -> START/END       │
    ├──────────────────────────┼─────────────────────────────┤
    │  Mouse Motion / Look     │ Checks m_mouseCaptured;     │
    │                          │ centers cursor & sends delta│
    ├──────────────────────────┼─────────────────────────────┤
    │  Mouse Wheel Scroll      │ Sends scroll delta + pos    │
    └──────────────────────────┴─────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────────────────────┐
│                 Active Scene Dispatch                       │
│            currentScene()->doAction(Action(...))            │
└─────────────────────────────────────────────────────────────┘

```

1. **GUI Interception**: Events are forwarded to `ImGui::SFML::ProcessEvent`. Mouse events are filtered out if `ImGui::GetIO().WantCaptureMouse` is active.


2. **Keyboard Processing**: Key presses increment global counters (`m_totalKeyPresses`, `m_lastKeyPressed`). Pressed and released keys lookup corresponding scancodes in `m_actionMap` and dispatch `Action(actionName, "START")` or `Action(actionName, "END")` to the active scene.


3. **Mouse & Look Delta Capture**: When `m_mouseCaptured` is enabled (mouse-look mode), cursor motion compares the new position against the window center, dispatches a relative delta action (`InputAction::MouseMove`, `"LOOK"`, `delta`), and resets the cursor to the center. If uncaptured, standard positional movement actions are dispatched.


4. **On-Screen / HUD Analog Pass**: If the active scene HUD provides joystick vector coordinates, these are translated into analog actions (`InputAction::CameraYawRight`, `InputAction::MoveUp`).



---

## 4. Scene Lifetime Management & Input Flushing

Scene stack transitions are managed by `GameEngine::changeScene`:

* **Scene Memory Tracking**: Keeps track of active, previous (`m_previousScene`), and historical (`m_oneBeforeThat`) scenes using a `SceneMap` container.


* **Input Flushing (`flushInput`)**: Whenever a scene transition occurs, `GameEngine::flushInput()` sends `"END"` action notifications for all registered keyboard actions. This prevents stale "key held" states from persisting when switching contexts (e.g., transitioning from active gameplay into a menu).



---

## 5. Planned Roadmap: Dual Analog Controller Support

Similar to upcoming updates in the `Action` class, the `GameEngine` input loop will soon be extended to support native dual analog stick gamepads. Future updates to `sUserInput()` will poll gamepad thumbsticks and triggers using SFML joystick API calls, running radial deadzone filtering and dispatching continuous normalized 2D movement and camera look vector actions directly to `currentScene()`.