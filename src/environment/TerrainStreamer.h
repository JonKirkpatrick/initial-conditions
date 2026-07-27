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
    
    // Quick helper to check manifest channels
    bool hasChannel(const std::string& channelName) const {
        for (const auto& c : channels) {
            if (c == channelName) return true;
        }
        return false;
    }
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
    const float* getRoadData(TileCoord coord) const; // Added for Road Access
    TileCoord getOriginTile() const;

    struct ActiveTileSlice
    {
        WorldCoordinates::Square::TileCoord coord;
        const float* heightData = nullptr;
        const float* roadData   = nullptr; // Added for Road Access
        bool         valid      = false;
    };

    using ActiveSubgrid = std::array<ActiveTileSlice, WorldCoordinates::Square::kVisibleGridDim * WorldCoordinates::Square::kVisibleGridDim>;
    ActiveSubgrid getActiveSubgrid() const;
    
    GLuint getOrUploadArrayTexture();     // Uploads Height GL_R32F
    GLuint getOrUploadRoadArrayTexture(); // Added: Uploads Road SDF GL_R32F / GL_R8
    
    sf::Vector2f getVisibleGridWorldOrigin() const;

private:

    static bool loadTileFromDisk(const TerrainManifest& manifest,
                                  TileCoord coord,
                                  float* outHeightBuffer,
                                  float* outRoadBuffer); // Updated signature

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
    
    // Height storage helpers
    float* slotData(int slotIndex);
    const float* slotData(int slotIndex) const;

    // Road storage helpers (Added)
    float* slotRoadData(int slotIndex);
    const float* slotRoadData(int slotIndex) const;

    TerrainManifest m_manifest;

    // CPU Cache Storage
    std::vector<float>     m_tileStorage; // Height data
    std::vector<float>     m_roadStorage; // Road SDF data (Added)
    std::vector<TileCoord> m_slotWorldCoord;
    std::vector<bool>      m_slotValid;

    TileCoord m_centerTileCoord{};

    // Async staging
    std::vector<float> m_stagingBuffer;     // Staging for Height
    std::vector<float> m_stagingRoadBuffer; // Staging for Roads (Added)

    std::thread              m_workerThread;
    std::mutex               m_requestQueueMutex;
    std::queue<LoadRequest>  m_requestQueue;
    std::mutex               m_completionQueueMutex;
    std::queue<LoadResult>   m_completionQueue;
    std::condition_variable  m_workerWakeCV;
    std::atomic<bool>        m_shutdownRequested{false};

    std::array<GLint, 81> m_activeSliceUniforms{};
    
    // OpenGL GPU Texture & PBO Objects
    GLuint m_arrayTexture     = 0; // Heightfield array (GL_TEXTURE_2D_ARRAY)
    GLuint m_pbo              = 0; // Heightfield PBO
    
    GLuint m_roadArrayTexture = 0; // Road SDF array (GL_TEXTURE_2D_ARRAY) [Added]
    GLuint m_roadPbo          = 0; // Road SDF PBO [Added]

    bool m_subgridDirty     = true;
    bool m_roadSubgridDirty = true; // [Added]

    std::vector<float> m_packedSubgridData;     // Packed 81 layers for Height
    std::vector<float> m_packedRoadSubgridData; // Packed 81 layers for Roads [Added]
};