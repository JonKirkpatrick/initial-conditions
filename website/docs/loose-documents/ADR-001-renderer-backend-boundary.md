# ADR-001: Establish a Renderer Backend Boundary Before the Vulkan Port

- **Status:** Accepted
- **Date:** 2026-10-16
- **Decision owners:** Initial Conditions Engine

## Context

The engine currently renders the interactive campaign scene through OpenGL. This
works well on platforms with a sufficiently modern OpenGL implementation, but
macOS is limited to OpenGL 4.1. The engine also uses features and shader
interfaces associated with newer OpenGL versions, so continuing to target modern
OpenGL would limit portability.

The planned long-term direction is Vulkan. On macOS, Vulkan can be provided
through MoltenVK, which translates Vulkan operations to Apple's Metal backend.
This is a backend change, not merely a shader-language conversion: Vulkan
requires explicit resource ownership, pipeline creation, descriptor
management, command recording, synchronization, and image-layout transitions.

At present, `Scene_IC_Camp` combines gameplay and rendering responsibilities. It
owns OpenGL program handles, vertex arrays, buffers, framebuffers, textures,
uniform buffers, shadow-map resources, and SSAO resources. It also directly
creates, updates, destroys, and binds those resources while performing render
passes. This makes the scene difficult to reason about independently of OpenGL
and would make a direct Vulkan rewrite unnecessarily risky.

The current resource wrappers such as `OrbSSBO`, `MaterialSSBO`, and
`SpeciesSSBO` are useful ownership improvements, but they are still explicitly
OpenGL implementations. Likewise, `Assets` currently exposes and owns several
OpenGL-specific resources alongside CPU-side assets and SFML resources.

## Decision

We will establish a renderer boundary before implementing the Vulkan backend.

The boundary will separate:

1. **Scene and gameplay code**
   - ECS updates
   - input and movement
   - terrain and astronomy simulation
   - camera and environment state
   - selection of entities and world data that should be rendered

2. **Backend-independent render data**
   - camera frame data
   - environment and lighting data
   - terrain render data
   - orb instance data
   - material and species data
   - render options and debug flags

3. **Backend-specific renderer implementations**
   - OpenGL resource ownership and state management
   - Vulkan resource ownership and command recording
   - shader/pipeline setup
   - render-pass execution
   - synchronization and presentation

The first implementation will retain OpenGL. It will move rendering ownership
and render-pass execution out of `Scene_IC_Camp` without changing visible
rendering behavior. A later Vulkan renderer will consume the same
backend-independent render data rather than attempting to reuse OpenGL handles
or OpenGL's state-machine API.

The initial shape is expected to be similar to:

```text
Scene_IC_Camp
    updates ECS and gameplay state
    builds a RenderFrame
            |
            v
WorldRenderer / SceneRenderer
    owns render passes and backend resources
            |
            +-- OpenGLRenderer
            +-- VulkanRenderer
```

OpenGL-specific types will remain visibly OpenGL-specific during the first
stage. Classes such as `GLBuffer`, `GLTexture`, `GLFramebuffer`, and
`GLWorldRenderer` are preferred to abstractions that pretend OpenGL's `bind`
operations map directly to Vulkan. Vulkan will have its own resource and
pipeline types where the ownership and usage model differs.

## Initial scope

The first refactoring wave will focus on the GPU work currently embedded in
`Scene_IC_Camp`:

- G-buffer creation, resizing, and destruction
- SSAO framebuffers, textures, and passes
- cascaded shadow-map resources and passes
- terrain, ocean, cube, fullscreen-quad, and related mesh resources
- uniform-buffer creation and updates
- shader program lookup and uniform uploads
- render-pass ordering and OpenGL state changes
- cleanup of OpenGL resources

The scene will continue to own gameplay state and will provide the renderer
with explicit data rather than exposing the entity manager as the renderer's
primary interface. For example, orb entities should be converted into a
render-oriented instance structure before being uploaded.

## Implementation sequence

### 1. Extract an OpenGL renderer

Introduce a renderer object that owns the resources and pass functions currently
owned by `Scene_IC_Camp`. Preserve the existing pass order:

