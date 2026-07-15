#include "TerrainStreamer.h"
#include "WorldCoordinates.hpp"

#include <cstring>
#include <fstream>
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
    manifest.worldOriginLatLon.x = latLonArr[0].get<double>(); // Latitude
    manifest.worldOriginLatLon.y = latLonArr[1].get<double>(); // Longitude

    auto ul = data.at("tileBoundsUpperLeft");
    manifest.tileBoundsUpperLeft.row = ul.at("row").get<int>();
    manifest.tileBoundsUpperLeft.col = ul.at("col").get<int>();

    auto lr = data.at("tileBoundsLowerRight");
    manifest.tileBoundsLowerRight.row = lr.at("row").get<int>();
    manifest.tileBoundsLowerRight.col = lr.at("col").get<int>();

    manifest.tileDirectory = data.at("tileDirectory").get<std::filesystem::path>();

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
    m_slotWorldCoord.assign(slotCount, TileCoord{});
    m_slotValid.assign(slotCount, false);
    m_stagingBuffer.assign(floatsPerTile, 0.0f);

    // Worker thread intentionally not started yet -- Stage 7.1 runs
    // synchronously on the calling thread. Stage 7.4 will start it here.
}

TerrainStreamer::~TerrainStreamer()
{
    // No worker thread running yet in this stage; nothing to signal or
    // join. Left as a no-op deliberately rather than omitted, so the
    // shutdown sequence has a home to grow into once 7.4 lands:
    //
    // m_shutdownRequested = true;
    // m_workerWakeCV.notify_all();
    // if (m_workerThread.joinable()) m_workerThread.join();
}

// ---------------------------------------------------------------------
// Per-frame entry point
// ---------------------------------------------------------------------

void TerrainStreamer::update(const sf::Vector2f& cameraWorldPos)
{
    if (m_slotValid.empty() || !m_gridInitialized)
    {
        initializeGrid(cameraWorldPos);
        return;
    }

    checkBoundaryCrossing(cameraWorldPos);

    // Stage 7.4 will add: drainCompletionQueue();
}

// ---------------------------------------------------------------------
// Stage 7.1: synchronous toroidal indexing
// ---------------------------------------------------------------------

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

void TerrainStreamer::initializeGrid(const sf::Vector2f& cameraWorldPos)
{
    m_centerTileCoord = clampToWorldBounds(
        WorldCoordinates::Square::worldPosToTileCoord(cameraWorldPos));

    for (int row = m_centerTileCoord.row - kHalfWindow;
         row <= m_centerTileCoord.row + kHalfWindow; ++row)
    {
        for (int col = m_centerTileCoord.col - kHalfWindow;
             col <= m_centerTileCoord.col + kHalfWindow; ++col)
        {
            loadTileIntoSlot(TileCoord{row, col});
        }
    }
    m_gridInitialized = true;
}

void TerrainStreamer::checkBoundaryCrossing(const sf::Vector2f& cameraWorldPos)
{
    TileCoord newCenter = clampToWorldBounds(
        WorldCoordinates::Square::worldPosToTileCoord(cameraWorldPos));

    if (newCenter == m_centerTileCoord)
        return;

    while (m_centerTileCoord.row != newCenter.row)
    {
        const int step = (newCenter.row > m_centerTileCoord.row) ? 1 : -1;
        m_centerTileCoord.row += step;

        const int edgeRow = m_centerTileCoord.row + kHalfWindow * step;
        for (int col = m_centerTileCoord.col - kHalfWindow;
             col <= m_centerTileCoord.col + kHalfWindow; ++col)
        {
            loadTileIntoSlot(TileCoord{edgeRow, col});
        }
    }

    while (m_centerTileCoord.col != newCenter.col)
    {
        const int step = (newCenter.col > m_centerTileCoord.col) ? 1 : -1;
        m_centerTileCoord.col += step;

        const int edgeCol = m_centerTileCoord.col + kHalfWindow * step;
        for (int row = m_centerTileCoord.row - kHalfWindow;
             row <= m_centerTileCoord.row + kHalfWindow; ++row)
        {
            loadTileIntoSlot(TileCoord{row, edgeCol});
        }
    }
}

void TerrainStreamer::loadTileIntoSlot(TileCoord coord)
{
    const int slot = WorldCoordinates::Square::slotIndexForTile(coord);
    const bool ok  = loadTileFromDisk(m_manifest, coord, slotData(slot));

    m_slotWorldCoord[slot] = coord;
    m_slotValid[slot]      = ok;
}

// ---------------------------------------------------------------------
// Stage 7.2: pure "load tile from disk" -- stubbed for now.
// Zero-fills and always reports success so the allocation/indexing
// path above is exercisable end-to-end before real file I/O exists.
// ---------------------------------------------------------------------
bool TerrainStreamer::loadTileFromDisk(const TerrainManifest& /*manifest*/,
                                        TileCoord /*coord*/,
                                        float* outBuffer)
{
    using namespace WorldCoordinates::Square;
    const size_t floatsPerTile =
        static_cast<size_t>(kTileResolution + kApronTexels) *
        static_cast<size_t>(kTileResolution + kApronTexels);

    std::memset(outBuffer, 0, floatsPerTile * sizeof(float));
    return true;
}

// ---------------------------------------------------------------------
// Slot storage helpers
// ---------------------------------------------------------------------

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

// ---------------------------------------------------------------------
// Stage 6 placeholder
// ---------------------------------------------------------------------

const std::array<GLint, 25>& TerrainStreamer::getActiveSliceUniforms() const
{
    return m_activeSliceUniforms;
}