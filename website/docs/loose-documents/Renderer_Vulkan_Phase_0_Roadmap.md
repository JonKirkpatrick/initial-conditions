# Renderer and Vulkan Migration — Phase 0 Roadmap

## Baseline and Constraints

Phase 0 establishes a trustworthy description of the current renderer before
changing ownership or introducing Vulkan. The goal is not to improve the
renderer yet. The goal is to make its current behavior, dependencies, and
limitations explicit so that later refactoring can distinguish intentional
changes from regressions.

This phase implements the first stage of the broader
[Renderer and Vulkan Migration Roadmap](./Renderer_Vulkan_Migration_Roadmap.md).
It supports the decision recorded in
[ADR-001: Establish a Renderer Backend Boundary Before the Vulkan Port](./ADR-001-renderer-backend-boundary.md).

## Scope

### Included

- Establishing a reproducible build and run baseline
- Recording the current platform, compiler, graphics API, and dependency
  assumptions
- Inventorying render-pass order and pass ownership
- Inventorying OpenGL resources and their lifetimes
- Inventorying shader stages, inputs, outputs, buffers, textures, and bindings
- Recording SFML, window, OpenGL-context, ImGui, and HUD integration points
- Capturing representative visual references
- Recording known defects and limitations without fixing unrelated issues
- Producing an evidence package that can guide Phase 1 extraction

### Excluded

- Moving code between classes
- Introducing a renderer interface
- Introducing Vulkan or MoltenVK
- Changing shader code or buffer layouts
- Replacing SFML, ImGui-SFML, or the asset manager
- Fixing pre-existing rendering defects unless they prevent baseline capture
- Performance optimization
- Broad cleanup or style-only changes

## Phase 0 deliverables

At the end of this phase, the following records should exist in this loose
documents directory:

1. **Baseline execution record**
   - build command
   - run command
   - platform and toolchain
   - commit or revision used
   - result and any limitations

2. **Render-pass inventory**
   - pass order
   - entry point
   - inputs and outputs
   - framebuffer or target
   - major GPU resources
   - scene-owned versus asset-owned state

3. **Shader and resource inventory**
   - shader programs and source files
   - stage combinations
   - uniforms and uniform blocks
   - SSBOs and texture bindings
   - render-target formats
   - vertex/index layouts

4. **Platform and integration constraints**
   - window creation and context ownership
   - SFML rendering calls
   - OpenGL state assumptions
   - ImGui-SFML integration
   - resize and scene-lifecycle behavior
   - macOS and portability considerations

5. **Visual reference set**
   - representative screenshots or equivalent records
   - capture conditions
   - expected visible features
   - known artifacts

6. **Phase 1 handoff checklist**
   - first extraction boundary
   - resources to move first
   - dependencies that must remain in the scene
   - tests and checks to repeat after extraction

These records may be split into additional documents if they become too large,
but they should remain linked from this roadmap or from a Phase 0 index.

## Working rules

- Start from a clean or explicitly recorded working-tree state.
- Do not discard, overwrite, or reset unrelated user changes.
- Record the revision used for every baseline run.
- Prefer existing build and run commands over creating new tooling.
- Do not treat a failed optional diagnostic as a renderer failure without
  recording the reason.
- Record observations with enough detail for another developer to reproduce
  them.
- Separate facts, hypotheses, and known defects.
- If a baseline cannot be captured, stop at the blocked item and record the
  blocker rather than proceeding with assumptions.

## Workstream 0.1: Establish the source revision and workspace state

### Objective

Ensure that all later observations can be tied to a known source state.

### Steps

1. Record the current branch and commit revision.
2. Record whether the working tree contains changes before baseline work.
3. Identify any generated files or local configuration that affect building or
   running the game.
4. Decide whether baseline artifacts should be stored in the repository,
   outside the repository, or both.
5. Do not modify source files as part of this workstream.

### Evidence

- branch name
- commit hash
- working-tree status
- list of relevant local-only prerequisites

### Exit criteria

- The baseline revision is unambiguous.
- Existing worktree changes are documented and will not be confused with Phase
  0 work.

## Workstream 0.2: Verify the build and run path

### Objective

Prove that the current project can be built and launched using documented
commands, or record the exact blocker if it cannot.

### Steps

1. Inspect the existing build instructions and `Makefile`.
2. Identify the required compiler, C++ standard, SFML version, OpenGL loader,
   and other link-time dependencies.
3. Run the smallest normal build command.
4. Record warnings and errors separately.
5. Launch the game using the normal run path.
6. Confirm that the primary scene can be entered.
7. Exercise normal shutdown.
8. If the application cannot run in the current environment, record the
   missing dependency, display limitation, or runtime failure without changing
   project configuration.

### Evidence

- exact build command
- exact run command
- toolchain versions
- build result
- startup result
- scene-entry result
- shutdown result

### Exit criteria

- A repeatable build and run procedure is known; or
- a precise, reproducible blocker has been documented.

## Workstream 0.3: Record the runtime and graphics environment

### Objective

