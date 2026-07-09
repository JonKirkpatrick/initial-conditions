#pragma once

#include "SoAEntityManager.hpp"
#include "ComponentTypes.hpp"

#include <array>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <limits>

class EntityManager;

using EntityVec = std::vector<SoAEntityHandle>;
using EntityMap = std::map<std::string, EntityVec>;
using SpawnCallback = std::function<void(SoAEntityHandle, EntityManager&)>;

struct SpawnRequest {
    std::string tag;
    SpawnCallback initializer;
};

class EntityManager
{
    // Packed active entity lists. Both containers stay dense via swap-remove.
    EntityVec                                   m_activeEntities;
    EntityMap                                   m_entitiesByTag;

    // Dense-slot lookup tables for O(1) swap-remove from packed vectors.
    std::array<uint32_t, soa::MAX_ENTITIES>     m_entityToDenseSlot;
    std::array<uint32_t, soa::MAX_ENTITIES>     m_tagToDenseSlot;
    std::vector<std::string>                    m_entityTags; // entity index -> tag

    // SoA integration
    SoAEntityPool           m_soaPool;

    // SoA component storages (fixed-size per-entity arrays)
    soa::ComponentArray<CTransform3D>           m_compTransform;
    soa::ComponentArray<CPhysics>               m_compPhysics;
    soa::ComponentArray<CBob>                   m_compBob;
    soa::ComponentArray<CPlayer>                m_compPlayer;
    soa::ComponentArray<CCamera>                m_compCamera;
    soa::ComponentArray<CInput>                 m_compInput;
    soa::ComponentArray<COrb>                   m_compOrb;
    soa::ComponentArray<CEyes>                  m_compEyes;

    // The Command Buffers
    std::vector<SpawnRequest>                   m_spawnQueue;
    std::vector<SoAEntityHandle>                m_destroyQueue;

    static constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

    void initDenseIndexArrays()
    {
        m_entityToDenseSlot.fill(INVALID_INDEX);
        m_tagToDenseSlot.fill(INVALID_INDEX);
    }

    void swapRemoveFromEntityList(SoAEntityHandle h)
    {
        const uint32_t denseIndex = m_entityToDenseSlot[h.index];
        if (denseIndex == INVALID_INDEX || denseIndex >= m_activeEntities.size()) return;

        const uint32_t lastIndex = static_cast<uint32_t>(m_activeEntities.size() - 1);
        if (denseIndex != lastIndex)
        {
            const SoAEntityHandle moved = m_activeEntities[lastIndex];
            m_activeEntities[denseIndex] = moved;
            m_entityToDenseSlot[moved.index] = denseIndex;
        }

        m_activeEntities.pop_back();
        m_entityToDenseSlot[h.index] = INVALID_INDEX;
    }

    void swapRemoveFromTagList(SoAEntityHandle h)
    {
        const std::string& tag = m_entityTags[h.index];
        auto it = m_entitiesByTag.find(tag);
        if (it == m_entitiesByTag.end()) return;

        EntityVec& vec = it->second;
        const uint32_t denseIndex = m_tagToDenseSlot[h.index];
        if (denseIndex == INVALID_INDEX || denseIndex >= vec.size()) return;

        const uint32_t lastIndex = static_cast<uint32_t>(vec.size() - 1);
        if (denseIndex != lastIndex)
        {
            const SoAEntityHandle moved = vec[lastIndex];
            vec[denseIndex] = moved;
            m_tagToDenseSlot[moved.index] = denseIndex;
        }

        vec.pop_back();
        m_tagToDenseSlot[h.index] = INVALID_INDEX;

        if (vec.empty())
        {
            m_entitiesByTag.erase(it);
        }
    }

    void sUpdateTransformVectors()
    {
        m_compTransform.each([](uint32_t entIndex, CTransform3D& transform) {
            if (!transform.isDirty()) return; // Early-exit for static entities!

            const glm::quat& q = transform.orientation();

            // Rotate the default cardinal axes by our orientation quaternion
            glm::vec3 f = q * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 r = q * glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 u = q * glm::vec3(0.0f, 1.0f, 0.0f);

            // Store back in the cached slots and clear the dirty flag
            transform.setCachedVectors({f.x, f.y, f.z}, {r.x, r.y, r.z}, {u.x, u.y, u.z});
            transform.clean();
        });
    }

public:

    EntityManager()
    {
        initDenseIndexArrays();
        m_entityTags.resize(soa::MAX_ENTITIES);
    }

