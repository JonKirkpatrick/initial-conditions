# High-Level Architecture: `WorldCoordinates` Namespace

The `WorldCoordinates` namespace serves as the engine's single source of truth for spatial invariants, unit transformations, and discrete spatial indexing. It defines physical scale metrics and translates continuous 2D world-space coordinates in meters $(x, z)$ into discrete indexing structures used by streaming terrain, spatial collision grids, and hexagonal system mechanics.

To support different layout paradigms, `WorldCoordinates` divides its functionality into two distinct sub-namespaces: `Square` (for streaming terrain and collision grids) and `Hex` (for axial hexagonal layouts).

---

## 1. Constants & Invariants

| Constant | Value | Sub-Namespace | Description |
| --- | --- | --- | --- |
| **`kTexelSizeM`** | $4.0\text{ m}$<br> | `Square`<br> | Physical side length of an individual texel / collision cell.

 |
| **`kTileResolution`** | $256\text{ texels}$<br> | `Square`<br> | Side resolution of a standard terrain tile ($1024\text{ m} \times 1024\text{ m}$).

 |
| **`kApronTexels`** | $1\text{ texel}$<br> | `Square`<br> | Border texel padding used for seamless bilinear sampling across boundaries.

 |
| **`kStreamerGridDim`** | $11 \times 11\text{ tiles}$<br> | `Square`<br> | Extents of the toroidal CPU memory streaming cache grid.

 |
| **`kVisibleGridDim`** | $9 \times 9\text{ tiles}$<br> | `Square`<br> | Extents of the active GPU-visible rendering slice.

 |
| **`kHexSize`** | $1.0\text{ m}$<br> | `Hex`<br> | Outer radius / edge length of a standard hexagonal unit.

 |

---

## 2. Square Grid Systems & Streaming Cache Dynamics

The `WorldCoordinates::Square` namespace manages transformations between continuous world space and discrete tile grids.

### Spatial Indexing Structs

* **`TileCoord`**: Identifies a specific terrain tile via row ($Z$-axis) and column ($X$-axis) indices.


* **`TexelCoord`**: Identifies specific discrete cells within a terrain tile or collision grid.



### Coordinate Conversions

1. **World to Local Tile (`worldPosToTileCoord`)**: Divides continuous world coordinates by tile size ($4.0\text{ m} \times 256 = 1024.0\text{ m}$) using `std::floor` to establish relative tile offsets.


2. **Absolute Tile Offset (`worldPosToAbsoluteTile`)**: Binds local relative tile coordinates to a global streamer anchor tile (`originTile`) to determine world-wide tile identities.



### Toroidal Streaming Indexing (`slotIndexForTile`)

To maintain an efficient fixed-size $11 \times 11$ memory cache during world traversal, `slotIndexForTile` maps arbitrary 2D tile coordinates into a linearized ring buffer index:

```
                     Tile Coordination (row, col)
                                  │
                                  ▼
                   ┌─────────────────────────────┐
                   │ Modulo Wrap ((v % 11) + 11) │
                   └──────────────┬──────────────┘
                                  │
                       Toroidal Grid (0..10)
                                  │
                                  ▼
                 ┌────────────────────────────────┐
                 │ Linear Index = (r * 11) + c    │
                 └────────────────────────────────┘

```

By taking the modulo of tile indices against `kStreamerGridDim` ($11$), tiles wrap around the memory grid seamlessly as the origin tile shifts, preventing the need to reallocate buffer memory during movement.

---

## 3. Hexagonal Grid System Mechanics

The `WorldCoordinates::Hex` namespace handles transformations between continuous world positions $(x, z)$ and point-topped axial hex coordinates $(q, r)$.

### Axial Conversion Formulas

* **World Space to Axial Hex (`worldToHex`)**:
Translates continuous positions to fractional axial coordinates using standard point-topped projection matrix coefficients:


$$q = \frac{\frac{2}{3} \cdot x}{\text{kHexSize}}, \quad r = \frac{\frac{z}{\text{kHexSize} \cdot \sqrt{3}} - \frac{q}{2}}{1}$$



To resolve fractional coordinates to the nearest discrete hex cell, the function derives implicit cube coordinate $s = -q - r$ and rounds all three values. If rounding errors introduce a non-zero sum ($q + r + s \neq 0$), it recomputes the component with the largest rounding delta to enforce cubic constraint invariants.


* **Axial Hex to World Space (`hexToWorld`)**:
Calculates the continuous world-space center point $(x, z)$ for a target axial coordinate $(q, r)$:


$$x = \text{kHexSize} \cdot \frac{3}{2} \cdot q$$



$$z = \text{kHexSize} \cdot \sqrt{3} \cdot \left(r + \frac{q}{2}\right)$$




---

## 4. Module Integration Roles

```
               ┌─────────────────────────────────────────┐
               │    WorldCoordinates Invariants & Scaling│
               └────────────────────┬────────────────────┘
                                    │
         ┌──────────────────────────┼──────────────────────────┐
         ▼                          ▼                          ▼
┌──────────────────┐       ┌──────────────────┐       ┌──────────────────┐
│ Terrain Streamer │       │    Topography    │       │ Spatial Queries  │
└────────┬─────────┘       └────────┬─────────┘       └────────┬─────────┘
         │                          │                          │
 Allocates & indexes       Translates sampling        Maps entity positions 
 11x11 toroidal slots      points to absolute         to discrete hex cells 
 for heightfield data.     tile texels & buffers.     or collision grids.

```

* **Terrain Streaming Engine**: References `kStreamerGridDim` and `slotIndexForTile` to swap tile data in and out of GPU/CPU ring buffers as the player moves across world boundaries.


* **Topography Sampling Engine**: Uses `worldPosToAbsoluteTile` and `kTexelSizeM` to compute bilinear sub-texel offsets ($f_x, f_z$) for terrain height and normal sampling.


* **Gameplay & Spatial Grids**: Uses `worldToHex` and `hexToWorld` for pathfinding, tactical overlays, and discrete unit placement.