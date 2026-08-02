#pragma once

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
 * @brief Describes a terrain dataset on disk: its coordinate frame, tile
 *        bounds, channel list, and where its tile files live.
 *
 * Manifests are loaded once via loadFromFile() and treated as immutable for
 * the lifetime of a TerrainStreamer; the streamer's worker thread reads from
 * a TerrainManifest concurrently with the main thread, so no field should be
 * mutated after construction.
 */
struct TerrainManifest
{
    /// Version of the on-disk manifest schema this struct expects to parse.
    int                      schemaVersion = 2;

    /// Human-readable name of the terrain dataset.
    std::string              name;

    /// Latitude/longitude of the world-space origin (x = lat, y = lon).
    sf::Vector2<double>      worldOriginLatLon;

    /// Tile coordinate of the dataset's upper-left (minimum row/col) bound.
    WorldCoordinates::Square::TileCoord tileBoundsUpperLeft;

    /// Tile coordinate of the dataset's lower-right (maximum row/col) bound.
    WorldCoordinates::Square::TileCoord tileBoundsLowerRight;

    /// Directory containing the per-tile height/road binary files.
    std::filesystem::path    tileDirectory;

    /// Names of optional data channels present in this dataset (e.g. "roads").
    std::vector<std::string> channels;

    /**
     * @brief Parse a terrain manifest JSON file from disk.
     * @param manifestPath Path to the manifest file.
     * @return A fully populated TerrainManifest.
     * @throws std::runtime_error if the file cannot be opened, is not valid
     *         JSON, or is missing required fields.
     */
    static TerrainManifest loadFromFile(const std::filesystem::path& manifestPath);

    /**
     * @brief Check whether this manifest advertises a given data channel.
     * @param channelName Channel to look for (e.g. "roads").
     * @return true if the channel is present in channels.
     */
    bool hasChannel(const std::string& channelName) const {
        for (const auto& c : channels) {
            if (c == channelName) return true;
        }
        return false;
    }
};

/**
 * @brief Streams a windowed grid of terrain tiles around the camera,
 *        loading tile data asynchronously and uploading it to GPU array
 *        textures for rendering.
 *
 * TerrainStreamer maintains a fixed-size ring of CPU-side tile "slots"
 * covering a square window of tiles around the camera. As the camera moves
 * across tile boundaries, stale slots are invalidated and reloaded from disk
 * on a dedicated worker thread, so the main/render thread is never blocked
 * waiting on disk I/O after the initial load.
 *
 * @par Threading model
 * Exactly one background worker thread is spawned per instance. The worker
 * thread only ever touches request/result data owned by the queues (never
 * the CPU tile cache or any OpenGL object directly); all cache mutation and
 * all GL calls happen exclusively on the thread that calls update() and the
 * other public members. Callers must therefore invoke all public methods of
 * a given instance from a single, consistent thread (typically the
 * game's main/render thread).
 *
 * @par Lifecycle
 * update() must be called once per frame with the current camera position.
 * The first call performs a one-time blocking load of the initial tile
 * window; subsequent calls are non-blocking and asynchronously refresh
 * slots as the camera crosses tile boundaries.
 */
class TerrainStreamer
{
public:
    using TileCoord = WorldCoordinates::Square::TileCoord;

    /**
     * @brief Construct a streamer from a terrain manifest, allocate CPU/GPU
     *        storage, and start the background loading thread.
     * @param manifestPath Path to the terrain manifest JSON file to load.
     * @throws std::runtime_error if the manifest cannot be loaded/parsed.
     */
    explicit TerrainStreamer(const std::filesystem::path& manifestPath);

    /**
     * @brief Signal the worker thread to stop, join it, and release all
     *        owned OpenGL objects.
     */
    ~TerrainStreamer();

    TerrainStreamer(const TerrainStreamer&) = delete;
    TerrainStreamer& operator=(const TerrainStreamer&) = delete;

    /**
     * @brief Advance streaming state for the current frame.
     *
     * Drains any tile loads completed by the worker thread since the last
     * call, and, once the initial grid is loaded, checks whether the camera
     * has crossed a tile boundary and kicks off asynchronous reloads for any
     * slots that fell outside the visible window. Must be called once per
     * frame from the same thread every time.
     *
     * @param cameraWorldPos Current camera position in world space.
     *
     * @note The very first call blocks until the initial tile window has
     *       finished loading; all subsequent calls are non-blocking.
     */
    void update(const sf::Vector2f& cameraWorldPos);

    /**
     * @brief Get the per-slice validity uniforms for the active subgrid.
     * @return Array of flags (1 = valid/loaded, 0 = invalid/not yet loaded),
     *         indexed in the same row-major order as getActiveSubgrid().
     */
    const std::array<GLint, 81>& getActiveSliceUniforms() const;

    /// @brief Get the terrain manifest this streamer was constructed from.
    const TerrainManifest& getManifest() const { return m_manifest; }

