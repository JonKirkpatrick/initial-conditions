## Baseline Details

| Detail              | Value      |
|---------------------|------------|
| Current Branch      | Main       |
| Commit Hash         | e47f6ds6   |
| Working Tree Status | up to date |

## Local only Prerequisites

| Category |	Current project example	| Why it matters |
| ---- | ---- | ---- |
| Compiler/toolchain | A C++ compiler supporting C++23	| The Makefile uses -std=c++23. |
| Build tool	| GNU Make |	The current build path is Make-based. |
| SFML	| SFML 3.x development headers and libraries |	The project includes SFML graphics, window, system, and audio modules. |
| GLEW	| GLEW headers and library	| The project calls glewInit() and uses GLEW OpenGL entry points. |
| OpenGL  development support |	Linux OpenGL libraries or macOS OpenGL framework |	The Makefile links -lGL on Linux and -framework OpenGL on macOS. |
| OpenMP |	OpenMP compiler/runtime support	| The build uses -fopenmp; macOS additionally references Homebrew libomp. |
Graphics hardware/driver |	A GPU and driver exposing the required OpenGL features |	The runtime reports the active OpenGL and GLSL versions. |
Display environment	| A working graphical desktop/display session |	The game creates a fullscreen window and cannot be meaningfully baseline-tested headlessly. |
Audio device |	An available audio backend/device |	The game loads music and sound effects through SFML audio. |
Working directory |	Running from the repository’s bin/ directory, or otherwise preserving asset paths |	The executable is launched from bin, while assets are referenced using relative paths. |

## Current Known Limitations

The program will compile under macOS and allow you to launch to the main menu, however the extensive use of OpenGL version 460 features causes it to crash immediately upon entering Scene_IC_Camp.  This is precisely why we are transitioning to Vulkan and ultimately MoltenVK for Mac.

## Compiling and Running

### Commands available via the Makefile
- make clean
- make all
- make run

### Once built
Navigate to the bin directory \
./sfmlgame

### Toolchain Versions
| Tool | Version |
| --- | --- |
| g++ | 15.2.0 |
| gcc | 15.2.0 |
| make | 4.4.1 |
| cmake | 4.2.3 |
| ninja | 1.13.2 |
| doxygen | 1.15.0 |
| dot | 14.1.2 |
| node | 22.23.3 |
| npm | 10.9.9 |
| git | 2.53.0 |

### Environment Table

| Category | Baseline Value |
| --- | --- |
| OS and Architecture | Ubuntu 26.04 LTS, X86-64 |
| Compiler | g++ 15.2.0 |
| C++ Standard | -std=c++23 |
| SFML | 3.0.2 |
| GLEW | 2.3.1 |
| GPU and Driver | NVIDIA GeForce RTX 2070 SUPER, 615.71.09 |
| OpenGL Version | 4.6.0 NVIDIA 615.71.09 |
| GLSL Version | 4.60 |
| Window Dimensions | 2560x1440 |

### Render-pass inventory

I filled in the table from the code you provided. The shader sources weren't included, so the attachments each shader writes and its output semantics are inferred from the uniforms, bindings, and names. Verify those against the GLSL files before you rely on them.

## Frame pass inventory