Document the environment in which the baseline behavior was observed.

### Steps

1. Record operating system and architecture.
2. Record compiler and linker versions.
3. Record SFML and GLEW versions or package sources.
4. Record GPU vendor, model, driver, and reported OpenGL version.
5. Record the GLSL version reported by the active context if available.
6. Record display resolution, window size, and whether the application is
   windowed or fullscreen.
7. Record relevant environment variables or launch options.
8. Repeat the most important observations on macOS when hardware access is
   available, especially the reported OpenGL version and shader-compile
   behavior.

### Evidence

A short environment table containing:

| Category | Baseline value |
|---|---|
| OS and architecture |  |
| Compiler |  |
| C++ standard |  |
| SFML |  |
| GLEW |  |
| GPU and driver |  |
| OpenGL version |  |
| GLSL version |  |
| Window dimensions |  |

### Exit criteria

- The baseline environment is recorded well enough to explain platform-specific
  results.

## Workstream 0.4: Inventory render-pass execution

### Objective

Describe what the renderer does each frame and identify where each operation
currently lives.

### Steps

1. Start at the scene render entry point.
2. Record the order of all passes.
3. For each pass, record:
   - function or method name;
   - shader program(s);
   - framebuffer or target;
   - input textures and buffers;
   - output attachments;
   - depth and blending behavior;
   - important state changes;
   - scene or asset dependencies.
4. Record operations that occur before or after the pass sequence, including
   UBO updates, SFML window operations, HUD rendering, and state resets.
5. Mark passes that are debug-only, optional, or conditional.
6. Mark operations that depend on global OpenGL state established by an earlier
   operation.

### Suggested inventory format

| Order | Pass | Entry point | Target | Inputs | Outputs | State/dependencies |
|---:|---|---|---|---|---|---|
| 1 | Terrain |  |  |  |  |  |
| 2 | Orb creatures |  |  |  |  |  |
| 3 | Shadows |  |  |  |  |  |
| 4 | SSAO |  |  |  |  |  |
| 5 | SSAO blur |  |  |  |  |  |
| 6 | Sky |  |  |  |  |  |
| 7 | Deferred lighting |  |  |  |  |  |
| 8 | Ocean |  |  |  |  |  |
| 9 | HUD/UI |  |  |  |  |  |

### Exit criteria

- The complete frame sequence is documented.
- Every pass has an identifiable owner and target.
- Hidden or accidental state dependencies are called out for Phase 1.

## Workstream 0.5: Inventory GPU resources and ownership

### Objective

Build a resource-level map that will guide extraction without changing
ownership yet.

### Steps

1. List every OpenGL handle owned by the interactive scene.
2. Group resources by role:
   - shader programs;
   - vertex arrays;
   - vertex and index buffers;
   - uniform buffers;
   - shader storage buffers;
   - textures and texture arrays;
   - framebuffers;
   - render targets;
   - samplers or sampler state;
   - OpenGL state values that are not represented by handles.
3. For each resource, record:
   - declaration;
   - creation function;
   - first-use point;
   - update points;
   - bind points;
   - destruction function;
   - dependent resources;
   - whether it is static, per-scene, per-frame, or dynamic.
4. Identify resources currently owned by `Assets`, `TerrainStreamer`, SSBO
   wrappers, SFML, or the scene.
5. Identify resources whose lifetime crosses scene transitions.

### Suggested inventory format

| Resource | Current owner | Created by | Used by | Updated | Destroyed by | Lifetime |
|---|---|---|---|---|---|---|
| G-buffer |  |  |  |  |  |  |
| Shadow map |  |  |  |  |  |  |
| SSAO targets |  |  |  |  |  |  |
| Terrain mesh |  |  |  |  |  |  |
| Orb SSBO |  |  |  |  |  |  |

### Exit criteria

- Every major GPU resource has a known creation and destruction path.
- Resources that should move together in Phase 1 are grouped.
- Cross-scene and asset-manager lifetimes are explicitly identified.

## Workstream 0.6: Inventory shader and data interfaces

### Objective

Record the CPU-to-shader contracts before any backend-independent data structures
are introduced.

### Steps

1. Locate all shader source files used by the scene.
2. Map each program to its vertex, fragment, geometry, compute, or other stages.
3. Record vertex attributes and their locations.
4. Record uniform names, types, and update sites.
5. Record uniform-block names, binding points, member order, and expected
   alignment.
6. Record SSBO names, binding points, element layouts, and active counts.
7. Record texture units, sampler types, array dimensions, formats, and
   filtering/wrapping behavior.
8. Record framebuffer attachment formats and intended value ranges.
9. Record coordinate systems, matrix conventions, units, and handedness.
10. Mark any interface that currently depends on hard-coded locations or
    implicit state.

### Suggested inventory format

| Program/pass | Stage | Interface | Binding/location | CPU source | Notes |
|---|---|---|---:|---|---|
| Terrain | vertex | attribute/uniform/block |  |  |  |
| Terrain | fragment | sampler/SSBO |  |  |  |
| Lighting | fragment | G-buffer sampler |  |  |  |

