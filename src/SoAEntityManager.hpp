#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <functional>
#include <limits>
#include <stdexcept>

// Packed SoA ECS primitives: entity handles, a pool, and dense component storage.

namespace soa {

    static constexpr std::size_t MAX_ENTITIES = 32768;

    struct EntityHandle {
        uint32_t index = std::numeric_limits<uint32_t>::max();
        uint32_t generation = 0;

        bool operator==(const EntityHandle& o) const noexcept {
            return index == o.index && generation == o.generation;
        }
        bool valid() const noexcept { return index != std::numeric_limits<uint32_t>::max(); }
    };

    class EntityPool {
        std::vector<uint32_t> freeList;
        std::array<uint32_t, MAX_ENTITIES> generation{};
        uint32_t capacity = static_cast<uint32_t>(MAX_ENTITIES);
        uint32_t alive = 0;

    public:
        EntityPool() {
            freeList.reserve(capacity);
            for (uint32_t i = 0; i < capacity; ++i) freeList.push_back(capacity - 1 - i);
            generation.fill(0);
        }

        EntityHandle create() {
            if (freeList.empty()) throw std::runtime_error("EntityPool exhausted");
            uint32_t idx = freeList.back(); freeList.pop_back();
            ++alive;
            return EntityHandle{ idx, generation[idx] };
        }

        void destroy(EntityHandle h) {
            if (h.index >= capacity) return;
            ++generation[h.index];
            freeList.push_back(h.index);
            if (alive > 0) --alive;
        }

        bool valid(EntityHandle h) const noexcept {
            if (!h.valid()) return false;
            if (h.index >= capacity) return false;
            return generation[h.index] == h.generation;
        }

        // Construct a current valid handle for a given index (reads current generation)
        EntityHandle handleFromIndex(uint32_t index) const noexcept {
            if (index >= capacity) return EntityHandle();
            return EntityHandle{ index, generation[index] };
        }

        uint32_t size() const noexcept { return alive; }
    };

    // Fixed-size packed component array.
    // Components live densely in index order and removals swap with the last live slot.
    template<typename T>
    class ComponentArray {
        std::vector<T> m_values;
        std::vector<EntityHandle> m_denseEntities;
        std::vector<uint32_t> m_denseSlotByEntity; 
        uint32_t m_liveCount = 0;

    public:
        ComponentArray() {
            m_values.resize(MAX_ENTITIES);
            m_denseEntities.resize(MAX_ENTITIES);
            
            m_denseSlotByEntity.assign(MAX_ENTITIES, std::numeric_limits<uint32_t>::max());
        }

        bool has(EntityHandle e) const noexcept {
            if (e.index >= MAX_ENTITIES) return false;
            const uint32_t denseIndex = m_denseSlotByEntity[e.index];
            if (denseIndex >= m_liveCount) return false;
            return m_denseEntities[denseIndex] == e;
        }

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

        T& get(EntityHandle e) {
            if (!has(e)) throw std::runtime_error("component not present");
            return m_values[m_denseSlotByEntity[e.index]];
        }

        const T& get(EntityHandle e) const {
            if (!has(e)) throw std::runtime_error("component not present");
            return m_values[m_denseSlotByEntity[e.index]];
        }

        template<typename F>
        void each(F&& f) {
            for (uint32_t i = 0; i < m_liveCount; ++i) {
                f(m_denseEntities[i].index, m_values[i]);
            }
        }

        std::size_t count() const noexcept {
            return m_liveCount;
        }
    };

} // namespace soa

// Convenience typedefs for user code
using SoAEntityHandle = soa::EntityHandle;
using SoAEntityPool = soa::EntityPool;