#pragma once

/**
 * @file TerrainStreamer.hpp
 * @brief Multithreaded asynchronous terrain streaming and subgrid texture array manager.
 * 
 * Manages background disk I/O, CPU caching, and GPU texture array generation (`GL_TEXTURE_2D_ARRAY`) 
 * for heightmaps and road Signed Distance Fields (SDF) around a moving camera view center.
 */

#include <GL/glew.h>
#include <SFML/System.hpp>
#include "core/WorldCoordinates.hpp"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Metadata manifest defining tile bounds, schema versions, and channel layouts.
 */
struct TerrainManifest
{
    int                      schemaVersion = 2;              ///< Schema format version number.
    std::string              name;                           ///< Display name of terrain dataset.
    sf::Vector2<double>      worldOriginLatLon;             ///< Geographic origin (Latitude, Longitude) in degrees.
    WorldCoordinates::Square::TileCoord tileBoundsUpperLeft;  ///< Top-left tile bounding coordinate.
    WorldCoordinates::Square::TileCoord tileBoundsLowerRight; ///< Bottom-right tile bounding coordinate.
    std::filesystem::path    tileDirectory;                  ///< Root directory path containing binary tile files.
    std::vector<std::string> channels;                       ///< List of stored data channels (e.g., "height", "road").

    /**
     * @brief Loads and parses a terrain manifest file from disk.
     * @param manifestPath Path to JSON/manifest file.
     * @return Parsed `TerrainManifest` instance.
     */
    static TerrainManifest loadFromFile(const std::filesystem::path& manifestPath);
    
    /**
     * @brief Checks if a specific channel is declared in the dataset manifest.
     * @param channelName Name of data channel (e.g. "height", "road").
     * @return `true` if channel exists in manifest, `false` otherwise.
     */
    bool hasChannel(const std::string& channelName) const {
        for (const auto& c : channels) {
            if (c == channelName) return true;
        }
        return false;
    }
};

/**
 * @brief Streams terrain heightmap and road SDF data centered around camera position.
 * 
 * Maintains a active visible $N \times N$ subgrid array of tiles, issuing asynchronous load 
 * requests to a worker thread and pushing active layers to OpenGL 2D Texture Arrays.
 */
class TerrainStreamer
{
public:
    using TileCoord = WorldCoordinates::Square::TileCoord;

    /**
     * @brief Constructs streamer and initializes background I/O threads.
     * @param manifestPath Path to terrain manifest file.
     */
    explicit TerrainStreamer(const std::filesystem::path& manifestPath);

    /**
     * @brief Destructor. Signals worker thread shutdown and cleans up OpenGL textures/PBOs.
     */
    ~TerrainStreamer();

    TerrainStreamer(const TerrainStreamer&) = delete;
    TerrainStreamer& operator=(const TerrainStreamer&) = delete;

    /**
     * @brief Updates stream position based on camera world coordinates.
     * 
     * Drains I/O completion queues, evaluates boundary crossings, and updates subgrid layout.
     * @param cameraWorldPos Current camera position in world space.
     */
    void update(const sf::Vector2f& cameraWorldPos);

    /**
     * @brief Gets uniform indices mapping subgrid slots to 2D texture array slice indices.
     * @return Reference to fixed array of 81 uniform slice values.
     */
    const std::array<GLint, 81>& getActiveSliceUniforms() const;

    /**
     * @brief Gets terrain dataset manifest.
     * @return Immutable reference to loaded `TerrainManifest`.
     */
    const TerrainManifest& getManifest() const { return m_manifest; }

    /**
     * @brief Bilinearly samples height value at given world space coordinates.
     * @param worldPos 2D coordinates in world space.
     * @return Interpolated height value in meters.
     */
    float sampleHeightAt(sf::Vector2f worldPos) const;

    /**
     * @brief Gets raw height data buffer for a specific tile coordinate.
     * @param coord Target tile coordinate.
     * @return Pointer to contiguous float height buffer, or `nullptr` if not cached.
     */
    const float* getTileData(TileCoord coord) const;

    /**
     * @brief Gets raw road SDF data buffer for a specific tile coordinate.
     * @param coord Target tile coordinate.
     * @return Pointer to contiguous float road SDF buffer, or `nullptr` if not cached.
     */
    const float* getRoadData(TileCoord coord) const;

