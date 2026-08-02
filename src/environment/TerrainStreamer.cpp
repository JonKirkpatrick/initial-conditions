#include "environment/TerrainStreamer.h"
#include "core/WorldCoordinates.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>

using WorldCoordinates::Square::TileCoord;
using json = nlohmann::json;

namespace {
    constexpr int kHalfWindow = WorldCoordinates::Square::kStreamerGridDim / 2;
}

TerrainManifest TerrainManifest::loadFromFile(const std::filesystem::path& manifestPath)
{
    std::ifstream file(manifestPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open terrain manifest file: " + manifestPath.string());
    }

    json data;
    try
    {
        file >> data;
    }
    catch (const json::parse_error& e)
    {
        throw std::runtime_error("JSON parse error in manifest: " + std::string(e.what()));
    }

    TerrainManifest manifest;

    manifest.schemaVersion = data.at("schemaVersion").get<int>();
    manifest.name          = data.at("name").get<std::string>();

    auto latLonArr = data.at("worldOriginLatLon");
    if (!latLonArr.is_array() || latLonArr.size() != 2)
    {
        throw std::runtime_error("Field 'worldOriginLatLon' must be a JSON array of 2 doubles.");
    }
    manifest.worldOriginLatLon.x = latLonArr[0].get<double>();
    manifest.worldOriginLatLon.y = latLonArr[1].get<double>();

    auto ul = data.at("tileBoundsUpperLeft");
    manifest.tileBoundsUpperLeft.row = ul.at("row").get<int>();
    manifest.tileBoundsUpperLeft.col = ul.at("col").get<int>();

    auto lr = data.at("tileBoundsLowerRight");
    manifest.tileBoundsLowerRight.row = lr.at("row").get<int>();
    manifest.tileBoundsLowerRight.col = lr.at("col").get<int>();

    manifest.tileDirectory = std::filesystem::path(data.at("tileDirectory").get<std::string>());
    manifest.channels = data.at("channels").get<std::vector<std::string>>();

    return manifest;
}

// ---------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------

TerrainStreamer::TerrainStreamer(const std::filesystem::path& manifestPath)
    : m_manifest(TerrainManifest::loadFromFile(manifestPath))
{
    const size_t floatsPerTile = tileAllocFloatCount();
    const size_t slotCount     = WorldCoordinates::Square::kStreamerGridDim
                                * WorldCoordinates::Square::kStreamerGridDim;

    m_tileStorage.assign(floatsPerTile * slotCount, 0.0f);
    m_roadStorage.assign(floatsPerTile * slotCount, 1.0f);

    m_slotWorldCoord.assign(slotCount, TileCoord{});
    m_slotValid.assign(slotCount, false);

    using namespace WorldCoordinates::Square;
    constexpr int kTexSide = kTileResolution + kApronTexels;
    constexpr int kVisibleLayers = kVisibleGridDim * kVisibleGridDim;
    const size_t totalSubgridBytes = kVisibleLayers * floatsPerTile * sizeof(float);

    // 1. Heightfield Texture Array & PBO
    glGenTextures(1, &m_arrayTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_arrayTexture);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R32F, kTexSide, kTexSide, kVisibleLayers);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    m_packedSubgridData.resize(kVisibleLayers * floatsPerTile, 0.0f);

    glGenBuffers(1, &m_pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, totalSubgridBytes, nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    // 2. Road SDF Texture Array & PBO
    glGenTextures(1, &m_roadArrayTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_roadArrayTexture);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R32F, kTexSide, kTexSide, kVisibleLayers);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    m_packedRoadSubgridData.resize(kVisibleLayers * floatsPerTile, 1.0f);

    glGenBuffers(1, &m_roadPbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_roadPbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, totalSubgridBytes, nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    m_workerThread = std::thread(&TerrainStreamer::workerThreadMain, this);
}

