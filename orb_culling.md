# Orb Culling Roadmap

Goal: Implement efficient GPU frustum culling for the 128k+ orb creatures using compute shaders, then switch to indirect drawing.

## Phase 0: Preparation & Baseline Measurement (1-2 hours)

1. Add debug counters
   - Track how many orbs are currently being drawn each frame.
   - Add a simple ImGui display: "Orbs Drawn: X / Total".

2. Create a "naive culling" mode on CPU first
   - Simple sphere-frustum test on CPU for a subset (e.g. 10k orbs).
   - Verify you can reduce draw calls correctly and FPS improves.

3. Verify your camera UBO exposes everything needed:
   - `view`, `proj`, `viewProj`, `cameraPos`, `cameraForward`, etc.

**Test:** Confirm you can toggle CPU culling and see both correct visual results and performance improvement.

## Phase 1: Basic Compute Shader Culling (2-4 hours)

1. Write `cull_orbs.comp`
   - Read `OrbBuffer`
   - Frustum plane test (6 planes)
   - Atomic counter + write visible indices to `CulledIndexBuffer`

2. CPU setup
   - Create SSBOs for culled indices + atomic counter
   - Dispatch compute before orb rendering
   - Add `GL_SHADER_STORAGE_BARRIER_BIT`

3. Intermediate test (very important)
   - Still draw **all** orbs, but color them differently if they appear in the culled list.
   - Or render a small debug point cloud of culled centers.

**Success criteria:** You should see roughly the same number of orbs "passing" culling as expected from camera view, and FPS should not regress.

## Phase 2: Indirect Drawing (3-5 hours)

1. Set up indirect command buffer
   - `DrawElementsIndirectCommand` struct (count, instanceCount, etc.)

2. Modify orb rendering path
   - Bind culled index buffer as index source (or use vertex pulling)
   - Use `glDrawElementsIndirect` with the atomic counter as count

3. Safety fallback
   - Keep a toggle to disable culling and fall back to full draw

**Test:** Fly camera around. Orbs should disappear correctly when leaving view frustum. Big FPS gain when looking at empty sky.

## Phase 3: Polish & Optimizations

1. Distance-based LOD binning in the same compute pass
   - Multiple output buffers: close, medium, far
   - Different mesh resolutions per bin

2. Temporal coherence / Coherence buffer
   - Keep last frame’s visible list and only re-test edge cases

3. Occlusion culling (advanced)
   - Hierarchical Z or simple occlusion queries

4. Performance instrumentation
   - Timer queries around compute pass + draw calls

## Phase 4: Validation & Edge Cases

- Camera edge cases (looking straight up/down)
- Very close orbs
- Rapid camera movement
- Extreme orb counts (256k+)
- Verify shadows still work on culled orbs (they should — shadows are separate)

## Recommended Order of Implementation

1. Phase 0 (baseline)
2. Phase 1 with visual debug (color coding)
3. Phase 2 (indirect draw)
4. Phase 3 (LOD + polish)

## Files You Will Touch

- `cull_orbs.comp` (new)
- `Scene_IC_Camp.cpp/h` (rendering path + SSBO management)
- `OrbSSBO.h` or new `CulledOrbSSBO.h`
- Possibly `renderer/` folder for helper classes

---