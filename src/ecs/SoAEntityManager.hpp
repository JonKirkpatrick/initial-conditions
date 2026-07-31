#pragma once

/**
 * @file SoAEntityManager.hpp
 * @brief Structure of Arrays (SoA) packed ECS primitives and storage.
 * 
 * Provides generational entity handle generation, entity index recycling, and 
 * sparse-dense packed component storage arrays with contiguous cache locality.
 */

#include <cstdint>
#include <vector>
#include <array>
#include <functional>
#include <limits>
#include <stdexcept>

namespace soa {

    /** @brief Maximum capacity bound for total allocated entities in the system. */
    static constexpr std::size_t MAX_ENTITIES = 262144;

    /**
     * @brief Light 64-bit generational handle uniquely identifying an entity instance.
     * 
     * Combines a 32-bit array slot index with a 32-bit generation counter to detect 
     * stale references after entity recycling.
     */
    struct EntityHandle {
        uint32_t index = std::numeric_limits<uint32_t>::max(); ///< Flat array index slot.
        uint32_t generation = 0;                              ///< Generational ID for handle validation.

        /**
         * @brief Equality operator testing both index and generation match.
         * @param o Other entity handle to compare.
         * @return `true` if handles refer to the exact same entity instance.
         */
        bool operator==(const EntityHandle& o) const noexcept {
            return index == o.index && generation == o.generation;
        }

        /**
         * @brief Checks if handle represents a initialized index.
         * @return `true` if index is valid (not sentinel `uint32_t::max`), `false` otherwise.
         */
        bool valid() const noexcept { return index != std::numeric_limits<uint32_t>::max(); }
    };

    /**
     * @brief Entity allocation pool managing recycled index slots and generational IDs.
     */
    class EntityPool {
        std::vector<uint32_t> freeList;                         ///< LIFO stack of available recycled entity indices.
        std::array<uint32_t, MAX_ENTITIES> generation{};       ///< Generational counters per index slot.
        uint32_t capacity = static_cast<uint32_t>(MAX_ENTITIES);///< Hard upper limit of system entity capacity.
        uint32_t alive = 0;                                     ///< Total count of currently active entities.

    public:
        /**
         * @brief Constructs pool pre-filling the free index stack.
         */
        EntityPool() {
            freeList.reserve(capacity);
            for (uint32_t i = 0; i < capacity; ++i) freeList.push_back(capacity - 1 - i);
            generation.fill(0);
        }

        /**
         * @brief Allocates a new entity handle from the pool.
         * @return Fresh `EntityHandle` with current slot generation.
         * @throws std::runtime_error If the entity pool exhausts its capacity.
         */
        EntityHandle create() {
            if (freeList.empty()) throw std::runtime_error("EntityPool exhausted");
            uint32_t idx = freeList.back(); freeList.pop_back();
            ++alive;
            return EntityHandle{ idx, generation[idx] };
        }

        /**
         * @brief Recycles an entity index and increments its generation counter.
         * @param h Handle of entity to destroy.
         */
        void destroy(EntityHandle h) {
            if (h.index >= capacity) return;
            ++generation[h.index];
            freeList.push_back(h.index);
            if (alive > 0) --alive;
        }

        /**
         * @brief Validates handle generation against current pool state.
         * @param h Handle to validate.
         * @return `true` if handle matches active generation, `false` if stale or out-of-bounds.
         */
        bool valid(EntityHandle h) const noexcept {
            if (!h.valid()) return false;
            if (h.index >= capacity) return false;
            return generation[h.index] == h.generation;
        }

        /**
         * @brief Reconstructs the current active handle for a given entity index slot.
         * @param index Slot index.
         * @return Current valid `EntityHandle` at index, or invalid handle if out-of-bounds.
         */
        EntityHandle handleFromIndex(uint32_t index) const noexcept {
            if (index >= capacity) return EntityHandle();
            return EntityHandle{ index, generation[index] };
        }

        /**
         * @brief Gets total count of active entities in the pool.
         * @return Active entity count.
         */
        uint32_t size() const noexcept { return alive; }
    };

    /**
     * @brief Packed contiguous component storage array.
     * 
     * Implements a sparse-dense vector set. Components are stored densely in memory 
     * to maximize cache throughput. Removals perform an $O(1)$ swap-remove with the 
     * last active component slot.
     * 
     * @tparam T Component value type.
     */
    template<typename T>
    class ComponentArray {
        std::vector<T> m_values;                      ///< Contiguous dense array of component instances.
        std::vector<EntityHandle> m_denseEntities;   ///< Back-pointers from dense slots to owning entity handles.
        std::vector<uint32_t> m_denseSlotByEntity;    ///< Sparse lookup array mapping entity index to dense slot.
        uint32_t m_liveCount = 0;                     ///< Count of currently active components.

