import json
import math
from pathlib import Path
import numpy as np
from PIL import Image
import rasterio
from scipy.ndimage import gaussian_filter
from rasterio.enums import Resampling
from rasterio.windows import Window

def pack_to_24bit_rgb(height_array_cm, max_height_cm=100000.0):
    """Legacy 24-bit RGB PNG packing fallback (Stage 2a)."""
    normalized = np.minimum(np.clip(height_array_cm / max_height_cm, 0, 1), 0.99999994)
    scaled_int = np.floor(normalized * 16777215.0).astype(np.int32)
    
    r = (scaled_int // 65536).astype(np.uint8)
    scaled_int -= r.astype(np.int32) * 65536
    g = (scaled_int // 256).astype(np.uint8)
    b = (scaled_int - g.astype(np.int32) * 256).astype(np.uint8)
    
    return np.stack([r, g, b], axis=-1)


def build_tileset(
    cog_path: str,
    name: str,
    output_dir: str,
    tile_resolution: int = 256,
    apron_texels: int = 1,
    target_meters_per_texel: float = 3.4,  # <-- Crucial metric scale constraint
    export_bin: bool = True,
    export_png: bool = False
):
    cog_path = Path(cog_path)
    base_output_path = Path(output_dir) / name
    tiles_output_path = base_output_path / "tiles"
    tiles_output_path.mkdir(parents=True, exist_ok=True)

    print(f"Opening Source COG: {cog_path.name}")
    with rasterio.open(cog_path) as src:
        bounds = src.bounds
        cog_width_px = src.width
        cog_height_px = src.height
        res_x, res_y = src.res
        
        # Geodesy calculation for local meter matching
        mid_lat = (bounds.top + bounds.bottom) / 2.0
        meters_per_degree_lat = 111320.0
        meters_per_degree_lon = meters_per_degree_lat * math.cos(math.radians(mid_lat))
        
        native_meters_per_px_x = res_x * meters_per_degree_lon if src.crs.is_geographic else res_x
        native_meters_per_px_y = res_y * meters_per_degree_lat if src.crs.is_geographic else res_y
        
        # Calculate full size of world footprint in meters
        total_world_width_m = cog_width_px * native_meters_per_px_x
        total_world_height_m = cog_height_px * native_meters_per_px_y
        
        # How much geographic real estate does ONE virtual tile cover?
        tile_coverage_m = tile_resolution * target_meters_per_texel
        
        # Convert that meter footprint back into a floating-point number of RAW COG pixels
        raw_pixels_per_tile_x = tile_coverage_m / native_meters_per_px_x
        raw_pixels_per_tile_y = tile_coverage_m / native_meters_per_px_y
        
        # Determine the size of the entire tile grid map
        cols = math.ceil(cog_width_px / raw_pixels_per_tile_x)
        rows = math.ceil(cog_height_px / raw_pixels_per_tile_y)
        grid_dimensions = [rows, cols]
        total_tiles = rows * cols
        
        # Account for fractional raw pixel remainders on edge tiles
        partial_band_right_px = cog_width_px % raw_pixels_per_tile_x
        partial_band_bottom_px = cog_height_px % raw_pixels_per_tile_y
        
        print(f"-> Target Scale: {target_meters_per_texel} m/pixel ({tile_coverage_m:.1f}m coverage per tile)")
        print(f"-> Virtual Grid: {rows} rows x {cols} cols ({total_tiles} total tiles generated via interpolation)")

        # Target output array dimensions: 256 + 1 = 257
        out_shape_dim = tile_resolution + apron_texels
        
        # Scaling factor for the right/bottom padding relative to the raw file
        raw_apron_x = apron_texels * (raw_pixels_per_tile_x / tile_resolution)
        raw_apron_y = apron_texels * (raw_pixels_per_tile_y / tile_resolution)

        for r in range(rows):
            for c in range(cols):
                # Core raw source frame positions (Top-Left)
                src_x = c * raw_pixels_per_tile_x
                src_y = r * raw_pixels_per_tile_y
                
                # We start EXACTLY at the tile origin (no left/top padding)
                window_x = src_x
                window_y = src_y
                
                # We extend the window width/height outward to catch the right/bottom border
                window_w = raw_pixels_per_tile_x + raw_apron_x
                window_h = raw_pixels_per_tile_y + raw_apron_y
                
                # Edge guard rail clamp
                if window_x + window_w > cog_width_px:
                    window_w = cog_width_px - window_x
                if window_y + window_h > cog_height_px:
                    window_h = cog_height_px - window_y

                # Read window section and up-sample smoothly to exactly (257, 257)
                window = Window(window_x, window_y, window_w, window_h)
                
                tile_data = src.read(
                    1, 
                    window=window,
                    out_shape=(out_shape_dim, out_shape_dim),
                    resampling=Resampling.cubic_spline
                ).astype(np.float32)

                # Post-process clip and clean data arrays
                tile_data = np.maximum(tile_data, 0.0)
                
                base_filename = f"tile_{r}_{c}"

                if export_bin:
                    bin_file = tiles_output_path / f"{base_filename}.bin"
                    with open(bin_file, "wb") as f:
                        f.write(tile_data.tobytes())

                if export_png:
                    png_file = tiles_output_path / f"{base_filename}.png"
                    height_cm = tile_data * 100.0
                    rgb_packed = pack_to_24bit_rgb(height_cm)
                    Image.fromarray(rgb_packed, 'RGB').save(png_file)

        # 5. Build and output final metadata manifest
        manifest = {
            "schema_version": 1,
            "name": name,
            "world_origin_latlon": [bounds.top, bounds.left],
            "world_size_m": [round(total_world_width_m, 2), round(total_world_height_m, 2)],
            "tile_resolution": tile_resolution,
            "apron_texels": apron_texels,
            "meters_per_texel": target_meters_per_texel,
            "grid_dimensions": grid_dimensions,
            "tile_directory": f"tiles/{name}/",
            "tile_naming_pattern": "tile_{row}_{col}.bin" if export_bin else "tile_{row}_{col}.png",
            "channels": ["height"],
            "diagnostics": {
                "total_tiles": total_tiles,
                "bottom_right_latlon": [bounds.bottom, bounds.right],
                "partial_edge_remainder_raw_px": {
                    "right_edge_width": round(partial_band_right_px, 2),
                    "bottom_edge_height": round(partial_band_bottom_px, 2)
                }
            }
        }

        with open(base_output_path / "manifest.json", "w") as f:
            json.dump(manifest, f, indent=2)
            
        print(f"Success! Saved manifest and compiled tiles down into '{base_output_path}'")

if __name__ == "__main__":
    build_tileset(
        cog_path='Copernicus_DSM_COG_10_N47_00_W053_00_DEM.tif',
        name='Avalon',
        output_dir='./output',
        tile_resolution=256,
        apron_texels=1,
        target_meters_per_texel=4.0,  # Matches the target specification
        export_bin=True,
        export_png=True               # Generates legacy PNG along with raw bins
    )