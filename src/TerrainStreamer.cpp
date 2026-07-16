#include "TerrainStreamer.h"
#include "WorldCoordinates.hpp"

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
    manifest.worldOriginLatLon.x = latLonArr[0].get<double>(); // Latitude
    manifest.worldOriginLatLon.y = latLonArr[1].get<double>(); // Longitude

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
        std::cout << "[TerrainStreamer] Grid initialized at center tile: [" 
                  << m_centerTileCoord.row << ", " << m_centerTileCoord.col << "]\n";
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
    // Use the offset helper!
    m_centerTileCoord = clampToWorldBounds(worldPosToAbsoluteTileCoord(cameraWorldPos));

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
    TileCoord newCenter = clampToWorldBounds(worldPosToAbsoluteTileCoord(cameraWorldPos));
    if (newCenter == m_centerTileCoord)
    {
        return;
    }

    auto isInWindow = [](TileCoord coord, TileCoord center) {
        return std::abs(coord.row - center.row) <= kHalfWindow &&
               std::abs(coord.col - center.col) <= kHalfWindow;
    };

    std::vector<TileCoord> tilesToLoad;
    float gridDim = WorldCoordinates::Square::kStreamerGridDim;
    tilesToLoad.reserve(gridDim * gridDim);

    for (int row = newCenter.row - kHalfWindow; row <= newCenter.row + kHalfWindow; ++row)
    {
        for (int col = newCenter.col - kHalfWindow; col <= newCenter.col + kHalfWindow; ++col)
        {
            TileCoord target{row, col};
            if (!isInWindow(target, m_centerTileCoord))
            {
                tilesToLoad.push_back(target);
            }
        }
    }

    for (const auto& tile : tilesToLoad)
    {
        loadTileIntoSlot(tile);
    }

    m_centerTileCoord = newCenter;
    std::cout << "[TerrainStreamer] Center tile updated to [" << m_centerTileCoord.row
              << ", " << m_centerTileCoord.col << "]\n";
}

void TerrainStreamer::loadTileIntoSlot(TileCoord coord)
{
    const int slot = WorldCoordinates::Square::slotIndexForTile(coord);
    const bool ok  = loadTileFromDisk(m_manifest, coord, slotData(slot));

    m_slotWorldCoord[slot] = coord;
    m_slotValid[slot]      = ok;
    std::cout << "[TerrainStreamer] Loaded tile [" << coord.row << ", " << coord.col
              << "] into slot " << slot << " (valid: " << std::boolalpha << ok << ")\n";
}

bool TerrainStreamer::loadTileFromDisk(const TerrainManifest& manifest,
                                       TileCoord coord,
                                       float* outBuffer)
{
    using namespace WorldCoordinates::Square;
    
    const size_t floatsPerTile =
        static_cast<size_t>(kTileResolution + kApronTexels) *
        static_cast<size_t>(kTileResolution + kApronTexels);
    const size_t bytesPerTile = floatsPerTile * sizeof(float);

    std::string fileName = "tile_" + std::to_string(coord.row) + "_" + std::to_string(coord.col) + ".bin";
    std::filesystem::path tilePath = manifest.tileDirectory / fileName;

    if (!std::filesystem::exists(tilePath))
    {
        std::memset(outBuffer, 0, bytesPerTile);
        return true; 
    }

    std::ifstream file(tilePath, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "[TerrainStreamer] Error: Failed to open existing file: " << tilePath << "\n";
        std::memset(outBuffer, 0, bytesPerTile);
        return false;
    }

    file.read(reinterpret_cast<char*>(outBuffer), bytesPerTile);

    const std::streamsize bytesRead = file.gcount();
    if (static_cast<size_t>(bytesRead) != bytesPerTile)
    {
        std::cerr << "[TerrainStreamer] Warning: Incomplete tile read on " << tilePath 
                  << ". Expected " << bytesPerTile << " bytes, but read " << bytesRead << ".\n";
        
        std::memset(reinterpret_cast<char*>(outBuffer) + bytesRead, 0, bytesPerTile - bytesRead);
        return false;
    }

    return true;
}

const float* TerrainStreamer::getTileData(TileCoord coord) const
{
    const int slot = WorldCoordinates::Square::slotIndexForTile(coord);
    if (m_slotValid[slot] && m_slotWorldCoord[slot] == coord)
    {
        return slotData(slot);
    }
    return nullptr; // Not loaded, out of bounds, or catching up
}

TileCoord TerrainStreamer::getOriginTile() const
{
    return m_manifest.tileBoundsUpperLeft;
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