    public:
        /**
         * @brief Constructs fixed-size backing vectors.
         */
        ComponentArray() {
            m_values.resize(MAX_ENTITIES);
            m_denseEntities.resize(MAX_ENTITIES);
            
            m_denseSlotByEntity.assign(MAX_ENTITIES, std::numeric_limits<uint32_t>::max());
        }

        /**
         * @brief Checks if entity owns a component in this array.
         * @param e Target entity handle.
         * @return `true` if entity possesses component and handle generation matches, `false` otherwise.
         */
        bool has(EntityHandle e) const noexcept {
            if (e.index >= MAX_ENTITIES) return false;
            const uint32_t denseIndex = m_denseSlotByEntity[e.index];
            if (denseIndex >= m_liveCount) return false;
            return m_denseEntities[denseIndex] == e;
        }

        /**
         * @brief Attaches or updates a component value for an entity.
         * @param e Target entity handle.
         * @param value Component value instance to store.
         * @throws std::out_of_range If entity index exceeds bounds.
         * @throws std::runtime_error If component storage exceeds max entity capacity.
         */
        void add(EntityHandle e, const T& value) {
            if (e.index >= MAX_ENTITIES) throw std::out_of_range("entity index out of range");
            if (has(e)) {
                m_values[m_denseSlotByEntity[e.index]] = value;
                return;
            }

            const uint32_t denseIndex = m_liveCount;
            if (denseIndex >= MAX_ENTITIES) throw std::runtime_error("component array exhausted");
            m_denseEntities[denseIndex] = e;
            m_denseSlotByEntity[e.index] = denseIndex;
            m_values[denseIndex] = value;
            ++m_liveCount;
        }

        /**
         * @brief Removes a component from an entity via $O(1)$ swap-remove.
         * @param e Target entity handle.
         */
        void remove(EntityHandle e) {
            if (e.index >= MAX_ENTITIES) return;
            if (!has(e)) return;

            const uint32_t removeIndex = m_denseSlotByEntity[e.index];
            const uint32_t lastIndex = m_liveCount - 1;
            if (removeIndex != lastIndex) {
                m_values[removeIndex] = std::move(m_values[lastIndex]);
                m_denseEntities[removeIndex] = m_denseEntities[lastIndex];
                m_denseSlotByEntity[m_denseEntities[removeIndex].index] = removeIndex;
            }

            m_denseSlotByEntity[e.index] = std::numeric_limits<uint32_t>::max();
            m_values[lastIndex] = T();
            m_denseEntities[lastIndex] = EntityHandle();
            --m_liveCount;
        }

        /**
         * @brief Retrieves mutable reference to entity component.
         * @param e Target entity handle.
         * @return Mutable component reference.
         * @throws std::runtime_error If component is not attached to entity.
         */
        T& get(EntityHandle e) {
            if (!has(e)) throw std::runtime_error("component not present");
            return m_values[m_denseSlotByEntity[e.index]];
        }

        /**
         * @brief Retrieves immutable reference to entity component.
         * @param e Target entity handle.
         * @return Immutable component reference.
         * @throws std::runtime_error If component is not attached to entity.
         */
        const T& get(EntityHandle e) const {
            if (!has(e)) throw std::runtime_error("component not present");
            return m_values[m_denseSlotByEntity[e.index]];
        }

        /**
         * @brief Fast contiguous iteration over all live components in memory order.
         * @tparam F Callable taking `(uint32_t entityIndex, T& component)`.
         * @param f Function or lambda executed for each live component.
         */
        template<typename F>
        void each(F&& f) {
            for (uint32_t i = 0; i < m_liveCount; ++i) {
                f(m_denseEntities[i].index, m_values[i]);
            }
        }

        /**
         * @brief Gets total count of active components stored in array.
         * @return Live component count.
         */
        std::size_t count() const noexcept {
            return m_liveCount;
        }
    };

} // namespace soa

/// @name SoA Global Aliases
/// @{

/** @brief Global alias for SoA entity handles. */
using SoAEntityHandle = soa::EntityHandle;

/** @brief Global alias for SoA entity pools. */
using SoAEntityPool = soa::EntityPool;

/// @}