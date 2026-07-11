# Terrain System Architecture Overhaul Roadmap

This document outlines the multi-stage rollout plan to transition the engine's terrain system from a monolithic, CPU-unpacked RGB PNG asset to a highly performant, streaming 7x7 toroidal float-buffer on the CPU mapped dynamically to a 5x5 `sampler2DArray` viewport on the GPU.

## Target Architecture Specifications
* **Base Core Tile Resolution:** 256 x 256 texels
* **Memory Allocation (with Apron):** 257 x 257 floats per tile
* **World Space Resolution:** 1 texel = ~3.1 to 4 meters
* **CPU Memory Footprint:** 7x7 Grid (49 Tiles) with integrated 256 x 256 Spatial Partitioning Grid
* **GPU Memory Footprint:** 5x5 Grid (25 Active Array Slices, size 256 x 256)

## First Version of the Tileset Description Schema
```json
{
  "schema_version": 1,
  "name": "Avalon",
  "world_origin_latlon": [47.0, -53.0],
  "world_size_m": [111320.0, 111320.0],
  "tile_resolution": 256,
  "apron_texels": 1,
  "meters_per_texel": 3.4,
  "grid_dimensions": [7, 7],
  "tile_directory": "tiles/Avalon/",
  "tile_naming_pattern": "tile_{row}_{col}.bin",
  "channels": ["height"]
}
```
The intention is to have the toolchain take as few of these properties as possible from the 
---

## Rollout Pipeline

### Stage 1: Documentation & Planning
* [x] Draft and finalize the multi-stage rollout roadmap.
* [x] Commit roadmap to repository tracking.

### Stage 2: Toolchain Data Extraction
* **Goal:** Write a Python script to slice raw Cloud Optimized GeoTIFF (COG) data directly into raw floating-point chunks.
* [ ] Parse COG/TIFF metadata to map absolute physical world heights to floating-point binaries.
* [ ] Implement export of a single tile as a flat binary file of continuous `float32` variables (256 x 256).
* [ ] Include edge-duplication logic to bake the 1-texel **apron** border directly into the exported binary, resulting in a physical 257 x 257 float array per file.

### Stage 2a: Legacy Support Generator
* **Goal:** Create a bridge to keep current engine systems functioning during the transition.
* [ ] Modify existing asset pipeline script to output tiles in the current legacy packed RGB PNG format, but clamped to the new 256 x 256 dimensions.

### Stage 3: Data Verification & Test Suite
* **Goal:** Ensure mathematical parity between the source GIS file and exported assets before modifying engine code.
* [ ] Write a verification test script (Python/C++).
* [ ] Sample random coordinates across the original COG TIFF.
* [ ] Compare the source values against the newly minted raw float binary data and the legacy packed PNG tiles to guarantee 100% precision accuracy.

### Stage 4: Engine Integration (Legacy Format, New Dimensions)
* **Goal:** Isolate engine coordinate math adjustments from data format changes.
* [ ] Implement a JSON manifest schema to be exported by the software tool defining key details about a tile set.
* [ ] Update game engine to handle the new 256 x 256 base grid bounds instead of 300 x 300.
* [ ] Package a monolithic 5 x 5 set of tiles into a single legacy PNG sheet (1280 x 1280 pixels) and ensure vertex displacement and grounding functions still behave perfectly.

### Stage 5: Native Float Buffer Engine Integration
* **Goal:** Cut ties with the PNG format on the CPU.
* [ ] Strip out the RGB byte-unpacking ALU operations.
* [ ] Update engine to read the raw float binary files directly into a contiguous `float*` or `std::vector<float>` heap allocation.
* [ ] Benchmark CPU cache performance improvements on height-lookup queries.

### Stage 6: Fixed Multi-Tile Grid & Array Shaders
* **Goal:** Transition from a monolithic mega-texture to modular slots.
* [ ] Implement the `sampler2DArray` vertex shader logic on the GPU.
* [ ] Set up the static 7 x 7 array pointer layout on the CPU.
* [ ] Implement the CPU-to-GPU Uniform Translation Map (`u_ActiveTileSlices[25]`) to feed the shader viewport.

### Stage 7.1: Synchronous Toroidal Indexing
* **Goal:** Build the ring-buffer modulo mapping
* [ ] Determine which of the 49 slots currently represents which world-tile coordinate.
* [ ] On boundary crossing, block and load new tile directly on main thread.

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