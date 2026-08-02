#pragma once

#include <GL/glew.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Uniforms {

namespace detail {

struct ShaderBindingEntry
{
    std::string cppNamespace;
    std::string cppSymbol;
    std::string glslName;
    GLint       location = 0;
};

inline std::string makeKey(const std::string& cppNamespace, const std::string& cppSymbol)
{
    return cppNamespace + "::" + cppSymbol;
}

class Registry
{
public:
    static Registry& instance()
    {
        static Registry registry;
        return registry;
    }

    GLint location(const char* cppNamespace, const char* cppSymbol)
    {
        ensureLoaded();
        const std::string key = makeKey(cppNamespace, cppSymbol);
        const auto it = m_locationByKey.find(key);
        if (it == m_locationByKey.end())
        {
            return 0;
        }
        return it->second;
    }

    std::string rewriteShaderLayoutLocations(const std::string& source, const std::string& targetName)
    {
        ensureLoaded();

        const auto it = m_targetLocations.find(targetName);
        if (it == m_targetLocations.end() || it->second.empty())
        {
            return source;
        }

        const auto& locationByGlslName = it->second;
        std::ostringstream rewritten;
        std::istringstream input(source);
        std::string line;

        const std::regex declarationRegex(R"((?:uniform|in|out)\s+.*\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]+\])?\s*;)");
        const std::regex locationRegex(R"(layout\s*\(\s*location\s*=\s*[^)]+\))");

        while (std::getline(input, line))
        {
            std::smatch match;
            if (std::regex_search(line, match, declarationRegex))
            {
                const std::string glslName = match[1].str();
                const auto locationIt = locationByGlslName.find(glslName);
                if (locationIt != locationByGlslName.end())
                {
                    line = std::regex_replace(line,
                                              locationRegex,
                                              "layout(location = " + std::to_string(locationIt->second) + ")");
                }
            }

            rewritten << line << '\n';
        }

        return rewritten.str();
    }

private:
    void ensureLoaded()
    {
        if (m_loaded)
        {
            return;
        }

        const std::array<std::filesystem::path, 2> candidates = {
            std::filesystem::path("shaders/shader_locations.json"),
            std::filesystem::path("bin/shaders/shader_locations.json")
        };

        for (const auto& candidate : candidates)
        {
            std::ifstream file(candidate);
            if (!file.is_open())
            {
                continue;
            }

            nlohmann::json data;
            try
            {
                file >> data;
            }
            catch (const nlohmann::json::parse_error& e)
            {
                std::cerr << "Shader location registry parse error (" << candidate.string()
                          << "): " << e.what() << std::endl;
                m_loaded = true;
                return;
            }

            std::vector<ShaderBindingEntry> sharedEntries;
            if (data.contains("shared") && data["shared"].is_array())
            {
                sharedEntries = parseEntries(data["shared"]);
            }

            for (const auto& entry : sharedEntries)
            {
                if (!entry.cppNamespace.empty() && !entry.cppSymbol.empty())
                {
                    m_locationByKey[makeKey(entry.cppNamespace, entry.cppSymbol)] = entry.location;
                }
            }

            if (!data.contains("targets") || !data["targets"].is_object())
            {
                std::cerr << "Shader location registry missing 'targets' object: "
                          << candidate.string() << std::endl;
                m_loaded = true;
                return;
            }

            std::unordered_map<std::string, GLint> sharedByGlsl;
            for (const auto& entry : sharedEntries)
            {
                sharedByGlsl[entry.glslName] = entry.location;
            }

            for (const auto& [targetName, entriesJson] : data["targets"].items())
            {
                if (!entriesJson.is_array())
                {
                    continue;
                }

                auto targetMap = sharedByGlsl;
                const auto entries = parseEntries(entriesJson);
                for (const auto& entry : entries)
                {
                    targetMap[entry.glslName] = entry.location;
                    if (!entry.cppNamespace.empty() && !entry.cppSymbol.empty())
                    {
                        m_locationByKey[makeKey(entry.cppNamespace, entry.cppSymbol)] = entry.location;
                    }
                }

                m_targetLocations[targetName] = std::move(targetMap);
            }

            m_loaded = true;
            return;
        }

        m_loaded = true;
    }

    static std::vector<ShaderBindingEntry> parseEntries(const nlohmann::json& entriesJson)
    {
        std::vector<ShaderBindingEntry> entries;
        if (!entriesJson.is_array())
        {
            return entries;
        }

        for (const auto& entryJson : entriesJson)
        {
            if (!entryJson.contains("glsl") || !entryJson.contains("location"))
            {
                continue;
            }

            ShaderBindingEntry entry;
            entry.glslName = entryJson.at("glsl").get<std::string>();
            entry.location = entryJson.at("location").get<GLint>();

            if (entryJson.contains("cppNamespace"))
            {
                entry.cppNamespace = entryJson.at("cppNamespace").get<std::string>();
            }
            if (entryJson.contains("cppSymbol"))
            {
                entry.cppSymbol = entryJson.at("cppSymbol").get<std::string>();
            }

            entries.push_back(std::move(entry));
        }

        return entries;
    }

