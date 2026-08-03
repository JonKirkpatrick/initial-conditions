#pragma once

/**
 * @file EntityManager.hpp
 * @brief Central Structure of Arrays (SoA) Entity Component System (ECS) manager.
 * 
 * Manages entity creation, destruction queues, component storage arrays, and 
 * high-performance contiguous vector iteration.
 */

#include "ecs/SoAEntityManager.hpp"
#include "ecs/ComponentTypes.hpp"

#include <array>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <limits>
#include <functional>

class EntityManager;

/** @brief Type alias for dense vectors of entity handles. */
using EntityVec = std::vector<SoAEntityHandle>;

/** @brief Type alias mapping string tags to vectors of active entity handles. */
using EntityMap = std::map<std::string, EntityVec>;

/** @brief Callback function signature invoked during deferred entity initialization. */
using SpawnCallback = std::function<void(SoAEntityHandle, EntityManager&)>;

/**
 * @brief Structure encapsulating a deferred entity spawn request.
 */
struct SpawnRequest {
    std::string tag;           ///< Tag assigned to the spawned entity.
    SpawnCallback initializer; ///< Optional callback to configure components post-creation.
};

/**
 * @brief Primary manager for SoA entity pooling, component storage, and frame updates.
 * 
 * Uses dense component arrays and fast dense-slot lookup tables to maintain $O(1)$ swap-remove 
 * behavior during entity destruction while preserving dense memory layouts for iteration.
 */
class EntityManager
{
    // Packed active entity lists. Both containers stay dense via swap-remove.
    EntityVec                                   m_activeEntities; ///< Dense vector of all active entity handles.
    EntityMap                                   m_entitiesByTag;  ///< Map of active entity handles grouped by tag.

    // Dense-slot lookup tables for O(1) swap-remove from packed vectors.
    std::array<uint32_t, soa::MAX_ENTITIES>     m_entityToDenseSlot; ///< Index lookup table for active entity array position.
    std::array<uint32_t, soa::MAX_ENTITIES>     m_tagToDenseSlot;    ///< Index lookup table for tag vector position.
    std::vector<std::string>                    m_entityTags;        ///< Map from entity index to assigned tag string.

    // SoA integration
    SoAEntityPool           m_soaPool; ///< Allocation pool generating handles and generational IDs.

    // SoA component storages (fixed-size per-entity arrays)
    soa::ComponentArray<CTransform3D>           m_compTransform;    ///< Storage array for 3D transforms.
    soa::ComponentArray<CPhysics>               m_compPhysics;      ///< Storage array for physics attributes.
    soa::ComponentArray<CBob>                   m_compBob;          ///< Storage array for legacy bobbing state.
    soa::ComponentArray<CGaitCycle>             m_compGaitCycle;    ///< Storage array for gait presentation state.
    soa::ComponentArray<CKinematicBob>          m_compKinematicBob; ///< Storage array for kinematic bobbing parameters.
    soa::ComponentArray<CPlayer>                m_compPlayer;       ///< Storage array for local player tags.
    soa::ComponentArray<CCamera>                m_compCamera;       ///< Storage array for camera projections.
    soa::ComponentArray<CInput>                 m_compInput;        ///< Storage array for player input states.
    soa::ComponentArray<COrb>                   m_compOrb;          ///< Storage array for orb visual properties.
    soa::ComponentArray<CEyes>                  m_compEyes;         ///< Storage array for eye/gaze animation data.

    // The Command Buffers
    std::vector<SpawnRequest>                   m_spawnQueue;   ///< Command buffer for deferred entity spawning.
    std::vector<SoAEntityHandle>                m_destroyQueue; ///< Command buffer for deferred entity destruction.

    static constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max(); ///< Sentinel value for unmapped dense slots.

    /**
     * @brief Resets the dense slot mapping arrays to unmapped sentinel values.
     */
    void initDenseIndexArrays()
    {
        m_entityToDenseSlot.fill(INVALID_INDEX);
        m_tagToDenseSlot.fill(INVALID_INDEX);
    }

    /**
     * @brief Performs an $O(1)$ swap-remove operation on the global active entity list.
     * @param h Handle of the entity to remove.
     */
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

    /**
     * @brief Performs an $O(1)$ swap-remove operation on the tag-grouped entity vector.
     * @param h Handle of the entity to remove.
     */
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

public:

    /**
     * @brief Default constructor initializing lookup tables and reserving capacity.
     */
    EntityManager()
    {
        initDenseIndexArrays();
        m_entityTags.resize(soa::MAX_ENTITIES);
    }