1. terrain
2. orb creatures
3. shadow maps
4. SSAO
5. SSAO blur
6. sky
7. deferred lighting
8. ocean
9. HUD/UI integration

The first milestone is behavioral parity, not a new graphics API.

### 2. Define backend-independent frame data

Create data structures that do not contain `GLuint`, `GLenum`, OpenGL shader
locations, or calls to `gl*` functions. Candidate structures include:

```cpp
struct CameraFrame;
struct EnvironmentFrame;
struct OrbRenderInstance;
struct TerrainRenderData;
struct RenderFrame;
```

These structures describe what the renderer needs to draw, not how a particular
backend stores or binds it.

### 3. Improve resource ownership

Use RAII for OpenGL resources and make ownership explicit. Resource wrappers
should release their own resources and should not be copyable when copying would
duplicate or invalidate a GPU handle.

Static asset descriptions and CPU-side registries should be separated over time
from backend resource caches. In particular, scene code should not need to
retrieve raw OpenGL program handles directly from the general asset manager.

### 4. Port a small Vulkan path

Do not begin the Vulkan port with the full deferred renderer. First implement a
small Vulkan path that can:

1. create a device and swapchain;
2. clear and present a frame;
3. draw one mesh;
4. draw a textured mesh;
5. render the basic terrain path.

Add instancing, G-buffer targets, lighting, shadows, SSAO, ocean, and other
passes incrementally. Each step should leave a working renderer.

## Alternatives considered

### Leave the scene unchanged until after the Vulkan port

Rejected. This would require understanding gameplay code, OpenGL state, resource
lifetime, and Vulkan's explicit model simultaneously. It would also encourage a
large rewrite with difficult-to-isolate regressions.

### Hide OpenGL and Vulkan behind one generic `Buffer` or `Texture` API

Rejected as the primary design. Operations such as OpenGL `bind` calls do not
map cleanly to Vulkan descriptor sets, pipeline layouts, command buffers, and
resource barriers. A superficial common API would likely preserve OpenGL's
state-machine assumptions and make the Vulkan backend harder to implement.

### Keep OpenGL and only lower shader versions to 4.1

Not selected as the long-term direction. This may be useful as a temporary
compatibility investigation, but it would not address the broader portability
and explicit-rendering goals. Some shader and resource features may also
require algorithmic changes rather than syntax changes.

### Use a complete third-party rendering abstraction

Deferred. A framework could reduce backend work, but adopting one would be a
larger architectural decision with implications for asset loading, SFML
integration, ImGui, ECS data flow, and the existing deferred pipeline. The first
step should clarify the engine's own renderer boundary before deciding whether a
third-party abstraction is appropriate.

## Consequences

### Benefits

- Gameplay scenes become less coupled to OpenGL.
- GPU resource lifetime becomes easier to inspect and test.
- OpenGL behavior can be preserved while the architecture is improved.
- Vulkan can be introduced incrementally rather than as one large rewrite.
- Backend-independent render data can be validated without requiring a graphics
  API context.
- OpenGL and Vulkan implementations can use their natural resource models.

### Costs and risks

- There will temporarily be more types and files.
- Some data will be copied or transformed from ECS storage into render data.
- The OpenGL renderer will initially be an additional layer rather than an
  immediate performance improvement.
- SFML rendering, ImGui-SFML, window creation, and presentation will need an
  explicit integration plan for Vulkan.
- Shader interfaces and buffer layouts must be documented carefully so that
  GLSL/SPIR-V, OpenGL, and Vulkan agree on memory layout and bindings.

## Success criteria

This decision is considered successfully implemented when:

- `Scene_IC_Camp` no longer owns the primary OpenGL render resources;
- `Scene_IC_Camp` no longer directly performs the main `gl*` render calls;
- the OpenGL renderer preserves the existing scene's visible behavior;
- render-frame structures contain no OpenGL-specific handles or enums;
- resource destruction is tied to owning renderer/resource objects;
- a small Vulkan prototype can consume the render-frame interface without
  requiring the scene to know Vulkan details.