    void queueSpawn(const std::string& tag, SpawnCallback initializer) {
        m_spawnQueue.push_back({ tag, std::move(initializer) });
    }

    void queueDestroy(SoAEntityHandle h) {
        m_destroyQueue.push_back(h);
    }

    void update()
    {
        // Process destroy queue
        for (const auto& h : m_destroyQueue)
        {
            destroyEntity(h);
        }
        m_destroyQueue.clear();

        for (const auto& request : m_spawnQueue) {
            SoAEntityHandle h = addEntity(request.tag);
            if (request.initializer) {
                request.initializer(h, *this);
            }
        }
        m_spawnQueue.clear();
        sUpdateTransformVectors();
    }

    // Create a new entity, return its SoA handle and register tag
    SoAEntityHandle addEntity(const std::string& tag)
    {
        auto h = m_soaPool.create();
        if (h.index >= m_entityTags.size()) m_entityTags.resize(soa::MAX_ENTITIES);
        m_entityTags[h.index] = tag;

        m_entityToDenseSlot[h.index] = static_cast<uint32_t>(m_activeEntities.size());
        m_activeEntities.push_back(h);

        EntityVec& tagVec = m_entitiesByTag[tag];
        m_tagToDenseSlot[h.index] = static_cast<uint32_t>(tagVec.size());
        tagVec.push_back(h);
        return h;
    }

    // Destroy an entity immediately: remove components and free handle
    void destroyEntity(SoAEntityHandle h)
    {
        if (!m_soaPool.valid(h)) return;
        if (m_compTransform.has(h)) m_compTransform.remove(h);
        if (m_compPhysics.has(h)) m_compPhysics.remove(h);
        if (m_compBob.has(h)) m_compBob.remove(h);
        if (m_compPlayer.has(h)) m_compPlayer.remove(h);
        if (m_compCamera.has(h)) m_compCamera.remove(h);
        if (m_compInput.has(h)) m_compInput.remove(h);
        if (m_compOrb.has(h)) m_compOrb.remove(h);
        if (m_compEyes.has(h)) m_compEyes.remove(h);
        swapRemoveFromTagList(h);
        swapRemoveFromEntityList(h);

        m_soaPool.destroy(h);
        if (h.index < m_entityTags.size()) m_entityTags[h.index].clear();
    }

    // Component shims (SoA handle based)