    /**
     * @brief Queues a deferred entity creation request to be processed during update().
     * @param tag Logical tag assigned to the newly created entity.
     * @param initializer Optional callback invoked with handle and manager reference to attach components.
     */
    void queueSpawn(const std::string& tag, SpawnCallback initializer) {
        m_spawnQueue.push_back({ tag, std::move(initializer) });
    }

    /**
     * @brief Queues a deferred entity destruction request to be processed during update().
     * @param h Handle of the entity to mark for destruction.
     */
    void queueDestroy(SoAEntityHandle h) {
        m_destroyQueue.push_back(h);
    }

    /**
     * @brief System updating cached orientation basis vectors for all dirty 3D transforms.
     */
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

    /**
     * @brief Flushes deferred destruction and spawn command buffers.
     * 
     * Must be called at frame boundaries to finalize entity lifetimes.
     */
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
    }

    /**
     * @brief Immediately allocates a new entity handle and binds its tag.
     * @param tag String tag identifier.
     * @return Fresh `SoAEntityHandle`.
     */
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

    /**
     * @brief Immediately destroys an entity, purging components and recycling handle pool index.
     * @param h Handle of entity to destroy.
     */
    void destroyEntity(SoAEntityHandle h)
    {
        if (!m_soaPool.valid(h)) return;
        if (m_compTransform.has(h)) m_compTransform.remove(h);
        if (m_compPhysics.has(h)) m_compPhysics.remove(h);
        if (m_compBob.has(h)) m_compBob.remove(h);
        if (m_compGaitCycle.has(h)) m_compGaitCycle.remove(h);
        if (m_compKinematicBob.has(h)) m_compKinematicBob.remove(h);
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

    /// @name Component Management API Shims
    /// @{

    // Transform
    /** @brief Attaches a transform component to an entity. */
    void addTransform(SoAEntityHandle h, const CTransform3D& t) { if (!m_soaPool.valid(h)) return; auto v = t; m_compTransform.add(h, v); }
    /** @brief Checks if an entity possesses a transform component. */
    bool hasTransform(SoAEntityHandle h) const { return m_compTransform.has(h); }
    /** @brief Gets mutable transform component reference. */
    CTransform3D& getTransform(SoAEntityHandle h) { return m_compTransform.get(h); }
    /** @brief Gets immutable transform component reference. */
    const CTransform3D& getTransform(SoAEntityHandle h) const { return m_compTransform.get(h); }
    /** @brief Removes transform component from entity. */
    void removeTransform(SoAEntityHandle h) { m_compTransform.remove(h); }
    /** @brief Executes callable `f` for all active transform components. */
    template<typename F> void forEachTransform(F&& f) { m_compTransform.each([this,&f](uint32_t entIndex, CTransform3D& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Physics
    /** @brief Attaches a physics component to an entity. */
    void addPhysics(SoAEntityHandle h, const CPhysics& p) { if (!m_soaPool.valid(h)) return; auto v = p; m_compPhysics.add(h, v); }
    /** @brief Checks if an entity possesses a physics component. */
    bool hasPhysics(SoAEntityHandle h) const { return m_compPhysics.has(h); }
    /** @brief Gets mutable physics component reference. */
    CPhysics& getPhysics(SoAEntityHandle h) { return m_compPhysics.get(h); }
    /** @brief Gets immutable physics component reference. */
    const CPhysics& getPhysics(SoAEntityHandle h) const { return m_compPhysics.get(h); }
    /** @brief Removes physics component from entity. */
    void removePhysics(SoAEntityHandle h) { m_compPhysics.remove(h); }
    /** @brief Executes callable `f` for all active physics components. */
    template<typename F> void forEachPhysics(F&& f) { m_compPhysics.each([this,&f](uint32_t entIndex, CPhysics& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Bob
    /** @brief Attaches a legacy bob component to an entity. */
    void addBob(SoAEntityHandle h, const CBob& b) { if (!m_soaPool.valid(h)) return; auto v = b; m_compBob.add(h, v); }
    /** @brief Checks if an entity possesses a legacy bob component. */
    bool hasBob(SoAEntityHandle h) const { return m_compBob.has(h); }
    /** @brief Gets mutable legacy bob component reference. */
    CBob& getBob(SoAEntityHandle h) { return m_compBob.get(h); }
    /** @brief Gets immutable legacy bob component reference. */
    const CBob& getBob(SoAEntityHandle h) const { return m_compBob.get(h); }
    /** @brief Removes legacy bob component from entity. */
    void removeBob(SoAEntityHandle h) { m_compBob.remove(h); }
    /** @brief Executes callable `f` for all active legacy bob components. */
    template<typename F> void forEachBob(F&& f) { m_compBob.each([this,&f](uint32_t entIndex, CBob& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Gait Cycle
    /** @brief Attaches a gait cycle component to an entity. */
    void addGaitCycle(SoAEntityHandle h, const CGaitCycle& g) { if (!m_soaPool.valid(h)) return; auto v = g; m_compGaitCycle.add(h, v); }
    /** @brief Checks if an entity possesses a gait cycle component. */
    bool hasGaitCycle(SoAEntityHandle h) const { return m_compGaitCycle.has(h); }
    /** @brief Gets mutable gait cycle component reference. */
    CGaitCycle& getGaitCycle(SoAEntityHandle h) { return m_compGaitCycle.get(h); }
    /** @brief Gets immutable gait cycle component reference. */
    const CGaitCycle& getGaitCycle(SoAEntityHandle h) const { return m_compGaitCycle.get(h); }
    /** @brief Removes gait cycle component from entity. */
    void removeGaitCycle(SoAEntityHandle h) { m_compGaitCycle.remove(h); }
    /** @brief Executes callable `f` for all active gait cycle components. */
    template<typename F> void forEachGaitCycle(F&& f) { m_compGaitCycle.each([this,&f](uint32_t entIndex, CGaitCycle& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Kinematic Bob
    /** @brief Attaches a kinematic bob component to an entity. */
    void addKinematicBob(SoAEntityHandle h, const CKinematicBob& k) { if (!m_soaPool.valid(h)) return; auto v = k; m_compKinematicBob.add(h, v); }
    /** @brief Checks if an entity possesses a kinematic bob component. */
    bool hasKinematicBob(SoAEntityHandle h) const { return m_compKinematicBob.has(h); }
    /** @brief Gets mutable kinematic bob component reference. */
    CKinematicBob& getKinematicBob(SoAEntityHandle h) { return m_compKinematicBob.get(h); }
    /** @brief Gets immutable kinematic bob component reference. */
    const CKinematicBob& getKinematicBob(SoAEntityHandle h) const { return m_compKinematicBob.get(h); }
    /** @brief Removes kinematic bob component from entity. */
    void removeKinematicBob(SoAEntityHandle h) { m_compKinematicBob.remove(h); }
    /** @brief Executes callable `f` for all active kinematic bob components. */
    template<typename F> void forEachKinematicBob(F&& f) { m_compKinematicBob.each([this,&f](uint32_t entIndex, CKinematicBob& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Player
    /** @brief Attaches a player tag component to an entity. */
    void addPlayer(SoAEntityHandle h, const CPlayer& p) { if (!m_soaPool.valid(h)) return; auto v = p; m_compPlayer.add(h, v); }
    /** @brief Checks if an entity possesses a player component. */
    bool hasPlayer(SoAEntityHandle h) const { return m_compPlayer.has(h); }
    /** @brief Gets mutable player component reference. */
    CPlayer& getPlayer(SoAEntityHandle h) { return m_compPlayer.get(h); }
    /** @brief Gets immutable player component reference. */
    const CPlayer& getPlayer(SoAEntityHandle h) const { return m_compPlayer.get(h); }
    /** @brief Removes player component from entity. */
    void removePlayer(SoAEntityHandle h) { m_compPlayer.remove(h); }
    /** @brief Executes callable `f` for all active player components. */
    template<typename F> void forEachPlayer(F&& f) { m_compPlayer.each([this,&f](uint32_t entIndex, CPlayer& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Camera
    /** @brief Attaches a camera component to an entity. */
    void addCamera(SoAEntityHandle h, const CCamera& c) { if (!m_soaPool.valid(h)) return; auto v = c; m_compCamera.add(h, v); }
    /** @brief Checks if an entity possesses a camera component. */
    bool hasCamera(SoAEntityHandle h) const { return m_compCamera.has(h); }
    /** @brief Gets mutable camera component reference. */
    CCamera& getCamera(SoAEntityHandle h) { return m_compCamera.get(h); }
    /** @brief Gets immutable camera component reference. */
    const CCamera& getCamera(SoAEntityHandle h) const { return m_compCamera.get(h); }
    /** @brief Removes camera component from entity. */
    void removeCamera(SoAEntityHandle h) { m_compCamera.remove(h); }
    /** @brief Executes callable `f` for all active camera components. */
    template<typename F> void forEachCamera(F&& f) { m_compCamera.each([this,&f](uint32_t entIndex, CCamera& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Input
    /** @brief Attaches an input component to an entity. */
    void addInput(SoAEntityHandle h, const CInput& i) { if (!m_soaPool.valid(h)) return; auto v = i; m_compInput.add(h, v); }
    /** @brief Checks if an entity possesses an input component. */
    bool hasInput(SoAEntityHandle h) const { return m_compInput.has(h); }
    /** @brief Gets mutable input component reference. */
    CInput& getInput(SoAEntityHandle h) { return m_compInput.get(h); }
    /** @brief Gets immutable input component reference. */
    const CInput& getInput(SoAEntityHandle h) const { return m_compInput.get(h); }
    /** @brief Removes input component from entity. */
    void removeInput(SoAEntityHandle h) { m_compInput.remove(h); }
    /** @brief Executes callable `f` for all active input components. */
    template<typename F> void forEachInput(F&& f) { m_compInput.each([this,&f](uint32_t entIndex, CInput& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    // Orb
    /** @brief Attaches an orb visual component to an entity. */
    void addOrb(SoAEntityHandle h, const COrb& o) { if (!m_soaPool.valid(h)) return; auto v = o; m_compOrb.add(h, v); }
    /** @brief Checks if an entity possesses an orb component. */
    bool hasOrb(SoAEntityHandle h) const { return m_compOrb.has(h); }
    /** @brief Gets mutable orb component reference. */
    COrb& getOrb(SoAEntityHandle h) { return m_compOrb.get(h); }
    /** @brief Gets immutable orb component reference. */
    const COrb& getOrb(SoAEntityHandle h) const { return m_compOrb.get(h); }
    /** @brief Removes orb component from entity. */
    void removeOrb(SoAEntityHandle h) { m_compOrb.remove(h); }
    /** @brief Executes callable `f` for all active orb components. */
    template<typename F> void forEachOrb(F&& f) { m_compOrb.each([this,&f](uint32_t entIndex, COrb& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    /**
     * @brief Joins `COrb` and `CTransform3D` components, calling `func` for entities possessing both.
     * @tparam F Callable taking `(SoAEntityHandle, CTransform3D&, COrb&)`.
     * @param func User-provided function or lambda.
     */
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
    /** @brief Attaches an eye animation component to an entity. */
    void addEyes(SoAEntityHandle h, const CEyes& e) { if (!m_soaPool.valid(h)) return; auto v = e; m_compEyes.add(h, v); }
    /** @brief Checks if an entity possesses an eye component. */
    bool hasEyes(SoAEntityHandle h) const { return m_compEyes.has(h); }
    /** @brief Gets mutable eye component reference. */
    CEyes& getEyes(SoAEntityHandle h) { return m_compEyes.get(h); }
    /** @brief Gets immutable eye component reference. */
    const CEyes& getEyes(SoAEntityHandle h) const { return m_compEyes.get(h); }
    /** @brief Removes eye component from entity. */
    void removeEyes(SoAEntityHandle h) { m_compEyes.remove(h); }
    /** @brief Executes callable `f` for all active eye components. */
    template<typename F> void forEachEyes(F&& f) { m_compEyes.each([this,&f](uint32_t entIndex, CEyes& data){ SoAEntityHandle h = m_soaPool.handleFromIndex(entIndex); f(h, data); }); }

    /**
     * @brief Complex multi-component join querying entities with `COrb` and `CTransform3D`, plus optional `CEyes`.
     * @tparam F Callable taking `(SoAEntityHandle, CTransform3D&, COrb&, CEyes*)`.
     * @param func User-provided function or lambda.
     */
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

    /// @}

    /// @name Entity Query API
    /// @{

    /**
     * @brief Gets all currently active entity handles.
     * @return Const reference to dense active vector.
     */
    const EntityVec& getEntities() const { return m_activeEntities; }

    /**
     * @brief Gets active entity handles sharing a specific tag.
     * @param tag String tag identifier.
     * @return Const reference to tag-grouped entity vector.
     */
    const EntityVec& getEntities(const std::string& tag) const 
    { 
        static const EntityVec empty;
        auto it = m_entitiesByTag.find(tag);
        return (it != m_entitiesByTag.end()) ? it->second : empty;
     }

    /**
     * @brief Gets map of all active entities grouped by tag.
     * @return Const reference to tag map.
     */
    const EntityMap& getEntityMap() const { return m_entitiesByTag; }

    /**
     * @brief Validates handle generation and index against active pool bounds.
     * @param h Handle to check.
     * @return `true` if handle is valid and active, `false` otherwise.
     */
    bool isSoAHandleValid(SoAEntityHandle h) const noexcept { return m_soaPool.valid(h); }

    /**
     * @brief Retrieves string tag associated with an entity handle.
     * @param h Entity handle.
     * @return Const reference to tag string, or empty string if invalid.
     */
    const std::string& getTag(SoAEntityHandle h) const {
        static const std::string empty;
        if (!h.valid() || h.index >= m_entityTags.size()) return empty;
        return m_entityTags[h.index];
    }

    /// @}
};