    /**
     * @brief Get a pointer to the cached heightfield data for a tile.
     * @param coord Absolute tile coordinate to look up.
     * @return Pointer to the tile's height buffer, or nullptr if that tile
     *         is not currently resident/valid in the cache.
     */
    const float* getTileData(TileCoord coord) const;

    /**
     * @brief Get a pointer to the cached road-SDF data for a tile.
     * @param coord Absolute tile coordinate to look up.
     * @return Pointer to the tile's road buffer, or nullptr if that tile is
     *         not currently resident/valid in the cache.
     */
    const float* getRoadData(TileCoord coord) const;

    /// @brief Get the tile coordinate corresponding to the manifest's origin.
    TileCoord getOriginTile() const;

    /**
     * @brief A single layer of the active (visible) tile subgrid, as seen
     *        by the renderer.
     */
    struct ActiveTileSlice
    {
        /// Absolute tile coordinate this slice represents.
        WorldCoordinates::Square::TileCoord coord;

        /// Pointer to cached heightfield data, or nullptr if not loaded.
        const float* heightData = nullptr;

        /// Pointer to cached road-SDF data, or nullptr if not loaded.
        const float* roadData   = nullptr;

        /// True if this slice's data is currently loaded and up to date.
        bool         valid      = false;
    };

    /// Row-major grid of tile slices currently visible around the camera.
    using ActiveSubgrid = std::array<ActiveTileSlice, WorldCoordinates::Square::kVisibleGridDim * WorldCoordinates::Square::kVisibleGridDim>;

    /**
     * @brief Build a snapshot of the currently visible tile window.
     * @return An ActiveSubgrid describing each visible tile slot and
     *         whether its data is presently valid.
     */
    ActiveSubgrid getActiveSubgrid() const;

    /**
     * @brief Get the heightfield array texture, uploading fresh data first
     *        if the active subgrid has changed since the last upload.
     * @return GL handle of the heightfield GL_TEXTURE_2D_ARRAY.
     * @note Must be called from a thread with the relevant GL context current.
     */
    GLuint getOrUploadArrayTexture();

    /**
     * @brief Get the road-SDF array texture, uploading fresh data first if
     *        the active subgrid has changed since the last upload.
     * @return GL handle of the road-SDF GL_TEXTURE_2D_ARRAY.
     * @note Must be called from a thread with the relevant GL context current.
     */
    GLuint getOrUploadRoadArrayTexture();

    /// @brief Get the world-space origin (in meters) of the visible tile grid.
    sf::Vector2f getVisibleGridWorldOrigin() const;

private:
    /**
     * @brief Load a single tile's height and road data from disk.
     *
     * Called only on the worker thread. Reads directly into caller-owned
     * buffers; missing files are treated as all-zero height / all-one road
     * (i.e. flat terrain, no road) rather than an error.
     *
     * @param manifest        Manifest describing where tile files live.
     * @param coord           Absolute tile coordinate to load.
     * @param outHeightBuffer Destination buffer for height samples; must be
     *                        at least tileAllocFloatCount() floats.
     * @param outRoadBuffer   Destination buffer for road samples; must be
     *                        at least tileAllocFloatCount() floats.
     * @return true on success (including the "file absent, defaults used"
     *         case).
     */
    static bool loadTileFromDisk(const TerrainManifest& manifest,
                                 TileCoord coord,
                                 float* outHeightBuffer,
                                 float* outRoadBuffer);

    /// @brief Clamp a tile coordinate to the manifest's dataset bounds.
    TileCoord clampToWorldBounds(TileCoord coord) const;

    /// @brief Convert a world-space camera position to an absolute tile coordinate.
    TileCoord worldPosToAbsoluteTileCoord(const sf::Vector2f& cameraWorldPos) const;

    /**
     * @brief Perform the one-time initial load of the tile window around
     *        the camera, blocking until every requested tile has completed.
     * @param cameraWorldPos Current camera position in world space.
     */
    void initializeGrid(const sf::Vector2f& cameraWorldPos);

    /**
     * @brief Check whether the camera has crossed into a new tile since the
     *        last update, and if so, asynchronously request reloads for any
     *        slots that no longer fall within the visible window.
     *
     * Never blocks: invalidated slots simply render as "not yet valid"
     * until their asynchronous reload completes on a later frame.
     *
     * @param cameraWorldPos Current camera position in world space.
     */
    void checkBoundaryCrossing(const sf::Vector2f& cameraWorldPos);

    /// @brief Recompute m_activeSliceUniforms from the current slot validity state.
    void refreshActiveSliceUniforms();

    /// @brief A single pending tile-load request handed to the worker thread.
    struct LoadRequest  
    { 
        /// CPU cache slot this result should be written into.
        int slotIndex; 
        /// Absolute tile coordinate to load.
        TileCoord coord; 
    };