TerrainStreamer::~TerrainStreamer()
{
    m_shutdownRequested = true;
    m_workerWakeCV.notify_all();

    if (m_workerThread.joinable())
    {
        m_workerThread.join();
    }

    if (m_arrayTexture != 0)     glDeleteTextures(1, &m_arrayTexture);
    if (m_pbo != 0)              glDeleteBuffers(1, &m_pbo);
    if (m_roadArrayTexture != 0) glDeleteTextures(1, &m_roadArrayTexture);
    if (m_roadPbo != 0)          glDeleteBuffers(1, &m_roadPbo);
}

// ---------------------------------------------------------------------
// Frame Update Loop
// ---------------------------------------------------------------------

void TerrainStreamer::update(const sf::Vector2f& cameraWorldPos)
{
    // Non-blocking consumption of queued load results from worker thread
    drainCompletionQueue();

    if (m_slotValid.empty() || !m_gridInitialized)
    {
        initializeGrid(cameraWorldPos);
        return;
    }

    checkBoundaryCrossing(cameraWorldPos);
}

TileCoord TerrainStreamer::clampToWorldBounds(TileCoord coord) const
{
    coord.row = std::clamp(coord.row,
                           m_manifest.tileBoundsUpperLeft.row,
                           m_manifest.tileBoundsLowerRight.row);
    coord.col = std::clamp(coord.col,
                           m_manifest.tileBoundsUpperLeft.col,
                           m_manifest.tileBoundsLowerRight.col);
    return coord;
}

TileCoord TerrainStreamer::worldPosToAbsoluteTileCoord(const sf::Vector2f& cameraWorldPos) const
{
    TileCoord localTile = WorldCoordinates::Square::worldPosToTileCoord(cameraWorldPos);
    TileCoord geoTile;
    geoTile.row = m_manifest.tileBoundsUpperLeft.row + localTile.row;
    geoTile.col = m_manifest.tileBoundsUpperLeft.col + localTile.col;
    return geoTile;
}

void TerrainStreamer::initializeGrid(const sf::Vector2f& cameraWorldPos)
{
    m_centerTileCoord = clampToWorldBounds(worldPosToAbsoluteTileCoord(cameraWorldPos));

    int expectedLoads = 0;
    for (int row = m_centerTileCoord.row - kHalfWindow;
         row <= m_centerTileCoord.row + kHalfWindow; ++row)
    {
        for (int col = m_centerTileCoord.col - kHalfWindow;
             col <= m_centerTileCoord.col + kHalfWindow; ++col)
        {
            TileCoord target{row, col};
            int slot = WorldCoordinates::Square::slotIndexForTile(target);
            requestTileLoad(slot, target);
            expectedLoads++;
        }
    }

    // Wait until the worker thread has pushed all expected initial results into m_completionQueue
    {
        std::unique_lock<std::mutex> lock(m_completionQueueMutex);
        m_initialLoadCV.wait(lock, [this, expectedLoads]() {
            return static_cast<int>(m_completionQueue.size()) >= expectedLoads || m_shutdownRequested.load();
        });
    }

    // Now drain all completed items into CPU memory on the main thread
    drainCompletionQueue();

    m_gridInitialized  = true;
    m_subgridDirty     = true;
    m_roadSubgridDirty = true;
    refreshActiveSliceUniforms();
}

void TerrainStreamer::checkBoundaryCrossing(const sf::Vector2f& cameraWorldPos)
{
    TileCoord newCenter = clampToWorldBounds(worldPosToAbsoluteTileCoord(cameraWorldPos));
    if (newCenter == m_centerTileCoord)
    {
        return;
    }

    auto isInWindow = [](TileCoord coord, TileCoord center) {
        return std::abs(coord.row - center.row) <= kHalfWindow &&
               std::abs(coord.col - center.col) <= kHalfWindow;
    };

    m_centerTileCoord = newCenter;

    for (int row = newCenter.row - kHalfWindow; row <= newCenter.row + kHalfWindow; ++row)
    {
        for (int col = newCenter.col - kHalfWindow; col <= newCenter.col + kHalfWindow; ++col)
        {
            TileCoord target{row, col};
            int slot = WorldCoordinates::Square::slotIndexForTile(target);

            // Re-request tile if slot doesn't match new target coordinates
            if (m_slotWorldCoord[slot] != target || !m_slotValid[slot])
            {
                m_slotValid[slot] = false;
                requestTileLoad(slot, target);
            }
        }
    }

    refreshActiveSliceUniforms();
    m_subgridDirty     = true;
    m_roadSubgridDirty = true;
}

