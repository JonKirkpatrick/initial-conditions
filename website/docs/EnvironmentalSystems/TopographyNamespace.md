# High-Level Architecture: `Topography` Namespace

The `Topography` namespace provides dynamic sampling and analytical queries for terrain surfaces across world space. Acting as an evaluation layer atop streaming heightfield data, it bridges tile-based storage buffers with world-space physics, placement algorithms, and rendering pipelines. It translates 2D coordinates into precise 3D surface elevations and normalized slope vectors.

---

## 1. Key Data Structures & Context

* **`Topography::TerrainContext`**: Holds an evaluation snapshot, combining a pointer to the active `TerrainStreamer` with top-left bounds (`worldMin`) and total dimensions (`worldSize`). If `streamer` is null or tile data is unloaded, routines gracefully default to flat ground height ($0.0\text{f}$) and upward normals ($(0, 1, 0)$).


* **`GRID_RESOLUTION` & `BASE_SIZE**`: Global constants defining overall sampling density and world coverage bounds.



---

## 2. World-to-Tile Coordinate Translation

`Topography` queries rely on standard grid transformations provided by `WorldCoordinates::Square`:

1. **Absolute Tile Lookup**: Map 2D world position `worldPos` against origin tile offsets to retrieve `TileCoord absoluteTile` and obtain the raw float buffer (`tileBuffer`).


2. **Local Texel Mapping**: Determine column/row deltas relative to the origin tile to compute minimum world bounds (`tileMinX`, `tileMinZ`).


3. **Texel Fractional Splits**: Convert position offsets into texel units based on `kTexelSizeM`:


* Integer components `tx0` and `tz0` pinpoint the base texel indices within the tile.


* Fractional components `fx` and `fz` $[0.0, 1.0)$ record sub-texel offsets used for bilinear interpolation.





---

## 3. Surface Evaluation Mechanics

### Bilinear Height Interpolation (`heightAt`)

Terrain elevation relies on bilinear interpolation across four adjacent texel heights ($h_{00}, h_{10}, h_{01}, h_{11}$) fetched using tile stride rules (`kStride = kTileResolution + kApronTexels`):

```
(bX0, bZ0)  h00 ─────────── h10  (bX0 + 1, bZ0)
             │               │
             │   *(fx, fz)   │
             │               │
(bX0, bZ0+1) h01 ─────────── h11  (bX0 + 1, bZ0 + 1)

```

$$\text{Height}(f_x, f_z) = (h_{00} + f_x(h_{10} - h_{00})) \cdot (1 - f_z) + (h_{01} + f_x(h_{11} - h_{01})) \cdot f_z$$

### Analytical Surface Normals (`normalAt`)

Rather than relying on finite-difference lookups against neighboring samples, `normalAt` evaluates partial derivatives directly across the bilinear quad surface:

* **Partial $X$-Derivative**: $\frac{\partial H}{\partial x} = (1 - f_z)(h_{10} - h_{00}) + f_z(h_{11} - h_{01})$

* **Partial $Z$-Derivative**: $\frac{\partial H}{\partial z} = (1 - f_x)(h_{01} - h_{00}) + f_x(h_{11} - h_{10})$


These slope rates are scaled by texel spacing (`kTexelSizeM`) to form an unnormalized vector $\vec{N} = \left(-\frac{\partial H / \partial x}{\text{kTexelSizeM}},\, 1.0,\, -\frac{\partial H / \partial z}{\text{kTexelSizeM}}\right)$, which is normalized before being returned.

---

## 4. Execution Flow & Module Integration

```
                 [ 2D World Position (sf::Vector2f) ]
                                  │
                                  ▼
                    ┌───────────────────────────┐
                    │  Topography::TerrainContext│
                    └─────────────┬─────────────┘
                                  │
                       Fetches Active Streamer Buffer
                                  │
                                  ▼
           ┌───────────────────────────────────────────┐
           │   Translate World Pos -> Absolute Tile    │
           └──────────────────────┬────────────────────┘
                                  │
               Calculates (tx0, tz0) & (fx, fz)
                                  │
               ┌──────────────────┴──────────────────┐
               ▼                                     ▼
   ┌───────────────────────┐             ┌───────────────────────┐
   │ Topography::heightAt()│             │ Topography::normalAt()│
   └───────────┬───────────┘             └───────────┬───────────┘
               │                                     │
    Bilinear Height Interp                Analytical Slope Partial
               │                                   Derivatives
               ▼                                     ▼
    [ Terrain Height (m) ]                [ Normalized Normal3f ]

```

* **Physics & Alignment**: Vehicle controllers, character movement, and foliage scattering query `heightAt` and `normalAt` to align transforms and handle ground collision.


* **Decal & Material Shaders**: Mesh deformation or slope-based terrain blending pass `normalAt` gradients to distinguish flat grass from steep cliff slopes.