# TerrainStreamer

`TerrainStreamer` keeps a window of terrain tiles loaded around the camera
and uploaded to the GPU, streaming new tiles in from disk on a background
thread as the camera moves, without ever stalling the render loop.

## What problem it solves

A terrain dataset is too large to keep entirely in memory or on the GPU, so
it's split into tiles on disk (a heightfield file and, optionally, a road
signed-distance-field file per tile, named `tile_<row>_<col>.bin` /
`road_<row>_<col>.bin`). At any given moment, only the tiles near the camera
matter for rendering. `TerrainStreamer`'s job is to:

1. Keep a fixed-size grid of tiles resident in CPU memory, centered on the
   camera.
2. Pack that grid into GPU array textures for the terrain shader to sample.
3. Reload tiles from disk as the camera crosses tile boundaries, without
   introducing a hitch in the frame rate.

## Core concepts

### The tile grid and slots

The streamer keeps a square grid of "slots" (`kStreamerGridDim × kStreamerGridDim`,
larger than the visible window so nearby-but-not-yet-visible tiles can be
prefetched). Each slot is a fixed-size chunk of a shared `m_tileStorage` /
`m_roadStorage` buffer that currently holds the data for one tile coordinate.
`WorldCoordinates::Square::slotIndexForTile()` maps an absolute tile
coordinate to a slot index (a spatial hash, effectively a scrolling ring
buffer), so as the camera moves, slots are recycled in place rather than
reallocated.

A slot is only trusted as valid when both:
- `m_slotValid[slot]` is `true`, and
- `m_slotWorldCoord[slot]` matches the tile coordinate being asked for.

That second check is what keeps things correct while a reload is in flight:
a slot can physically still contain an old tile's bytes while conceptually
belonging to a new coordinate, and the coordinate check prevents that stale
data from ever being read as valid.

### The visible subgrid vs. the full slot grid

`kVisibleGridDim` (a smaller window than `kStreamerGridDim`) defines what
actually gets packed into the GPU textures and rendered each frame.
`getActiveSubgrid()` walks that visible window, looks up each tile's slot,
and returns a snapshot (`ActiveSubgrid`) describing which tiles are ready
and which aren't yet. `getOrUploadArrayTexture()` /
`getOrUploadRoadArrayTexture()` consume that snapshot to fill a
GL_TEXTURE_2D_ARRAY, substituting a flat/zero default tile for any slice
that isn't valid yet.

## Threading model

Each `TerrainStreamer` owns exactly one background worker thread
(`workerThreadMain`). The division of responsibilities is deliberately
simple:

- **The worker thread** only reads tile files from disk into buffers it
  owns locally, and hands them off through a queue. It never touches the
  CPU tile cache, the slot-validity arrays, or any OpenGL object.
- **The calling thread** (the render/main thread) owns all the mutable
  cache state and every GL call. It is the only thread that ever writes
  `m_tileStorage`, `m_roadStorage`, `m_slotValid`, or `m_slotWorldCoord`.

Because of that split, the two threads only need to coordinate through two
mutex-guarded queues:

- `m_requestQueue` — tile coordinates the main thread wants loaded, pushed
  by `requestTileLoad()` and consumed by the worker.
- `m_completionQueue` — finished loads (with their own freshly-read data),
  pushed by the worker and consumed by the main thread in
  `drainCompletionQueue()`.

This means all shared mutable state is confined to a single writer, which
avoids the need to lock the cache itself and keeps the worker's job as
simple as "read a file, hand over the bytes."

## Per-frame flow

Every frame, the game calls `update(cameraWorldPos)`, which does two things:

1. **`drainCompletionQueue()`** — non-blocking. Pulls any tile loads the
   worker has finished since the last frame, copies their data into the
   right cache slot, and marks that slot valid. This is cheap and never
   waits on anything.
2. **`checkBoundaryCrossing()`** — also non-blocking. Converts the camera
   position to a tile coordinate; if it's the same tile as last frame,
   nothing happens. If the camera has moved into a new tile, the visible
   window shifts, any slot that now falls outside — or newly inside — the
   window is invalidated and an asynchronous reload is requested for it.
   The frame then continues immediately; slots waiting on a reload simply
   render as their default (flat height, no road) until the data arrives.

The one exception is the very first call: since there's no data loaded yet,
`initializeGrid()` requests every tile in the initial window and blocks
until the worker has delivered all of them. That's an intentional one-time
cost (a loading pause is expected on startup) rather than something that
happens on every boundary crossing.

## Why this avoids the original stutter

The bug being fixed was the main thread blocking on the worker every time
the camera crossed a tile boundary, causing a visible hitch. The fix
separates "ask for a reload" from "wait for a reload": `checkBoundaryCrossing`
only ever enqueues work and returns, and `drainCompletionQueue` only ever
takes what's already finished. The render loop is never blocked waiting on
disk I/O after the initial load — it just renders slightly incomplete data
(a flat default) for a tile or two until the corresponding load finishes on
a later frame.

## Practical notes / gotchas

- **All public methods must be called from one consistent thread.**
  `update()`, the getters, and the texture-upload calls all assume they're
  invoked from the same thread (typically the render thread). The class
  does not defend against being called concurrently from multiple threads.
- **Fast camera movement can trigger redundant reloads.** If the camera
  crosses several tile boundaries before a slot's previous reload finishes,
  a newer request for the same slot can be queued behind an older one.
  Because there's a single worker thread, requests and results stay in
  order, so the final state is always correct — but some disk reads become
  wasted work, and a slot can briefly flicker invalid an extra frame as a
  stale (now-superseded) result is applied and then immediately
  overwritten.
- **Missing tile files are not an error.** If a `tile_<row>_<col>.bin` or
  `road_<row>_<col>.bin` file doesn't exist on disk, `loadTileFromDisk`
  fills in a default (flat height, no road) instead of failing, which is
  useful for sparse or partially-generated datasets.