bool TerrainStreamer::loadTileFromDisk(const TerrainManifest& manifest,
                                       TileCoord coord,
                                       float* outHeightBuffer,
                                       float* outRoadBuffer)
{
    using namespace WorldCoordinates::Square;
    
    const size_t floatsPerTile =
        static_cast<size_t>(kTileResolution + kApronTexels) *
        static_cast<size_t>(kTileResolution + kApronTexels);
    const size_t bytesPerTile = floatsPerTile * sizeof(float);

    std::memset(outHeightBuffer, 0, bytesPerTile);
    std::fill_n(outRoadBuffer, floatsPerTile, 1.0f);

    std::string heightFileName = "tile_" + std::to_string(coord.row) + "_" + std::to_string(coord.col) + ".bin";
    std::filesystem::path heightPath = manifest.tileDirectory / heightFileName;

    if (std::filesystem::exists(heightPath))
    {
        std::ifstream file(heightPath, std::ios::binary);
        if (file.is_open())
        {
            file.read(reinterpret_cast<char*>(outHeightBuffer), bytesPerTile);
        }
    }

    if (manifest.hasChannel("roads"))
    {
        std::string roadFileName = "road_" + std::to_string(coord.row) + "_" + std::to_string(coord.col) + ".bin";
        std::filesystem::path roadPath = manifest.tileDirectory / roadFileName;

        if (std::filesystem::exists(roadPath))
        {
            std::ifstream file(roadPath, std::ios::binary);
            if (file.is_open())
            {
                file.read(reinterpret_cast<char*>(outRoadBuffer), bytesPerTile);
            }
        }
    }

    return true;
}

// ---------------------------------------------------------------------
// Threading & Queue Processing
// ---------------------------------------------------------------------

void TerrainStreamer::requestTileLoad(int slotIndex, TileCoord coord)
{
    {
        std::lock_guard<std::mutex> lock(m_requestQueueMutex);
        m_requestQueue.push(LoadRequest{slotIndex, coord});
    }
    m_workerWakeCV.notify_one();
}

void TerrainStreamer::workerThreadMain()
{
    const size_t floatsPerTile = tileAllocFloatCount();

    while (true)
    {
        LoadRequest request{};
        {
            std::unique_lock<std::mutex> lock(m_requestQueueMutex);
            m_workerWakeCV.wait(lock, [this] {
                return m_shutdownRequested.load() || !m_requestQueue.empty();
            });

            if (m_shutdownRequested.load() && m_requestQueue.empty())
            {
                return;
            }

            request = m_requestQueue.front();
            m_requestQueue.pop();
        }

        LoadResult result;
        result.slotIndex = request.slotIndex;
        result.coord     = request.coord;
        result.heightBuffer.resize(floatsPerTile);
        result.roadBuffer.resize(floatsPerTile);

        result.success = loadTileFromDisk(m_manifest,
                                           request.coord,
                                           result.heightBuffer.data(),
                                           result.roadBuffer.data());

        {
            std::lock_guard<std::mutex> lock(m_completionQueueMutex);
            m_completionQueue.push(std::move(result));
        }
        
        // Signal the main thread (used during initializeGrid startup wait)
        m_initialLoadCV.notify_one();
    }
}

