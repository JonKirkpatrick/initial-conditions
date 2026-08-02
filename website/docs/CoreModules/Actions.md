# High-Level Architecture: `Action` Class

The `Action` class serves as the abstraction layer for the engine's input system. It decouples low-level hardware inputs—such as keyboard keypresses, mouse movements, scrolling, and gamepad triggers—from gameplay and engine logic by translating raw device signals into named semantic events (e.g., `"JUMP"`, `"PAUSE"`, `"MOVE"`).

Rather than querying raw key codes or device states directly, downstream systems consume uniform `Action` payloads that describe both the intent (name and type/phase) and any associated data attributes.

---

## 1. Supported Input Modalities & Data Attributes

The `Action` class acts as a variant-style container that encapsulates state payload attributes tailored to different hardware input modes:

* **Semantic Identifiers (`m_name`, `m_type`)**: Core identifiers present in every action. `m_name` specifies the action identifier (e.g., `"JUMP"`), while `m_type` defines the input state or phase (e.g., `"START"`, `"END"`).


* **Digital Actions**: Basic binary actions (e.g., button presses or key releases) instantiated with just a name and phase.


* **Positional Actions (`m_position`)**: Encapsulates 2D pixel coordinates (`sf::Vector2i`) associated with pointing devices (e.g., cursor position during mouse clicks).


* **Scroll Actions (`m_scrollDelta`)**: Captures floating-point scroll wheel offsets alongside cursor position for UI navigation or zoom controls.


* **Scalar Analog Actions (`m_value`)**: Stores single-axis scalar magnitudes (usually in range $[0.0, 1.0]$) for trigger pressure or single-axis inputs.


* **2D Delta Actions (`m_delta`)**: Represents continuous directional movement vectors (`sf::Vector2f`) used for mouse look, panning, or relative motion.



---

## 2. API Design & Constructor Overloads

To handle multiple hardware sources seamlessly, `Action` provides constructor overloads that accept both `std::string_view` and `const std::string&` parameters, avoiding unnecessary string allocations when passing string literals:

```cpp
// Digital Action
Action jumpAction("JUMP", "START");

// Positional Action (Mouse Click)
Action clickAction("SELECT", "START", sf::Vector2i(400, 300));

// Directional Delta (Mouse Look / Relative Motion)
Action lookAction("LOOK", "UPDATE", sf::Vector2f(12.5f, -3.2f));

// Scalar Analog Action (Trigger Pressure)
Action triggerAction("ACCELERATE", "UPDATE", 0.85f);

```

Accessors (`name()`, `type()`, `pos()`, `scrollDelta()`, `value()`, `delta()`) provide read-only access to state attributes for consumption by scene handlers and gameplay systems.

---

## 3. Architecture & Input Handling Lifecycle

```
┌───────────────────────────────────────────────────────────┐
│                    Raw Hardware Input                     │
│      (Keyboard, Mouse, Gamepad Triggers / Joysticks)      │
└─────────────────────────────┬─────────────────────────────┘
                              │
                              ▼
┌───────────────────────────────────────────────────────────┐
│                     Input Map / Manager                   │
│         (Maps raw keycodes/axes -> Action events)         │
└─────────────────────────────┬─────────────────────────────┘
                              │
                              ▼
┌───────────────────────────────────────────────────────────┐
│                     Action Instance                       │
│  - Name ("MOVE")           - Value / Delta / Position     │
│  - Type ("START" / "END")  - Unified Data Container       │
└─────────────────────────────┬─────────────────────────────┘
                              │
                              ▼
┌───────────────────────────────────────────────────────────┐
│                     Gameplay Systems                      │
│        (Consumes semantic actions without raw device      │
│                     hardware coupling)                    │
└───────────────────────────────────────────────────────────┘

```

1. **Hardware Capture**: Input events from SFML or raw hardware devices are received by the engine's input management layer.


2. **Action Translation**: The input manager matches device signals against mapped keybindings and constructs the appropriate `Action` object with scalar, positional, or delta context.


3. **Dispatch to Systems**: Scene controllers process active `Action` instances, enabling full remapping and multi-device support without altering gameplay logic.



---

## 4. Planned Roadmap: Dual Analog Controller Support

While the current `Action` implementation fully covers keyboard, mouse, scalar analog triggers, and 2D relative deltas, upcoming updates to the input pipeline will expand support to native **dual analog stick controller inputs**. This will leverage the 2D delta (`m_delta`) and scalar (`m_value`) channels to accommodate thumbstick radial deadzones, normalized $2D$ vector input fields, and multi-gamepad device indexing.