    // Transform
    void addTransform(SoAEntityHandle h, const CTransform3D& t) { if (!m_soaPool.valid(h)) return; auto v = t; m_compTransform.add(h, v); }
    bool hasTransform(SoAEntityHandle h) const { return m_compTransform.has(h); }
    CTransform3D& getTransform(SoAEntityHandle h) { return m_compTransform.get(h); }
    const CTransform3D& getTransform(SoAEntityHandle h) const { return m_compTransform.get(h); }
    void removeTransform(SoAEntityHandle h) { m_compTransform.remove(h); }
    template<typename F> void forEachTransform(F&& f) { m_compTransform.each([this,&f](uint32_t entIndex, CTransform3D& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Physics
    void addPhysics(SoAEntityHandle h, const CPhysics& p) { if (!m_soaPool.valid(h)) return; auto v = p; m_compPhysics.add(h, v); }
    bool hasPhysics(SoAEntityHandle h) const { return m_compPhysics.has(h); }
    CPhysics& getPhysics(SoAEntityHandle h) { return m_compPhysics.get(h); }
    const CPhysics& getPhysics(SoAEntityHandle h) const { return m_compPhysics.get(h); }
    void removePhysics(SoAEntityHandle h) { m_compPhysics.remove(h); }
    template<typename F> void forEachPhysics(F&& f) { m_compPhysics.each([this,&f](uint32_t entIndex, CPhysics& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Bob
    void addBob(SoAEntityHandle h, const CBob& b) { if (!m_soaPool.valid(h)) return; auto v = b; m_compBob.add(h, v); }
    bool hasBob(SoAEntityHandle h) const { return m_compBob.has(h); }
    CBob& getBob(SoAEntityHandle h) { return m_compBob.get(h); }
    const CBob& getBob(SoAEntityHandle h) const { return m_compBob.get(h); }
    void removeBob(SoAEntityHandle h) { m_compBob.remove(h); }
    template<typename F> void forEachBob(F&& f) { m_compBob.each([this,&f](uint32_t entIndex, CBob& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Player
    void addPlayer(SoAEntityHandle h, const CPlayer& p) { if (!m_soaPool.valid(h)) return; auto v = p; m_compPlayer.add(h, v); }
    bool hasPlayer(SoAEntityHandle h) const { return m_compPlayer.has(h); }
    CPlayer& getPlayer(SoAEntityHandle h) { return m_compPlayer.get(h); }
    const CPlayer& getPlayer(SoAEntityHandle h) const { return m_compPlayer.get(h); }
    void removePlayer(SoAEntityHandle h) { m_compPlayer.remove(h); }
    template<typename F> void forEachPlayer(F&& f) { m_compPlayer.each([this,&f](uint32_t entIndex, CPlayer& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Camera
    void addCamera(SoAEntityHandle h, const CCamera& c) { if (!m_soaPool.valid(h)) return; auto v = c; m_compCamera.add(h, v); }
    bool hasCamera(SoAEntityHandle h) const { return m_compCamera.has(h); }
    CCamera& getCamera(SoAEntityHandle h) { return m_compCamera.get(h); }
    const CCamera& getCamera(SoAEntityHandle h) const { return m_compCamera.get(h); }
    void removeCamera(SoAEntityHandle h) { m_compCamera.remove(h); }
    template<typename F> void forEachCamera(F&& f) { m_compCamera.each([this,&f](uint32_t entIndex, CCamera& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Input
    void addInput(SoAEntityHandle h, const CInput& i) { if (!m_soaPool.valid(h)) return; auto v = i; m_compInput.add(h, v); }
    bool hasInput(SoAEntityHandle h) const { return m_compInput.has(h); }
    CInput& getInput(SoAEntityHandle h) { return m_compInput.get(h); }
    const CInput& getInput(SoAEntityHandle h) const { return m_compInput.get(h); }
    void removeInput(SoAEntityHandle h) { m_compInput.remove(h); }
    template<typename F> void forEachInput(F&& f) { m_compInput.each([this,&f](uint32_t entIndex, CInput& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Orb
    void addOrb(SoAEntityHandle h, const COrb& o) { if (!m_soaPool.valid(h)) return; auto v = o; m_compOrb.add(h, v); }
    bool hasOrb(SoAEntityHandle h) const { return m_compOrb.has(h); }
    COrb& getOrb(SoAEntityHandle h) { return m_compOrb.get(h); }
    const COrb& getOrb(SoAEntityHandle h) const { return m_compOrb.get(h); }
    void removeOrb(SoAEntityHandle h) { m_compOrb.remove(h); }
    template<typename F> void forEachOrb(F&& f) { m_compOrb.each([this,&f](uint32_t entIndex, COrb& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }
    template<typename F>
    void forEachOrbWithTransform(F&& func)
    {
        m_compOrb.each([&](uint32_t entIndex, COrb& orbData)
        {
            SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex);
            if (m_compTransform.has(h))
            {
                CTransform3D& transform = m_compTransform.get(h);
                func(h, transform, orbData);
            }
        });
    }

    // Eyes
    void addEyes(SoAEntityHandle h, const CEyes& e) { if (!m_soaPool.valid(h)) return; auto v = e; m_compEyes.add(h, v); }
    bool hasEyes(SoAEntityHandle h) const { return m_compEyes.has(h); }
    CEyes& getEyes(SoAEntityHandle h) { return m_compEyes.get(h); }
    const CEyes& getEyes(SoAEntityHandle h) const { return m_compEyes.get(h); }
    void removeEyes(SoAEntityHandle h) { m_compEyes.remove(h); }
    template<typename F> void forEachEyes(F&& f) { m_compEyes.each([this,&f](uint32_t entIndex, CEyes& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }
    template<typename F>
    void forEachOrbWithComponents(F&& func)
    {
        m_compOrb.each([&](uint32_t entIndex, COrb& orbData)
        {
            SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex);
            if (m_compTransform.has(h))
            {
                CTransform3D& transform = m_compTransform.get(h);
                CEyes*        eyes      = m_compEyes.has(h) ? &m_compEyes.get(h) : nullptr;
                func(h, transform, orbData, eyes);
            }
        });
    }
    const EntityVec& getEntities() const { return m_activeEntities; }
    const EntityVec& getEntities(const std::string& tag) const { return m_entitiesByTag.at(tag); }
    const EntityMap& getEntityMap() const { return m_entitiesByTag; }

    bool isSoAHandleValid(SoAEntityHandle h) const noexcept { return m_soaPool.valid(h); }
    const std::string& getTag(SoAEntityHandle h) const {
        static const std::string empty;
        if (!h.valid() || h.index >= m_entityTags.size()) return empty;
        return m_entityTags[h.index];
    }
};
