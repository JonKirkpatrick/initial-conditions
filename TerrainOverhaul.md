# Terrain System Architecture Overhaul Roadmap

This document outlines the multi-stage rollout plan to transition the engine's terrain system from a monolithic, CPU-unpacked RGB PNG asset to a highly performant, streaming 7x7 toroidal float-buffer on the CPU mapped dynamically to a 5x5 `sampler2DArray` viewport on the GPU.

## Target Architecture Specifications
* **Base Core Tile Resolution:** 256 x 256 texels
* **Memory Allocation (with Apron):** 257 x 257 floats per tile
* **World Space Resolution:** 1 texel = 4 meters
* **CPU Memory Footprint:** 7x7 Grid (49 Tiles) with integrated 256 x 256 Spatial Partitioning Grid
* **GPU Memory Footprint:** 5x5 Grid (25 Active Array Slices, size 256 x 256)

## Second Version of the Tileset Description Schema

```json
{
  "schemaVersion": 2,
  "name": "StJohns",
  "worldOriginLatLon": [48.00013888888889, -53.00013888888889],
  "tileBoundsUpperLeft": { "row": 40, "col": 10 },
  "tileBoundsLowerRight": { "row": 50, "col": 20 },
  "tileDirectory": "tiles/StJohns/",
  "channels": ["height"]
}
```
The description above describes an 11x11 patch of tiles containing much of the St. John's, NL area.  The worldOriginLatLon is taken directly from the Python tooling we built to extract these tiles from Copernicus COG DEM GeoTiff files.  It is the latitude and longitude of the northwest corner of the actual GeoTiff file, not the specific offset of the tile bounds included in the patch described by this manifest.  Each GeoTiff contains exactly one degree of latitude and longitude, representing one arcsecond square per pixel.  This results in a source file of 3600x3600 pixels, which we interpolate to give us tiles of (256+1)x(256+1) pixel tiles, where each pixel represents a 4 metre square of terrain.  In the tiles, the additional row and column is just the first row and column of the adjacent tile, serving as an apron to be sampled in game instead of ever needing to read from two files for a single height lookup which uses a 2x2 neighbourhood to bilinearly interpolate over.

We have included channels as a single element list in Schema Version 2, where we are only using height as our data.  We will likely be using parallel assets or a revised binary format to pack additional channels in the future, such as what is actually on the terrain, or other data about the terrain needed for other systems.  We may end up mapping out water here, or forests, or even roads.  It may even serve to provide a simple way to register spawn points or patrol paths when we build out a level editor.

The whole idea of tiling and streaming our terrain in this manner is to support larger levels in the future, that would otherwise just eat into system memory.

---

## Rollout Pipeline

### Stage 1: Documentation & Planning
* [x] Draft and finalize the multi-stage rollout roadmap.
* [x] Commit roadmap to repository tracking.

### Stage 2: Toolchain Data Extraction
* **Goal:** Write a Python script to slice raw Cloud Optimized GeoTIFF (COG) data directly into raw floating-point chunks.
* [x] Parse COG/TIFF metadata to map absolute physical world heights to floating-point binaries.
* [x] Implement export of a single tile as a flat binary file of continuous `float32` variables (256 x 256).
* [x] Include edge-duplication logic to bake the 1-texel **apron** border directly into the exported binary, resulting in a physical 257 x 257 float array per file.

### Stage 2a: Legacy Support Generator
* **Goal:** Create a bridge to keep current engine systems functioning during the transition.
* [x] Modify existing asset pipeline script to output tiles in the current legacy packed RGB PNG format, but clamped to the new 256 x 256 dimensions.

### Stage 3: Data Verification & Test Suite
* **Goal:** Ensure mathematical parity between the source GIS file and exported assets before modifying engine code.
* [x] Write a verification test script (Python/C++).
* [x] Sample random coordinates across the original COG TIFF.
* [x] Compare the source values against the newly minted raw float binary data and the legacy packed PNG tiles to guarantee 100% precision accuracy.

### Stage 4: Engine Integration (Legacy Format, New Dimensions)
* **Goal:** Isolate engine coordinate math adjustments from data format changes.
* [x] Implement a JSON manifest schema to be exported by the software tool defining key details about a tile set.
* [x] Update game engine to handle the new 256 x 256 base grid bounds instead of 300 x 300.
* [x] Package a monolithic 5 x 5 set of tiles into a single legacy PNG sheet (1280 x 1280 pixels) and ensure vertex displacement and grounding functions still behave perfectly.

### Stage 5: Native Float Buffer Engine Integration
* **Goal:** Cut ties with the PNG format on the CPU.
* [x] Strip out the RGB byte-unpacking ALU operations.
* [x] Update engine to read the raw float binary files directly into a contiguous `float*` or `std::vector<float>` heap allocation.
* [x] Benchmark CPU cache performance improvements on height-lookup queries.

### Stage 6: Fixed Multi-Tile Grid & Array Shaders
* **Goal:** Transition from a monolithic mega-texture to modular slots.
* [ ] Implement the `sampler2DArray` vertex shader logic on the GPU.
* [x] Set up the static 7 x 7 array pointer layout on the CPU.
* [ ] Implement the CPU-to-GPU Uniform Translation Map (`u_ActiveTileSlices[25]`) to feed the shader viewport.

### Stage 7.1: Synchronous Toroidal Indexing
* **Goal:** Build the ring-buffer modulo mapping
* [x] Determine which of the 49 slots currently represents which world-tile coordinate.
* [x] On boundary crossing, block and load new tile directly on main thread.

### Stage 7.2:  Extract "load tile from disc" As A Pure Function
* **Goal:** Input: tile coordinate.  Output:  a filled 257x257 float buffer.
* [ ] Ensure it doesn't touch any shared engine state.  Just file I/O in, and buffer out.
* [ ] Gracefully handle missing data.

### Stage 7.3:  Add A Staging Buffer
* **Goal:** Separate the live slot from access contention.
* [ ] Load the new tile into a scratch buffer
* [ ] Publish as a distinct step
* [ ] Test by crossing boundary threshholds repeatedly.  This should still cause a stutter as we are still blocking on the main thread.

### Stage 7.4:  Move The Load Call Onto One Worker
* **Goal:** Main thread pushes request to queue a tile load.  Worker pushes a completion status to a second queue
* [ ] Request queue.  Load tile X into staging slot Y.
* [ ] Completion queue.  Staging slot Y is ready.
* [ ] Main thread pops it once per frame and does the actual copy/publish
* [ ] Each queue will need a mutex around the push and pop, but in practice there shouldn't actually be any contention.

### Stage 7.5:  Profile Reactive Tile Loading
* **Goal:** At this stage, the loading is being done on a worker thread and we should no longer have any stutter on boundary crossing.
* [ ] Rapidly cross boundaries as in 7.3, empirically judge fluidity.
* [ ] Wrap the buffer swap in a timer to determine actual latency.
* [ ] Decide if the buffer swapping is performant enough under reactive tile loading.