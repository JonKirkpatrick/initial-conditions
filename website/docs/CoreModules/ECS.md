# Architecture & Technical Reference: Entity Component System (ECS) Architecture

This document provides a comprehensive technical reference for the custom Structure of Arrays (SoA) Entity Component System (ECS) in the engine. It covers low-level memory layouts, generational handle validation, component structure definitions, and query execution mechanisms.

---

## 1. High-Level ECS Architecture & Storage Paradigm

The custom ECS architecture uses a **Structure of Arrays (SoA)** design backed by sparse-dense packed component vectors. This structure keeps components of the same type stored in contiguous cache-friendly arrays while supporting $O(1)$ allocations, lookups, and deletions.

```
                     ┌──────────────────────────────────────────────┐
                     │           EntityManager Orchestrator         │
                     └──────────────────────┬───────────────────────┘
                                            │
               ┌────────────────────────────┼────────────────────────────┐
               ▼                            ▼                            ▼
┌────────────────────────────┐┌────────────────────────────┐┌────────────────────────────┐
│      SoAEntityPool         ││  m_activeEntities / Tags   ││   Command Queues           │
│  - Generational Handle Gen ││  - Packed Entity Handles   ││  - Deferred Spawns         │
│  - Index Recycling Stack   ││  - Fast Swap-Remove Tables ││  - Deferred Destroys       │
└────────────────────────────┘└────────────────────────────┘└────────────────────────────┘
                                            │
                                            ▼
               ┌─────────────────────────────────────────────────────────┐
               │    soa::ComponentArray<T> (Sparse-Dense Storage)        │
               ├────────────────────────────┬────────────────────────────┤
               │  Dense Component Vector    │ std::vector<T> m_values    │
               │  Dense Entity Handles      │ std::vector<EntityHandle>  │
               │  Sparse Lookup Mapping     │ std::vector<uint32_t>      │
               └────────────────────────────┴────────────────────────────┘

```

---

## 2. Low-Level Memory & Lifetime Primitives

### Generational Entity Handles (`soa::EntityHandle`)

Entities are represented by lightweight 64-bit handles (`SoAEntityHandle`):

* **`index` (32-bit `uint32_t`)**: Flat array index pointing into allocation pools and component arrays.


* **`generation` (32-bit `uint32_t`)**: Monotonically increasing counter incremented every time an index slot is recycled upon entity destruction. This guards against dangling references and stale accesses.



### Recycling Pool (`soa::EntityPool`)

Manages up to $\text{MAX\_ENTITIES} = 262,144$ simultaneous entity instances:

* **Free Index Stack**: Pre-filled LIFO vector (`freeList`) providing $O(1)$ index allocation.


* **Handle Creation & Validation**: `create()` pops an index from `freeList` and pairs it with the current generation counter for that slot. `valid(h)` ensures the handle's generation matches the pool's current active generation for that slot.



### Sparse-Dense Component Array (`soa::ComponentArray<T>`)

Maintains flat, contiguous arrays (`m_values`) for component type $T$:

* **Contiguous Cache Locality**: Active components occupy contiguous indices $[0, \text{m\_liveCount})$. Iterating over components via `each()` processes memory sequentially.


* **$O(1)$ Swap-Remove**: When a component is deleted, the last element in the dense array overwrites the deleted slot, updating the corresponding sparse lookup entries (`m_denseSlotByEntity`) and decrementing `m_liveCount`.



---

## 3. Pure Component Definitions (`Components.h`)

Components are plain data structures (PODs) satisfying standard C++ default constructibility rules:

### Spatial & Transform

* **`CTransform3D`**: Tracks 3D position (`pos`), scale (`scale`), linear velocity (`velocity`), and orientation (`m_orientation` quaternion). Features a dirty flag pattern (`m_isDirty`) that defers directional vector re-computation ($\vec{forward}$, $\vec{right}$, $\vec{up}$) until needed by rendering or physics systems.



### Movement & Locomotion