    /**
     * @brief Gets top-left origin tile coordinate of current active subgrid.
     * @return Active subgrid origin `TileCoord`.
     */
    TileCoord getOriginTile() const;

    /**
     * @brief Slot metadata structure representing a single active subgrid tile slice.
     */
    struct ActiveTileSlice
    {
        WorldCoordinates::Square::TileCoord coord;  ///< Tile grid coordinate.
        const float* heightData = nullptr;         ///< Pointer to loaded height data array.
        const float* roadData   = nullptr;         ///< Pointer to loaded road SDF data array.
        bool         valid      = false;           ///< `true` if tile data is fully loaded and valid.
    };

    /** @brief Type alias for full $N \times N$ active subgrid array. */
    using ActiveSubgrid = std::array<ActiveTileSlice, WorldCoordinates::Square::kVisibleGridDim * WorldCoordinates::Square::kVisibleGridDim>;

    /**
     * @brief Retrieves complete snapshot array of all active subgrid tile slices.
     * @return Populated `ActiveSubgrid` structure.
     */
    ActiveSubgrid getActiveSubgrid() const;
    
    /**
     * @brief Uploads or returns OpenGL 2D Texture Array for height field (`GL_R32F`).
     * @return OpenGL texture object handle (`GLuint`).
     */
    GLuint getOrUploadArrayTexture();

    /**
     * @brief Uploads or returns OpenGL 2D Texture Array for road SDF (`GL_R32F`).
     * @return OpenGL texture object handle (`GLuint`).
     */
    GLuint getOrUploadRoadArrayTexture();
    
    /**
     * @brief Calculates top-left world-space origin position of visible grid.
     * @return 2D vector position in world space.
     */
    sf::Vector2f getVisibleGridWorldOrigin() const;

private:

    /**
     * @brief Helper loading raw binary tile channels from disk into preallocated CPU buffers.
     * 
     * @param manifest Terrain manifest reference.
     * @param coord Target tile coordinate to load.
     * @param[out] outHeightBuffer Destination float buffer for height channel data.
     * @param[out] outRoadBuffer Destination float buffer for road SDF channel data.
     * @return `true` if tile files were read successfully, `false` otherwise.
     */
    static bool loadTileFromDisk(const TerrainManifest& manifest,
                                  TileCoord coord,
                                  float* outHeightBuffer,
                                  float* outRoadBuffer);

    /**
     * @brief Clamps tile coordinate within manifest bounding limits.
     * @param coord Input tile coordinate.
     * @return Bounded tile coordinate.
     */
    TileCoord clampToWorldBounds(TileCoord coord) const;

    /**
     * @brief Maps world-space camera position to absolute tile coordinate.
     * @param cameraWorldPos Camera location in world space.
     * @return Absolute `TileCoord`.
     */
    TileCoord worldPosToAbsoluteTileCoord(const sf::Vector2f& cameraWorldPos) const;

    /**
     * @brief Pre-initializes subgrid state centered on camera location.
     * @param cameraWorldPos Camera location in world space.
     */
    void initializeGrid(const sf::Vector2f& cameraWorldPos);

    /**
     * @brief Synchronously loads a tile into a specific internal slot.
     * @param coord Target tile coordinate.
     */
    void loadTileIntoSlot(TileCoord coord);

    /**
     * @brief Checks if camera crossed subgrid center boundary and re-centers active grid.
     * @param cameraWorldPos Camera position in world coordinates.
     */
    void checkBoundaryCrossing(const sf::Vector2f& cameraWorldPos);

    /**
     * @brief Recomputes OpenGL slice index uniforms mapped to internal subgrid slots.
     */
    void refreshActiveSliceUniforms();

    /**
     * @brief Promotes staged background thread buffer data into active slot cache.
     * @param slotIndex Destination slot index.
     * @param newCoord Associated tile coordinate.
     */
    void publishStagedTile(int slotIndex, TileCoord newCoord);

    /** @brief Internal structure for asynchronous tile load requests. */
    struct LoadRequest  { int slotIndex; TileCoord coord; };

    /** @brief Internal structure for completed tile load results. */
    struct LoadResult   { int slotIndex; bool success; };