    bool m_loaded = false;
    std::unordered_map<std::string, GLint> m_locationByKey;
    std::unordered_map<std::string, std::unordered_map<std::string, GLint>> m_targetLocations;
};

} // namespace detail

struct LocationRef
{
    const char* cppNamespace = nullptr;
    const char* cppSymbol = nullptr;

    operator GLint() const
    {
        return detail::Registry::instance().location(cppNamespace, cppSymbol);
    }
};

inline std::string rewriteShaderLayoutLocations(const std::string& source, const std::string& targetName)
{
    return detail::Registry::instance().rewriteShaderLayoutLocations(source, targetName);
}

#define UNIFORM_LOCATION(namespaceName, symbolName) \
    inline const ::Uniforms::LocationRef symbolName{#namespaceName, #symbolName}

namespace SkyShared {
    UNIFORM_LOCATION(SkyShared, Cubemap);
    UNIFORM_LOCATION(SkyShared, StarRotationMatrix);
    UNIFORM_LOCATION(SkyShared, UseSkyCubemap);
    UNIFORM_LOCATION(SkyShared, MoonTexture);
}

namespace Shadows {
    UNIFORM_LOCATION(Shadows, LightViewProj);
    UNIFORM_LOCATION(Shadows, TexelWorldSize);
    UNIFORM_LOCATION(Shadows, CascadeSplitDepths);
    UNIFORM_LOCATION(Shadows, ShadowMapArray);
}

namespace GBuffer {
    UNIFORM_LOCATION(GBuffer, GAlbedoTex);
    UNIFORM_LOCATION(GBuffer, GNormalTex);
    UNIFORM_LOCATION(GBuffer, GIndicesTex);
    UNIFORM_LOCATION(GBuffer, GRetroTex);
    UNIFORM_LOCATION(GBuffer, GDepthTex);
}

namespace SharedTerrain {
    UNIFORM_LOCATION(SharedTerrain, HeightMax);
    UNIFORM_LOCATION(SharedTerrain, TerrainGridWorldOrigin);
    UNIFORM_LOCATION(SharedTerrain, TerrainTileWorldSize);
    UNIFORM_LOCATION(SharedTerrain, TerrainHeightArray);
    UNIFORM_LOCATION(SharedTerrain, TerrainRoadArray);
    UNIFORM_LOCATION(SharedTerrain, TerrainSliceValid);
}

namespace Blit {
    UNIFORM_LOCATION(Blit, InputTex);
}

namespace MiniMap {
    UNIFORM_LOCATION(MiniMap, PlayerXZ);
    UNIFORM_LOCATION(MiniMap, WorldRadius);
    UNIFORM_LOCATION(MiniMap, SeaLevel);
}

namespace Terrain {
    UNIFORM_LOCATION(Terrain, CursorMode);
    UNIFORM_LOCATION(Terrain, HexSize);
    UNIFORM_LOCATION(Terrain, HoveredHex);
    UNIFORM_LOCATION(Terrain, GridColour);
    UNIFORM_LOCATION(Terrain, SeaLevel);
    UNIFORM_LOCATION(Terrain, TerrainDiffuseArray);
    UNIFORM_LOCATION(Terrain, TerrainNormalArray);
    UNIFORM_LOCATION(Terrain, DrawHexGrid);
}

namespace ShadowPass {
    UNIFORM_LOCATION(ShadowPass, LightViewProj);
}

namespace OrbCreature {
    UNIFORM_LOCATION(OrbCreature, DiffuseTex);
    UNIFORM_LOCATION(OrbCreature, NormalTex);
}

namespace Lighting {
    UNIFORM_LOCATION(Lighting, NightAmbientFloor);
    UNIFORM_LOCATION(Lighting, HeadlampIntensity);
    UNIFORM_LOCATION(Lighting, HeadlampRange);
    UNIFORM_LOCATION(Lighting, HeadlampEnabled);
    UNIFORM_LOCATION(Lighting, SSAOTex);
}

namespace SSAO {
    UNIFORM_LOCATION(SSAO, NoiseTex);
    UNIFORM_LOCATION(SSAO, Radius);
    UNIFORM_LOCATION(SSAO, Bias);
    UNIFORM_LOCATION(SSAO, NoiseScale);
    UNIFORM_LOCATION(SSAO, SampleCount);
    UNIFORM_LOCATION(SSAO, TopRight);
    UNIFORM_LOCATION(SSAO, TopLeft);
    UNIFORM_LOCATION(SSAO, BottomLeft);
    UNIFORM_LOCATION(SSAO, BottomRight);
    UNIFORM_LOCATION(SSAO, KernelSample);
}

namespace SSAOBlur {
    UNIFORM_LOCATION(SSAOBlur, SSAOInput);
}

namespace Ocean {
    UNIFORM_LOCATION(Ocean, Model);
    UNIFORM_LOCATION(Ocean, NormalMatrix);
    UNIFORM_LOCATION(Ocean, Time);
    UNIFORM_LOCATION(Ocean, NightAmbientFloor);
}

#undef UNIFORM_LOCATION

} // namespace Uniforms
