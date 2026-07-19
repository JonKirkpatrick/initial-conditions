#pragma once

#include <GL/glew.h>
#include <SFML/System.hpp>
#include "core/WorldCoordinates.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

struct TerrainManifest
{
    int                      schemaVersion = 2;
    std::string              name;
    sf::Vector2<double>      worldOriginLatLon;
    WorldCoordinates::Square::TileCoord tileBoundsUpperLeft;
    WorldCoordinates::Square::TileCoord tileBoundsLowerRight;
    std::filesystem::path    tileDirectory;
    std::vector<std::string> channels;

    static TerrainManifest loadFromFile(const std::filesystem::path& manifestPath);
};

class TerrainStreamer
{
public:
    using TileCoord = WorldCoordinates::Square::TileCoord;

    explicit TerrainStreamer(const std::filesystem::path& manifestPath);
    ~TerrainStreamer();

    TerrainStreamer(const TerrainStreamer&) = delete;
    TerrainStreamer& operator=(const TerrainStreamer&) = delete;

    void update(const sf::Vector2f& cameraWorldPos);

    const std::array<GLint, 81>& getActiveSliceUniforms() const;
    const TerrainManifest& getManifest() const { return m_manifest; }
    float sampleHeightAt(sf::Vector2f worldPos) const;
    const float* getTileData(TileCoord coord) const;
    TileCoord getOriginTile() const;

    struct ActiveTileSlice
    {
        WorldCoordinates::Square::TileCoord coord;
        const float* data  = nullptr;
        bool         valid = false;
    };

    using ActiveSubgrid = std::array<ActiveTileSlice, WorldCoordinates::Square::kVisibleGridDim * WorldCoordinates::Square::kVisibleGridDim>;
    ActiveSubgrid getActiveSubgrid() const;
    GLuint getOrUploadArrayTexture();
    sf::Vector2f getVisibleGridWorldOrigin() const;

private:

    static bool loadTileFromDisk(const TerrainManifest& manifest,
                                  TileCoord coord,
                                  float* outBuffer);

    bool m_gridInitialized = false;

    TileCoord clampToWorldBounds(TileCoord coord) const;
    TileCoord worldPosToAbsoluteTileCoord(const sf::Vector2f& cameraWorldPos) const;
    void      initializeGrid(const sf::Vector2f& cameraWorldPos);
    void      loadTileIntoSlot(TileCoord coord);

    void checkBoundaryCrossing(const sf::Vector2f& cameraWorldPos);
    void refreshActiveSliceUniforms();
    void publishStagedTile(int slotIndex, TileCoord newCoord);

    struct LoadRequest  { int slotIndex; TileCoord coord; };
    struct LoadResult   { int slotIndex; bool success; };

    void workerThreadMain();
    void requestTileLoad(int slotIndex, TileCoord coord);
    void drainCompletionQueue();

    size_t tileAllocFloatCount() const;
    float* slotData(int slotIndex);
    const float* slotData(int slotIndex) const;

    TerrainManifest m_manifest;

    std::vector<float>     m_tileStorage;
    std::vector<TileCoord> m_slotWorldCoord;
    std::vector<bool>      m_slotValid;

    TileCoord m_centerTileCoord{};

    std::vector<float> m_stagingBuffer;

    std::thread              m_workerThread;
    std::mutex               m_requestQueueMutex;
    std::queue<LoadRequest>  m_requestQueue;
    std::mutex               m_completionQueueMutex;
    std::queue<LoadResult>   m_completionQueue;
    std::condition_variable  m_workerWakeCV;
    std::atomic<bool>        m_shutdownRequested{false};

    std::array<GLint, 81> m_activeSliceUniforms{};
    GLuint m_arrayTexture = 0;
    bool   m_subgridDirty = true;
};