    /**
     * @brief Main loop for asynchronous disk I/O worker thread.
     */
    void workerThreadMain();

    /**
     * @brief Pushes a new tile load request onto the worker thread queue.
     * @param slotIndex Target subgrid slot index.
     * @param coord Tile coordinate to fetch.
     */
    void requestTileLoad(int slotIndex, TileCoord coord);

    /**
     * @brief Processes completed worker thread load results on main thread.
     */
    void drainCompletionQueue();

    /**
     * @brief Calculates total float elements contained within one tile channel.
     * @return Total float count per tile.
     */
    size_t tileAllocFloatCount() const;
    
    /**
     * @brief Gets mutable pointer to height buffer for specified slot.
     * @param slotIndex Internal slot index.
     * @return Mutable float pointer.
     */
    float* slotData(int slotIndex);

    /**
     * @brief Gets immutable pointer to height buffer for specified slot.
     * @param slotIndex Internal slot index.
     * @return Const float pointer.
     */
    const float* slotData(int slotIndex) const;

    /**
     * @brief Gets mutable pointer to road SDF buffer for specified slot.
     * @param slotIndex Internal slot index.
     * @return Mutable float pointer.
     */
    float* slotRoadData(int slotIndex);

    /**
     * @brief Gets immutable pointer to road SDF buffer for specified slot.
     * @param slotIndex Internal slot index.
     * @return Const float pointer.
     */
    const float* slotRoadData(int slotIndex) const;

    TerrainManifest m_manifest; ///< Loaded dataset manifest metadata.

    // CPU Cache Storage
    std::vector<float>     m_tileStorage;     ///< Contiguous CPU backing storage for height channels.
    std::vector<float>     m_roadStorage;     ///< Contiguous CPU backing storage for road SDF channels.
    std::vector<TileCoord> m_slotWorldCoord;  ///< Mapped tile coordinates per slot.
    std::vector<bool>      m_slotValid;       ///< Loaded status flags per slot.

    TileCoord m_centerTileCoord{};            ///< Current center tile coordinate of active grid.

    // Async staging
    std::vector<float> m_stagingBuffer;       ///< Staging buffer for async height loading.
    std::vector<float> m_stagingRoadBuffer;   ///< Staging buffer for async road SDF loading.

    std::thread              m_workerThread;          ///< Background disk I/O worker thread.
    std::mutex               m_requestQueueMutex;     ///< Mutex guarding request queue.
    std::queue<LoadRequest>  m_requestQueue;          ///< Queue of pending disk load requests.
    std::mutex               m_completionQueueMutex;  ///< Mutex guarding completion queue.
    std::queue<LoadResult>   m_completionQueue;       ///< Queue of finished tile load results.
    std::condition_variable  m_workerWakeCV;          ///< Condition variable signaling worker thread.
    std::atomic<bool>        m_shutdownRequested{false}; ///< Flag requesting worker thread termination.

    std::array<GLint, 81> m_activeSliceUniforms{};     ///< Array uniform mapping 81 subgrid tiles to texture slices.
    
    // OpenGL GPU Texture & PBO Objects
    GLuint m_arrayTexture     = 0; ///< Handle to OpenGL 2D Texture Array for heightfield (`GL_TEXTURE_2D_ARRAY`).
    GLuint m_pbo              = 0; ///< Pixel Buffer Object handle for heightfield asynchronous GPU uploads.
    
    GLuint m_roadArrayTexture = 0; ///< Handle to OpenGL 2D Texture Array for road SDF (`GL_TEXTURE_2D_ARRAY`).
    GLuint m_roadPbo          = 0; ///< Pixel Buffer Object handle for road SDF asynchronous GPU uploads.

    bool m_subgridDirty     = true; ///< Dirty flag indicating height texture array requires GPU update.
    bool m_roadSubgridDirty = true; ///< Dirty flag indicating road texture array requires GPU update.
    bool m_gridInitialized  = false; ///< Flag indicating initial subgrid layout has been established.

    std::vector<float> m_packedSubgridData;     ///< Staging buffer packing active 81 tile layers for height GPU upload.
    std::vector<float> m_packedRoadSubgridData; ///< Staging buffer packing active 81 tile layers for road SDF GPU upload.
};