* **`CPhysics`**: Physical parameters including gravity coefficient ($9.81\text{ m/s}^2$), jump velocity ($4.20\text{ m/s}$), ground friction ($12.0$), air drag ($3.0$), standing/crouch height constraints, and grounding flags.


* **`CGaitCycle`**: View presentation state tracking step cycle phase (`accumulator`), stride rate, vertical bob amplitude, and lateral sway amplitude for camera motion and footstep audio triggers.


* **`CKinematicBob`**: Phase accumulator used directly by non-player entities (e.g., floating collectibles) to drive physical visual vertical oscillations.


* **`CBob`**: Legacy bobbing component preserved for backward compatibility.



### Camera & Input

* **`CCamera`**: Perspective matrix configuration storing vertical FOV (radians), aspect ratio, near/far plane limits, and pixel viewport dimensions.


* **`CInput`**: Per-frame state accumulator storing boolean digital inputs (`forward`, `strafe`, `jump`, etc.), normalized analog joystick axes ($[-1.0, 1.0]$), and mouse delta vectors.


* **`CPlayer`**: Lightweight tag component identifying the local player entity.



### Visual Appearance & Animation

* **`COrb`**: Interactive spherical mesh properties including radius, custom orientation basis vectors, and visual species texture palette index (`speciesIdx`).


* **`CEyes`**: Procedural animation state tracking 2D local gaze offset vectors, pupil dilation ($[0.0, 1.0]$), and eyelid closure ratios ($[0.0, 1.0]$).



---

## 4. Manager API & Multi-Component Queries

The `EntityManager` serves as the central API for spawning, queueing, destroying, and querying entities.

### Command Buffer Queueing

To avoid invalidating iteration loops during system updates, mutations are deferred via command buffers flushed at frame boundaries via `EntityManager::update()`:

* **`queueSpawn(tag, initializer)`**: Queues an entity spawn request, optionally executing a configuration lambda post-allocation.


* **`queueDestroy(handle)`**: Queues an entity handle for destruction and component detachment.



### Optimized Joint Iteration (Joins)

Instead of searching all active entities for component matches, `EntityManager` provides specialized join functions that iterate directly over dense component arrays:

```cpp
// Iterating over entities possessing both COrb and CTransform3D
entityManager.forEachOrbWithTransform([](SoAEntityHandle h, CTransform3D& transform, COrb& orb) {
    transform.pos += transform.velocity * deltaTime;
});

// Joins querying COrb, CTransform3D, and optional CEyes components
entityManager.forEachOrbWithComponents([](SoAEntityHandle h, CTransform3D& transform, COrb& orb, CEyes* eyes) {
    if (eyes) {
        eyes->pupilDilation = std::sin(time) * 0.5f + 0.5f;
    }
});

```

---

## 5. Summary Matrix: Component Storage & Access Features

| Component Class | Dense Array Storage | Primary Dependent Systems | Memory Overhead / Properties |
| --- | --- | --- | --- |
| **`CTransform3D`** | `m_compTransform` | Physics, Rendering, Scene Graphs | Quaternion orientation; lazy vector sync dirty flag. |
| **`CPhysics`** | `m_compPhysics`<br> | Collision, Locomotion, Kinematic Solvers | Friction, stance heights, and velocity states. |
| **`CGaitCycle`** | `m_compGaitCycle`<br> | Camera Bob, Footstep Audio | View bobbing ccumulator & stride phase limits.|
| **`CKinematicBob`** | `m_compKinematicBob`<br> | Visual Oscillators, Collectibles | Parametric vertical positioning phase.|
| **`CCamera`** | `m_compCamera`<br> | Viewport Projection, Visibility Culling | Perspective FOV and viewport dimensions. |
| **`CInput`** | `m_compInput`<br> | Input Dispatcher, Player Movement | Hardware state accumulator & mouse deltas. |
| **`COrb`** | `m_compOrb`<br> | Mesh Rendering, Visual Customization | Radius, custom basis vectors, and texture index. |
| **`CEyes`** | `m_compEyes`<br> | Procedural Eye Shader/Animation | Normalized gaze vectors, dilation, and eyelid parameters.