### Exit criteria

- The important CPU-to-shader contracts are documented.
- Layout-sensitive data has explicit size and alignment notes.
- The inventory identifies which interfaces can become shared render data and
  which must remain backend-specific.

## Workstream 0.7: Record window, SFML, UI, and lifecycle integration

### Objective

Identify application-level assumptions that may complicate Vulkan presentation
and UI integration.

### Steps

1. Locate window creation and OpenGL-context setup.
2. Record where the active context is selected or changed.
3. Record SFML render-texture creation and use.
4. Record calls that draw through SFML after raw OpenGL rendering.
5. Record calls such as `resetGLStates`, `setActive`, and window clears.
6. Locate ImGui-SFML initialization, update, rendering, and shutdown.
7. Record scene enter, exit, end, resize, focus, and shutdown behavior.
8. Identify assumptions that require a live OpenGL context.
9. Identify which UI components must be ported, replaced, or temporarily
   isolated before Vulkan can become the active backend.

### Exit criteria

- Window and UI integration points are documented.
- The Vulkan foundation has a concrete list of application integrations to
  preserve or replace.
- No presentation or UI dependency is left as an unrecorded assumption.

## Workstream 0.8: Capture visual and functional references

### Objective

Create repeatable references for detecting behavior changes after extraction
and porting.

### Steps

1. Choose a fixed level, camera position, window size, and render settings.
2. Capture the initial scene with:
   - terrain;
   - sky and atmosphere;
   - ocean;
   - orb creatures;
   - shadows;
   - SSAO;
   - minimap;
   - HUD.
3. Capture debug views, including SSAO output if available.
4. Exercise a small set of dynamic behaviors:
   - camera movement;
   - terrain streaming;
   - orb movement or animation;
   - day/night or astronomy updates;
   - window resize;
   - scene exit and re-entry.
5. Record settings and conditions for every capture.
6. Note known artifacts separately from expected output.

### Suggested reference case format

| Case | Setup | Action | Expected result | Known artifacts |
|---|---|---|---|---|
| Static world |  |  |  |  |
| Terrain movement |  |  |  |  |
| Resize |  |  |  |  |
| Scene re-entry |  |  |  |  |

### Exit criteria

- A developer can reproduce the selected reference views.
- Each reference identifies the behavior that later stages must preserve.

## Workstream 0.9: Record known defects and constraints

### Objective

Prevent pre-existing issues from being mistaken for migration regressions.

### Steps

1. Record visible rendering defects observed during baseline capture.
2. Record build warnings, validation warnings, and runtime errors.
3. Record platform-specific differences.
4. Classify each item as:
   - baseline defect;
   - migration blocker;
   - migration risk;
   - out of scope.
5. Do not fix an item unless it prevents completion of a Phase 0 exit
   criterion.
6. For every item deferred, record the phase where it should be revisited.

### Suggested issue format

| ID | Observation | Classification | Evidence | Revisit phase |
|---|---|---|---|---|
| P0-001 |  |  |  |  |

### Exit criteria

- Baseline defects and migration blockers are distinguishable.
- Deferred fixes have an owner phase or an explicit reason for exclusion.

## Final Phase 0 review

Before declaring Phase 0 complete, review the evidence as a single package.

### Review questions

- Can another developer build and launch the baseline?
- Is the exact source revision recorded?
- Is the render-pass order complete?
- Does every major GPU resource have a known lifetime?
- Are shader bindings and layouts documented?
- Are SFML, ImGui, window, resize, and lifecycle dependencies recorded?
- Can the visual references be reproduced?
- Are known defects separated from migration regressions?
- Does the Phase 1 extraction boundary follow the evidence?

### Phase 0 exit criteria

Phase 0 is complete when:

- the baseline build and runtime behavior are reproducible or precisely
  blocked;
- the render-pass and resource inventories are complete enough to plan
  extraction;
- shader and CPU-to-GPU interfaces are documented;
- window and UI integration constraints are documented;
- visual and functional reference cases exist;
- known defects and platform limitations are recorded;
- a Phase 1 handoff checklist identifies the first safe extraction boundary.

## Phase 1 handoff

The Phase 0 handoff should recommend a small first extraction rather than
moving the entire renderer at once. The recommended starting boundary is:

1. scene-owned mesh resources and their creation/destruction;
2. G-buffer and SSAO resource ownership;
3. the render-pass coordinator;
4. one pass at a time behind an OpenGL renderer object.

Gameplay updates, ECS ownership, terrain queries, astronomy, and input should
remain in `Scene_IC_Camp` until a later render-data milestone proves that they
can be represented without exposing the entity manager to the renderer.

The handoff should also list the checks to repeat immediately after the first
extraction:

- build and startup;
- scene entry and exit;
- static visual reference cases;
- window resize;
- repeated resource initialization and cleanup;
- OpenGL error and framebuffer completeness checks;
- terrain, orb, shadow, SSAO, ocean, sky, minimap, and HUD behavior.