void TerrainStreamer::drainCompletionQueue()
{
    const size_t floatsPerTile = tileAllocFloatCount();
    const size_t bytesPerTile  = floatsPerTile * sizeof(float);

    bool processedAny = false;
    while (true)
    {
        LoadResult result{};
        {
            std::lock_guard<std::mutex> lock(m_completionQueueMutex);
            if (m_completionQueue.empty())
            {
                break;
            }

            result = std::move(m_completionQueue.front());
            m_completionQueue.pop();
        }

        std::memcpy(slotData(result.slotIndex), result.heightBuffer.data(), bytesPerTile);
        std::memcpy(slotRoadData(result.slotIndex), result.roadBuffer.data(), bytesPerTile);

        m_slotWorldCoord[result.slotIndex] = result.coord;
        m_slotValid[result.slotIndex]      = result.success;

        m_subgridDirty     = true;
        m_roadSubgridDirty = true;
        processedAny       = true;
    }

    if (processedAny)
    {
        m_initialLoadCV.notify_all();
    }
}

// ---------------------------------------------------------------------
// Slot storage & Texture Helpers
// ---------------------------------------------------------------------

const float* TerrainStreamer::getTileData(TileCoord coord) const
{
    const int slot = WorldCoordinates::Square::slotIndexForTile(coord);
    if (m_slotValid[slot] && m_slotWorldCoord[slot] == coord)
    {
        return slotData(slot);
    }
    return nullptr;
}

const float* TerrainStreamer::getRoadData(TileCoord coord) const
{
    const int slot = WorldCoordinates::Square::slotIndexForTile(coord);
    if (m_slotValid[slot] && m_slotWorldCoord[slot] == coord)
    {
        return slotRoadData(slot);
    }
    return nullptr;
}

TileCoord TerrainStreamer::getOriginTile() const
{
    return m_manifest.tileBoundsUpperLeft;
}

size_t TerrainStreamer::tileAllocFloatCount() const
{
    using namespace WorldCoordinates::Square;
    return static_cast<size_t>(kTileResolution + kApronTexels) *
           static_cast<size_t>(kTileResolution + kApronTexels);
}

float* TerrainStreamer::slotData(int slotIndex)
{
    return m_tileStorage.data() + static_cast<size_t>(slotIndex) * tileAllocFloatCount();
}

const float* TerrainStreamer::slotData(int slotIndex) const
{
    return m_tileStorage.data() + static_cast<size_t>(slotIndex) * tileAllocFloatCount();
}

float* TerrainStreamer::slotRoadData(int slotIndex)
{
    return m_roadStorage.data() + static_cast<size_t>(slotIndex) * tileAllocFloatCount();
}

const float* TerrainStreamer::slotRoadData(int slotIndex) const
{
    return m_roadStorage.data() + static_cast<size_t>(slotIndex) * tileAllocFloatCount();
}

TerrainStreamer::ActiveSubgrid TerrainStreamer::getActiveSubgrid() const
{
    using namespace WorldCoordinates::Square;
    constexpr int kHalfVisible = kVisibleGridDim / 2;

    ActiveSubgrid subgrid{};
    int i = 0;
    for (int row = m_centerTileCoord.row - kHalfVisible;
         row <= m_centerTileCoord.row + kHalfVisible; ++row)
    {
        for (int col = m_centerTileCoord.col - kHalfVisible;
             col <= m_centerTileCoord.col + kHalfVisible; ++col, ++i)
        {
            const TileCoord coord{row, col};
            const int slot = slotIndexForTile(coord);

            ActiveTileSlice& slice = subgrid[i];
            slice.coord = coord;

            if (m_slotValid[slot] && m_slotWorldCoord[slot] == coord)
            {
                slice.heightData = slotData(slot);
                slice.roadData   = slotRoadData(slot);
                slice.valid      = true;
            }
        }
    }
    return subgrid;
}

const std::array<GLint, 81>& TerrainStreamer::getActiveSliceUniforms() const
{
    return m_activeSliceUniforms;
}

