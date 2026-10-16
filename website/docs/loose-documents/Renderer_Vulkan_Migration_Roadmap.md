# Renderer and Vulkan Migration Roadmap

This roadmap describes the overall transition from the current OpenGL renderer
to a renderer architecture that can support both OpenGL and Vulkan. It is
intentionally high level. Each stage should receive its own tightly scoped
roadmap before implementation begins.

The roadmap follows the decision recorded in
[ADR-001: Establish a Renderer Backend Boundary Before the Vulkan Port](./ADR-001-renderer-backend-boundary.md).

## Goals

- Keep the game playable throughout the transition.
- Preserve current rendering behavior while the architecture is reorganized.
- Separate gameplay and ECS code from graphics-backend details.
- Establish backend-independent render data.
- Retain OpenGL as a working reference backend during the early Vulkan work.
- Introduce Vulkan incrementally, starting with the smallest useful renderer.
- Support macOS through Vulkan portability tooling such as MoltenVK.
- Make rendering resources, shader interfaces, and synchronization explicit.

## Non-goals

- Rewriting gameplay systems, ECS storage, or terrain simulation as part of the
  renderer transition.
- Replacing SFML, ImGui, or the asset pipeline before their renderer
  integration needs are understood.
- Designing a universal graphics abstraction that hides all differences between
  OpenGL and Vulkan.
- Porting every render pass in one step.
- Optimizing the renderer before the new ownership and backend boundaries are
  stable.

## Guiding principles

1. **Preserve a working build and runnable game.** Every stage should leave the
   project in a state that can be built and exercised.
2. **Separate data from execution.** Scenes describe what should be rendered;
   backends decide how it is rendered.
3. **Prefer explicit ownership.** GPU resources belong to renderer or resource
   objects with clear lifetimes.
4. **Keep backend differences visible.** OpenGL and Vulkan may share render data
   without sharing resource-management APIs.
5. **Migrate by vertical slices.** A small end-to-end Vulkan path is more useful
   than a large collection of incomplete abstractions.
6. **Use the OpenGL renderer as a behavioral reference.** Visual differences
   should be investigated rather than silently accepted.
7. **Document interfaces before porting them.** Buffer layouts, shader bindings,
   formats, and coordinate conventions must be recorded explicitly.

## Target architecture

```text
GameEngine
    |
    +-- Scene_IC_Camp
    |       updates ECS and gameplay state
    |       builds RenderFrame
    |
    +-- Renderer interface
            |
            +-- OpenGLRenderer
            |       GL resources and render passes
            |
            +-- VulkanRenderer
                    Vulkan resources, command buffers,
                    synchronization, and presentation
```

The scene should eventually produce data structures such as:

```cpp
struct CameraFrame;
struct EnvironmentFrame;
struct TerrainRenderData;
struct OrbRenderInstance;
struct RenderFrame;
```

These structures must not contain `GLuint`, `GLenum`, Vulkan handles, or
backend-specific binding commands.

## Stage 0: Baseline and constraints

### Purpose

Record the current rendering behavior and establish the development constraints
before changing ownership.

### Work

- Confirm the existing build and run workflow on the primary development
  platform.
- Record the current render-pass order.
- Record shader stages, buffer layouts, texture formats, and binding locations.
- Identify the current SFML, OpenGL, and ImGui integration points.
- Capture representative screenshots or other visual references for the terrain,
  orb creatures, sky, ocean, shadows, SSAO, minimap, and HUD.
- Identify any existing OpenGL errors, validation warnings, or known rendering
  defects without expanding scope to fix unrelated issues.

### Exit criteria

- A documented baseline can be rebuilt and run.
- The major render passes and GPU resources are inventoried.
- Visual reference cases exist for later comparison.
- Known pre-existing issues are distinguished from migration regressions.

## Stage 1: Extract the OpenGL renderer

### Purpose

Move rendering ownership out of `Scene_IC_Camp` while preserving behavior.

### Work