| Order | Pass | Entry point | Target | Inputs | Outputs | State / dependencies |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Terrain (depth pre-pass, then shading) | `runTerrainPass()` | G-buffer FBO: GB0 albedo RGBA8, GB1 normal RGBA16F, GB2 index RGBA8, GB3 retro RGBA8, depth D32F | Height array (unit 0), diffuse and normal splat arrays (units 1–2), material SSBO (binding 1), `m_gridVAO`, slice-valid uniforms, grid origin, hovered hex (CPU raymarch in `screenToWorld` when `m_cursorMode`) | G-buffer colour 0–3 and depth | Pass A: colour mask off, depth write on, LEQUAL. Pass B: colour mask on, depth write off, LEQUAL. Restores depth write on. Rebinds SSBO binding 1. Needs `buildTerrainGrid()` run, G-buffer cleared, and slice uniforms current. |
| 2 | Orb creatures | `renderOrbCreature()` | G-buffer | Orb SSBO (binding 0, packed in `update()`), species SSBO (binding 1, overwrites terrain's binding), species diffuse and normal arrays (units 0–1), `m_cubeVAO` (lazy-built here) | G-buffer colour and depth | Depth test against terrain depth, depth write on, no blend. Depends on terrain depth, `m_cubeVAO` existing, and the orb SSBO being current. |
| 3 | Shadows (CSM, 5 cascades) | `runShadowPass()` | `m_shadowFBO`, 2048² `GL_TEXTURE_2D_ARRAY` depth, one layer per cascade | Camera near and far (far clamped to `m_shadowMaxDistance`), sun direction, height array and slice uniforms (unit 0), `m_gridVAO`, orb SSBO (binding 0), `m_cubeVAO` | Shadow depth array; writes `m_cascadeSplits`, `m_lightViewProjCascades`, `m_lightDepthRange`, `m_texelWorldSize` | Sets viewport to 2048² and never restores it. Cull enabled and toggled. Terrain draws with culling off; orbs draw with front-face culling; cascade 4 skips orbs. Depends on `m_cubeVAO` (set up in pass 2) and the camera updated this frame. |
| 4 | SSAO | `runSSAOPass()` | `m_ssaoPipeline.fbo`, `colorTex` (GL_RED, float) | G-buffer normal (unit 0), G-buffer depth (unit 1), 4×4 noise (unit 2), kernel, noise scale, radius, bias, sample count, view rays from `uploadViewRays()`, `m_gridVAO` | `colorTex` | Clears colour only. Disables depth test and does not restore it. Blend inherited off. Depends on passes 1–2 and the FOV units (see below). |
| 5 | SSAO blur | `runSSAOBlurPass()` | `blurFBO` → `blurTex` | `colorTex` (0), normal (1), depth (2), `m_gridVAO` | `blurTex` | Depth test still off (inherited), blend off. Depends on pass 4. |
| 6 | Sky | `renderSky()` | `m_skyTexture` (`sf::RenderTexture`) | NightSky cubemap (unit 0, procedural fallback if missing), moon texture (unit 1, via `sf::Texture::bind`), star rotation matrix, `m_gridVAO` | `m_skyTexture` (`display()`, smoothing on) | Viewport set to sky texture size. Depth test off and blend off (inherited). `setActive(true/false)` switches the context. In `sRender`, the window is then cleared transparent and the sky is drawn as an `sf::Sprite`. Depends on astro state and the cubemap and moon assets being loaded. |
| 7 | Deferred lighting | `deferredLighting()` (or `blitToScreen(blurTex)` when `m_debugShowSSAOBlur`) | Default framebuffer (window back buffer) | G-buffer 0–4, shadow array (5), SSAO blur (6), cascade matrices, texel sizes, split depths, species SSBO (binding 5), material SSBO (binding 6), headlight flags and parameters, night ambient floor, `m_lightingVAO` (lazy-built here), UBOs 0–2 | Back buffer, blended over the sky sprite | Viewport set to window size. Depth test off. Blend `SRC_ALPHA, ONE_MINUS_SRC_ALPHA`. Units 0–6 unbound at end. Depends on the sky sprite having cleared and drawn the window, the shadow pass having run this frame, and G-buffer size matching the window. |
| 8 | Ocean | `renderOceanGrid()` | Default framebuffer (inherited from pass 7) | G-buffer depth (unit 0), shadow array (1), cubemap (2), moon (3), cascade data, star rotation, night ambient floor, time, camera position for grid snapping, `m_oceanVAO`/`m_oceanEBO` | Back buffer | Depth test off, depth write off, blend on. Culling disabled temporarily and restored only if it was on. Polygon mode saved and restored. Depth test, depth write, and blend are re-enabled or disabled at the end. Occlusion depends on shader depth sampling (see below). |
| 9 | HUD / UI | `window.resetGLStates()` then `m_hud->render(window, false)` in `sRender()` | Default framebuffer | `HUD_Data`, minimap texture (produced in `update()`, see below) | Back buffer | `resetGLStates()` clears SFML's cached GL state. Depends on the minimap texture being current. |

## Operations outside the pass sequence

| When | Operation | Where | Notes |
| --- | --- | --- | --- |
| `update()`, before render | Terrain streaming, movement, gait and audio, `updateCamera`, `updateAstronomySystem`, `updateOrbShaderStorage` (OpenMP pack and SSBO upload), `m_entityManager.update()` | `update()` | CPU work. Orb SSBO upload happens here, not in the render pass. |
| `update()` → `updateHUDData()` | **Minimap render pass.** Renders the terrain into `m_minimapTexture` via `m_minimapProgram` with `pushGLStates`/`popGLStates`, sets viewport to 256² without restoring it, then draws the home marker with SFML. | `updateMinimapTexture()` | This is a GL pass that runs before `sRender()`. It needs its own row in the inventory. |
| `update()`, if `m_showGUI` | ImGui widgets built | `sGUI()` | ImGui draw call is not in this scene code. |
| `sRender()` start | Upload Atmosphere, Environment, and Camera UBOs (binding points 2, 1, 0). Bind G-buffer FBO, set viewport, clear colour and depth, depth on with LEQUAL, depth write on, blend off. | `sRender()` | Fog constants are hard-coded here. |
| Not in scene code | Window display and swap, ImGui render | Presumably `GameEngine` | Not visible in what you provided. |

## Conditional and debug markers

- **Always runs:** Terrain, orbs, shadows, SSAO, blur, sky, lighting, ocean, HUD.
- **Conditional:** `m_debugShowSSAOBlur` replaces lighting with a blit of the blurred SSAO.
- **Conditional:** `m_cursorMode` enables the hovered-hex uniform and the CPU raymarch in the terrain pass.
- **Conditional:** `m_headlightState` controls the headlamp uniforms in lighting.
- **Conditional:** The orb shadow draw is skipped for the last cascade.
- **Conditional:** Sky uses a procedural fallback when the cubemap is missing.
- **Conditional:** `m_showGUI` (ImGui build and draw).
- **Conditional:** The minimap texture is recreated if its size changes.
- **Dead or unwired:** `m_debugShowShadowMap`, `m_debugShowCascadeColors`, `m_debugDisableTexelSnap`, `m_drawGrid`, `m_drawTextures`, and `m_drawCollision` are set in the GUI but never read in the code you provided. Texel snapping is always on because `m_debugDisableTexelSnap` is never checked. If the shaders read these through uniforms that the C++ never sets, they are effectively dead anyway.

## Hidden dependencies for Phase 1

1. **Lazy VAO initialization order.** `m_cubeVAO` is built inside `renderOrbCreature()`, but `runShadowPass()` also uses it. If the shadow pass ran first, it would draw with VAO 0. Move this into `initGraphicsPipelines()`.
2. **`m_gridVAO` as a fullscreen-triangle VAO.** SSAO, blur, minimap, and sky all draw three vertices with `m_gridVAO`. The comment calls this a "zero-attribute VAO trick," but the VAO has attribute 0 enabled. This only works if the vertex shaders generate positions from `gl_VertexID` and ignore attribute input. Vulkan needs an explicit pipeline with no vertex input for these.
3. **Depth-test leak.** SSAO disables `GL_DEPTH_TEST` and never re-enables it. Blur and sky depend on it staying off, and ocean re-enables it at the end. Each pass's depth state is implicit.
4. **Viewport leaks.** Shadows leave the viewport at 2048², and minimap leaves it at 256². Later passes set their own viewport, but SFML's sprite and HUD draws rely on SFML reapplying its view.
5. **Cull state.** Shadows toggle `GL_CULL_FACE` and `glCullFace`. Ocean reads `glIsEnabled(GL_CULL_FACE)` to decide whether to restore it.
6. **SSBO binding conflict.** Binding 1 is the material SSBO for terrain and the species SSBO for orbs. Every pass has to rebind its own buffers.
7. **Texture-unit cleanup is inconsistent.** Ocean unbinds `GL_TEXTURE_CUBE_MAP` on unit 3, but unit 3 holds the moon texture bound through SFML. This is harmless now, but descriptor sets will replace it cleanly.
8. **Cascade data is a cross-pass dependency.** Shadows write the cascade matrices and split depths, and lighting and ocean read them. If the shadow pass is skipped, they use stale data.
9. **Sky composition goes through the back buffer.** The sky is drawn as a sprite into the window, then lighting blends over it with `SRC_ALPHA`. Sky visibility depends on the lighting shader's output alpha, which I can't see.
10. **Ocean has no depth test.** Occlusion by terrain and orbs has to come from shader sampling of `gDepthTex`. Confirm this in the ocean shader.
11. **CPU raymarch in a render pass.** `runTerrainPass()` calls `screenToWorld()` (up to 512 terrain samples) every frame in cursor mode. Its cost depends on the cursor mode.
12. **`getOrUploadArrayTexture()` is called from four places per frame** (shadows, terrain twice, minimap). Its "upload" name suggests side effects. Check whether it uploads on every call, and fetch it once per frame if so.
13. **Size coupling.** G-buffer, SSAO, and sky textures are allocated in the constructor and `initLevelState()`. `onEnter()` updates only `m_gBufferWidth`/`m_gBufferHeight` and the camera aspect, but the lighting and ocean viewports use the live window size. A window size that differs from the allocation size will break the output.
14. **Possible FOV unit bug.** `spawnCamera()` converts `FOVY` to radians before storing it. `uploadViewRays()` treats `camData.fovY` as degrees and multiplies by π/180 before taking the tangent. If `CCamera` stores what it's given, SSAO view rays are wrong. Check `Camera::getProjectionMatrix` to confirm the expected unit.

## Caveats

- The shader sources weren't in the supplied code, so the attachments each shader writes and its output alpha are inferred.
- `Camera::`, `Assets::`, `TerrainStreamer`, and `HUD` implementations aren't included, so some state claims (for example, what `getOrUploadArrayTexture()` does) need checking against those files.

Many of these items, such as implicit depth and cull state, the shared fullscreen VAO, lazy initialization, and SSBO binding reuse, are exactly what an explicit Vulkan pipeline forces you to resolve. Items 1–3 and 13–14 are the most likely to cause trouble during extraction.

### Build Result
There are no errors and all required build artifacts placed in the build directory, with the final binary placed in the bin directory.

### Startup Result
Game loads all assets, compiles all the OpenGL programs, and launches to the main menu.  From there, it allows access to the main scene along with the ability to cleanly shutdown.

### Scene-entry Result
All scene specific OpenGL state and context is constructed and the scene launches cleanly without any errors.

### Shutdown Result
Quiting the scene returns to the main menu, and quiting the main menu cleanly shuts down the program.

### Known limitations
- The OpenGL context is not completely deconstructed on exit to main menu and thus re-entering the scene results in a black screen.  The game doesn't freeze, but nothing is displayed.
- macOS only supports up to OpenGL version 410, so the game will launch to the main menu, but crash upon entering the main scene, where many features of version 460 are employed.
- On occassion, the whole system locks up and it becomes necessary to hard reboot.  This will require some investigation and I don't yet know the cause.