void TerrainStreamer::refreshActiveSliceUniforms()
{
    using namespace WorldCoordinates::Square;
    constexpr int kHalfVisible = kVisibleGridDim / 2;

    int i = 0;
    for (int row = m_centerTileCoord.row - kHalfVisible;
         row <= m_centerTileCoord.row + kHalfVisible; ++row)
    {
        for (int col = m_centerTileCoord.col - kHalfVisible;
             col <= m_centerTileCoord.col + kHalfVisible; ++col, ++i)
        {
            const TileCoord coord{row, col};
            const int slot = slotIndexForTile(coord);
            const bool valid = m_slotValid[slot] && m_slotWorldCoord[slot] == coord;
            m_activeSliceUniforms[i] = valid ? 1 : 0;
        }
    }
}

GLuint TerrainStreamer::getOrUploadArrayTexture()
{
    if (!m_subgridDirty)
    {
        return m_arrayTexture;
    }

    using namespace WorldCoordinates::Square;
    constexpr int kTexSide = kTileResolution + kApronTexels;
    const size_t floatsPerTile = tileAllocFloatCount();
    const size_t bytesPerTile  = floatsPerTile * sizeof(float);
    static const std::vector<float> kZeroTile(floatsPerTile, 0.0f);

    const ActiveSubgrid subgrid = getActiveSubgrid();

    for (int layer = 0; layer < static_cast<int>(subgrid.size()); ++layer)
    {
        const ActiveTileSlice& slice = subgrid[layer];
        const float* src = (slice.valid && slice.heightData) ? slice.heightData : kZeroTile.data();

        float* dst = m_packedSubgridData.data() + (layer * floatsPerTile);
        std::memcpy(dst, src, bytesPerTile);
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, m_packedSubgridData.size() * sizeof(float), m_packedSubgridData.data(), GL_STREAM_DRAW);

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_arrayTexture);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                    0, 0, 0,
                    kTexSide, kTexSide, subgrid.size(),
                    GL_RED, GL_FLOAT, nullptr);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    m_subgridDirty = false;
    return m_arrayTexture;
}

GLuint TerrainStreamer::getOrUploadRoadArrayTexture()
{
    if (!m_roadSubgridDirty)
    {
        return m_roadArrayTexture;
    }

    using namespace WorldCoordinates::Square;
    constexpr int kTexSide = kTileResolution + kApronTexels;
    const size_t floatsPerTile = tileAllocFloatCount();
    const size_t bytesPerTile  = floatsPerTile * sizeof(float);
    static const std::vector<float> kDefaultRoadTile(floatsPerTile, 1.0f);

    const ActiveSubgrid subgrid = getActiveSubgrid();

    for (int layer = 0; layer < static_cast<int>(subgrid.size()); ++layer)
    {
        const ActiveTileSlice& slice = subgrid[layer];
        const float* src = (slice.valid && slice.roadData) ? slice.roadData : kDefaultRoadTile.data();

        float* dst = m_packedRoadSubgridData.data() + (layer * floatsPerTile);
        std::memcpy(dst, src, bytesPerTile);
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_roadPbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, m_packedRoadSubgridData.size() * sizeof(float), m_packedRoadSubgridData.data(), GL_STREAM_DRAW);

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_roadArrayTexture);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                    0, 0, 0,
                    kTexSide, kTexSide, subgrid.size(),
                    GL_RED, GL_FLOAT, nullptr);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    m_roadSubgridDirty = false;
    return m_roadArrayTexture;
}

sf::Vector2f TerrainStreamer::getVisibleGridWorldOrigin() const
{
    using namespace WorldCoordinates::Square;
    constexpr int kHalfVisible = kVisibleGridDim / 2;

    const TileCoord absoluteOrigin{ m_centerTileCoord.row - kHalfVisible,
                                     m_centerTileCoord.col - kHalfVisible };
    const TileCoord localOrigin{ absoluteOrigin.row - m_manifest.tileBoundsUpperLeft.row,
                                  absoluteOrigin.col - m_manifest.tileBoundsUpperLeft.col };

    constexpr float kTileSizeM = kTexelSizeM * kTileResolution;
    return sf::Vector2f(localOrigin.col * kTileSizeM, localOrigin.row * kTileSizeM);
}