- Introduce a scene/world renderer object with an OpenGL implementation.
- Move render-pass execution out of `Scene_IC_Camp`.
- Move creation, update, and destruction of framebuffers, textures, buffers,
  meshes, shader programs, UBOs, SSBOs, and shadow resources.
- Keep gameplay systems, ECS updates, terrain queries, astronomy, and input in
  the scene.
- Replace direct scene access to GPU handles with renderer-owned operations.
- Preserve the existing render-pass order and visible output.

### Exit criteria

- `Scene_IC_Camp` no longer owns the primary OpenGL render resources.
- The scene no longer performs the main raw OpenGL render calls.
- The OpenGL-backed game builds, starts, and renders the baseline scene.
- Resource destruction is owned by the corresponding renderer/resource object.

## Stage 2: Define backend-independent render data

### Purpose

Create the interface that both OpenGL and Vulkan implementations will consume.

### Work

- Define camera, environment, terrain, material, species, and instance data
  structures.
- Convert ECS entities into render-oriented data before backend submission.
- Separate render configuration and debug options from OpenGL state.
- Remove OpenGL handles and shader-location constants from the shared data
  structures.
- Document CPU-to-shader memory layout, alignment, coordinate systems, and
  units.
- Define ownership and lifetime rules for frame data and transient arrays.

### Exit criteria

- The renderer can consume a `RenderFrame` without requiring the scene's ECS
  manager.
- Shared render data contains no backend-specific handles or commands.
- OpenGL output remains equivalent to the Stage 1 baseline.
- The interface is sufficient to describe the first Vulkan vertical slice.

## Stage 3: Stabilize the OpenGL reference backend

### Purpose

Make the OpenGL renderer a reliable behavioral reference before adding Vulkan.

### Work

- Introduce consistent RAII wrappers for OpenGL resources where useful.
- Separate shader source/asset descriptions from compiled program handles.
- Centralize OpenGL error and framebuffer completeness reporting.
- Remove unnecessary global-state assumptions between render passes.
- Add focused checks for resource creation, resizing, and cleanup.
- Verify window resize, scene changes, repeated enter/exit, and shutdown paths.

### Exit criteria

- The OpenGL renderer can be initialized and destroyed repeatedly without
  leaking or reusing invalid handles.
- Render passes establish the state they require rather than depending on
  accidental state from an earlier pass.
- The reference backend has a documented set of visual and functional checks.

## Stage 4: Introduce the Vulkan platform foundation

### Purpose

Create a minimal Vulkan runtime without attempting to port the whole renderer.

### Work

- Select and document the Vulkan loader, SDK, and validation-layer requirements.
- Create the Vulkan instance and enable required validation and portability
  extensions.
- Select a physical device and create a logical device and queues.
- Integrate the windowing and presentation path.
- Create the swapchain, image views, command pool, command buffers, fences, and
  semaphores.
- Add platform-specific handling needed for macOS and MoltenVK.
- Establish a debug-reporting and validation workflow.

### Exit criteria

- A Vulkan backend can open the game window, clear a swapchain image, present
  it, and shut down cleanly.
- Validation errors are either resolved or explicitly documented.
- The Vulkan path can be enabled without breaking the OpenGL build.

## Stage 5: Implement the first Vulkan vertical slice

### Purpose

Prove that backend-independent render data can drive a real Vulkan draw.

### Work

- Compile a minimal shader pair to SPIR-V.
- Create descriptor-set and pipeline-layout conventions.
- Create vertex and index buffers.
- Render one known mesh using camera data.
- Add a texture and sampler.
- Render a simple terrain or test scene from `RenderFrame`.
- Compare the result with the OpenGL reference.

### Exit criteria

- Vulkan can draw a textured mesh from shared render data.
- Shader interface conventions are documented and reproducible.
- The Vulkan renderer has a small automated or repeatable smoke test.
- The scene does not need to know whether OpenGL or Vulkan is active.

## Stage 6: Port core scene rendering

### Purpose

