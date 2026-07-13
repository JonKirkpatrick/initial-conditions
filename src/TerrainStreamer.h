#pragma once

#include <GL/glew.h>
#include <SFML/System.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------
// TerrainManifest
//
// Plain-data mirror of the tileset description schema (schema_version 1).
// TODO: wire this up to whatever JSON parsing you already built for the
// Stage 4 manifest ingestion -- not re-deriving that here.
// ---------------------------------------------------------------------
struct TerrainManifest
{
    int                      schemaVersion = 1;
    std::string              name;
    sf::Vector2<double>       worldOriginLatLon;
    sf::Vector2<double>       worldSizeM;
    int                      tileResolution = 256;   // core, apron excluded
    int                      apronTexels = 1;
    float                    metersPerTexel = 0.0f;
    sf::Vector2i             gridDimensions{7, 7};
    std::filesystem::path    tileDirectory;
    std::string              tileNamingPattern;       // e.g. "tile_{row}_{col}.bin"
    std::vector<std::string> channels;                // e.g. {"height"}

    static TerrainManifest loadFromFile(const std::filesystem::path& manifestPath);
};

// ---------------------------------------------------------------------
// TerrainStreamer
//
// Owns the permanent 7x7 (49-slot) tile buffer on the CPU and streams
// tiles in/out as the camera crosses tile boundaries, eventually via a
// worker thread (Stage 7.4). Exactly one instance is expected to be
// alive at a time, owned by the current scene/world object via
// std::unique_ptr -- not a singleton, to keep teardown-and-replace cheap
// across future iterations of this system.
//
// Public surface is intentionally small: everything else in the engine
// should only ever touch update()/heightAt()/normalAt()/the GPU slice
// feed, never the internals below.
// ---------------------------------------------------------------------
class TerrainStreamer
{
public:
    explicit TerrainStreamer(const std::filesystem::path& manifestPath);
    ~TerrainStreamer();

    TerrainStreamer(const TerrainStreamer&) = delete;
    TerrainStreamer& operator=(const TerrainStreamer&) = delete;

    // Call once per frame. Detects tile-boundary crossings and drives
    // the load/staging/publish pipeline (Stages 7.1-7.4).
    void update(const sf::Vector2f& cameraWorldPos);

    // Height/normal queries, same shape Topography:: already expects.
    // TODO: implementation routes world position -> toroidal slot lookup
    // -> bilinear sample, replacing the flat HeightArray index from the
    // monolithic-tile prototype.
    float heightAt(float worldX, float worldZ) const;
    sf::Vector3f normalAt(float worldX, float worldZ) const;

    // Stage 6: feeds u_ActiveTileSlices[25]. Shape/semantics TBD when
    // Stage 6 is actually implemented -- placeholder for now.
    const std::array<GLint, 25>& getActiveSliceUniforms() const;

private:
    // A tile's position in *world* tile-grid space (not slot space).
    struct TileCoord
    {
        int row = 0;
        int col = 0;

        bool operator==(const TileCoord& other) const
        {
            return row == other.row && col == other.col;
        }
    };

    // ---- Stage 7.2: pure "load tile from disk" ----
    // No shared state touched -- coordinate + manifest in, filled
    // apron-inclusive buffer out. Safe to call from the worker thread.
    static bool loadTileFromDisk(const TerrainManifest& manifest,
                                  TileCoord coord,
                                  float* outBuffer /* tileAllocFloatCount() floats */);

    // ---- Stage 7.1: toroidal ring-buffer indexing ----
    int slotIndexForWorldTile(TileCoord coord) const;
    TileCoord worldPosToTileCoord(const sf::Vector2f& worldPos) const;
    void checkBoundaryCrossing(const sf::Vector2f& cameraWorldPos);

    // ---- Stage 7.3: staging buffer ----
    void publishStagedTile(int slotIndex, TileCoord newCoord);

    // ---- Stage 7.4: worker thread + queues ----
    struct LoadRequest  { int slotIndex; TileCoord coord; };
    struct LoadResult   { int slotIndex; bool success; };

    void workerThreadMain();
    void requestTileLoad(int slotIndex, TileCoord coord);
    void drainCompletionQueue(); // called from update(), main thread only

    // ---- slot storage helpers ----
    size_t tileAllocFloatCount() const; // (resolution + 2*apron)^2
    float* slotData(int slotIndex);
    const float* slotData(int slotIndex) const;

    TerrainManifest m_manifest;

    static constexpr int kGridDim = 7; // permanent 7x7 = 49 slots

    // One contiguous allocation for all 49 slots -- made once at
    // construction, never resized or reallocated afterward.
    std::vector<float>     m_tileStorage;
    std::vector<TileCoord> m_slotWorldCoord;  // which world tile each slot currently holds
    std::vector<bool>      m_slotValid;       // false until first successful load

    TileCoord m_centerTileCoord{}; // world tile currently anchoring the toroidal wrap

    // Staging (Stage 7.3): one scratch buffer, reused per in-flight load.
    std::vector<float> m_stagingBuffer;

    // Worker thread plumbing (Stage 7.4) -- present so the shutdown path
    // is correct from day one, per the note about anticipated teardown.
    std::thread             m_workerThread;
    std::mutex               m_requestQueueMutex;
    std::queue<LoadRequest>  m_requestQueue;
    std::mutex               m_completionQueueMutex;
    std::queue<LoadResult>   m_completionQueue;
    std::condition_variable  m_workerWakeCV;
    std::atomic<bool>        m_shutdownRequested{false};

    std::array<GLint, 25> m_activeSliceUniforms{}; // Stage 6 placeholder
};