    /// @brief A completed tile load, carrying its own freshly-loaded data.
    struct LoadResult   
    { 
        /// CPU cache slot this result should be written into.
        int slotIndex; 
        /// Absolute tile coordinate that was loaded.
        TileCoord coord; 
        /// Whether the load completed successfully.
        bool success;
        /// Freshly loaded heightfield samples for this tile.
        std::vector<float> heightBuffer;
        /// Freshly loaded road-SDF samples for this tile.
        std::vector<float> roadBuffer;
    };

    /**
     * @brief Background worker loop: waits for load requests, loads tiles
     *        from disk, and pushes completed results to the completion
     *        queue for the main thread to consume.
     *
     * Runs for the lifetime of the streamer, exiting only once shutdown has
     * been requested and the request queue has drained. Never touches the
     * CPU tile cache, slot-validity state, or any OpenGL object directly.
     */
    void workerThreadMain();

    /**
     * @brief Enqueue an asynchronous tile load and wake the worker thread.
     * @param slotIndex CPU cache slot the result should be written into.
     * @param coord     Absolute tile coordinate to load.
     */
    void requestTileLoad(int slotIndex, TileCoord coord);

    /**
     * @brief Non-blocking: consume all currently-available completed loads
     *        from the completion queue, writing their data into the CPU
     *        tile cache and marking the corresponding slots dirty/valid.
     *
     * Must be called from the same thread as the rest of the public API
     * (typically once per frame, at the top of update()).
     */
    void drainCompletionQueue();

    /// @brief Number of floats allocated per tile (including apron texels).
    size_t tileAllocFloatCount() const;
    
    /// @brief Mutable pointer to a slot's heightfield data within m_tileStorage.
    float* slotData(int slotIndex);
    /// @brief Const pointer to a slot's heightfield data within m_tileStorage.
    const float* slotData(int slotIndex) const;
    /// @brief Mutable pointer to a slot's road-SDF data within m_roadStorage.
    float* slotRoadData(int slotIndex);
    /// @brief Const pointer to a slot's road-SDF data within m_roadStorage.
    const float* slotRoadData(int slotIndex) const;

    /// Manifest this streamer was constructed from; immutable after construction.
    TerrainManifest m_manifest;

    // CPU Cache Storage

    /// Flat storage for all slots' heightfield data (tileAllocFloatCount() floats per slot).
    std::vector<float>     m_tileStorage;
    /// Flat storage for all slots' road-SDF data (tileAllocFloatCount() floats per slot).
    std::vector<float>     m_roadStorage;
    /// Absolute tile coordinate currently occupying each slot.
    std::vector<TileCoord> m_slotWorldCoord;
    /// Whether each slot's data is currently loaded and up to date.
    std::vector<bool>      m_slotValid;

    /// Absolute tile coordinate the visible window is currently centered on.
    TileCoord m_centerTileCoord{};

    // Threading

    /// Background thread running workerThreadMain() for this instance's lifetime.
    std::thread              m_workerThread;
    /// Guards m_requestQueue.
    std::mutex               m_requestQueueMutex;
    /// Pending tile-load requests, consumed by the worker thread.
    std::queue<LoadRequest>  m_requestQueue;
    /// Guards m_completionQueue.
    std::mutex               m_completionQueueMutex;
    /// Completed tile loads awaiting consumption by the main thread.
    std::queue<LoadResult>   m_completionQueue;
    /// Wakes the worker thread when a new request is enqueued or shutdown begins.
    std::condition_variable  m_workerWakeCV;
    /// Signaled by the worker whenever a result is pushed; used to block
    /// initializeGrid() until the initial tile window has fully loaded.
    std::condition_variable  m_initialLoadCV;
    /// Set on destruction to tell the worker thread to exit.
    std::atomic<bool>        m_shutdownRequested{false};

    /// Per-slice validity flags (1/0) for the current active subgrid, in
    /// the same row-major order as ActiveSubgrid, suitable for direct
    /// upload as a shader uniform array.
    std::array<GLint, 81> m_activeSliceUniforms{};
    
    // OpenGL Objects

    /// GL_TEXTURE_2D_ARRAY holding the visible window's heightfield data.
    GLuint m_arrayTexture     = 0;
    /// Pixel unpack buffer used to stream data into m_arrayTexture.
    GLuint m_pbo              = 0;
    /// GL_TEXTURE_2D_ARRAY holding the visible window's road-SDF data.
    GLuint m_roadArrayTexture = 0;
    /// Pixel unpack buffer used to stream data into m_roadArrayTexture.
    GLuint m_roadPbo          = 0;

    /// True if m_arrayTexture needs to be re-uploaded from the CPU cache.
    bool m_subgridDirty     = true;
    /// True if m_roadArrayTexture needs to be re-uploaded from the CPU cache.
    bool m_roadSubgridDirty = true;
    /// True once the initial tile window has finished loading.
    bool m_gridInitialized  = false;

    /// Scratch buffer holding packed heightfield data for the next texture upload.
    std::vector<float> m_packedSubgridData;
    /// Scratch buffer holding packed road-SDF data for the next texture upload.
    std::vector<float> m_packedRoadSubgridData;
};