Move the primary world-rendering path to Vulkan in incremental passes.

### Suggested order

1. depth buffering and camera transforms;
2. terrain geometry and streamed terrain textures;
3. orb instance data and instanced rendering;
4. sky and atmosphere;
5. G-buffer attachments;
6. deferred lighting;
7. cascaded shadow maps;
8. ocean rendering;
9. SSAO and blur;
10. minimap and remaining scene-specific render targets.

### Work

- Port one pass at a time.
- Preserve the OpenGL backend until the Vulkan equivalent is validated.
- Add Vulkan image usage, layout transitions, barriers, and synchronization as
  each pass requires them.
- Reconcile shader interfaces and resource formats at every pass boundary.
- Compare each pass against the OpenGL reference using the Stage 0 cases.

### Exit criteria

- The main scene can render through Vulkan with the required visual systems.
- Each ported pass has a documented resource and synchronization model.
- OpenGL and Vulkan results are acceptably equivalent for the supported cases.

## Stage 7: Port UI, tools, and integration surfaces

### Purpose

Complete the application integration that is outside the core world renderer.

### Work

- Decide how HUD rendering will coexist with Vulkan presentation.
- Replace or adapt ImGui-SFML integration for the Vulkan path.
- Port the minimap and diagnostic overlays where they depend on OpenGL.
- Review SFML render textures and direct window drawing assumptions.
- Ensure scene transitions, resizing, focus changes, and shutdown work with
  Vulkan.

### Exit criteria

- The complete playable application, including HUD and diagnostics, works on
  the Vulkan backend.
- No UI path requires an accidental OpenGL context.
- Window lifecycle and resize behavior are validated on target platforms.

## Stage 8: Validation, performance, and backend decision

### Purpose

Determine whether Vulkan is ready to become the preferred backend.

### Work

- Run functional and visual regression checks.
- Validate resource cleanup under repeated scene transitions.
- Measure CPU submission cost, frame time, memory use, and synchronization
  stalls.
- Test macOS through MoltenVK and the primary non-macOS target.
- Record unsupported features, driver limitations, and fallback behavior.
- Decide whether OpenGL remains supported, becomes a fallback, or is removed.

### Exit criteria

- Vulkan meets the project's functional and platform requirements.
- Performance and memory behavior are understood.
- Known limitations and fallback behavior are documented.
- The project has an explicit backend support policy.

## Cross-stage dependencies

```text
Stage 0 Baseline
        |
        v
Stage 1 OpenGL extraction
        |
        v
Stage 2 Render data
        |
        v
Stage 3 OpenGL reference
        |
        +--------------------+
        |                    |
        v                    v
Stage 4 Vulkan foundation   Stage 3 validation
        |
        v
Stage 5 Vulkan vertical slice
        |
        v
Stage 6 Core render passes
        |
        v
Stage 7 UI and integration
        |
        v
Stage 8 Validation and backend policy
```

Stages 1 through 3 should be completed before substantial Vulkan porting begins.
Stage 4 may be prototyped in parallel with the later parts of Stage 3, but the
Vulkan prototype must consume the interface established by Stage 2 rather than
creating a second scene-specific interface.

## Risk controls

- Keep OpenGL available until Vulkan has passed the visual and functional
  validation stages.
- Make each stage independently buildable and runnable.
- Avoid broad mechanical renames that obscure ownership changes.
- Port render passes incrementally rather than replacing the complete pipeline.
- Treat validation-layer errors and synchronization warnings as defects to
  resolve, not as harmless noise.
- Keep platform-specific window and UI integration isolated from backend-neutral
  render data.
- Record temporary compatibility shims and give each one a removal condition.

## First detailed roadmap to write

The first stage-specific roadmap should be **Stage 0: Baseline and
constraints**. It should inventory the current render passes, resources, shader
interfaces, window/UI integration, build assumptions, and repeatable visual
checks. That information will make the Stage 1 OpenGL extraction smaller and
less speculative.

