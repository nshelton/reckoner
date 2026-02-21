#pragma once

#include "core/Entity.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

/// Spatial index for fast entity picking in both the map and timeline views.
/// Build once via rebuild() whenever entities change; query every frame.
class EntityPicker
{
public:
    // Cell size for the map spatial grid (degrees lat/lon).
    // Smaller = faster queries but more memory; 0.05° ≈ 5 km.
    static constexpr double MAP_CELL_SIZE = 0.05;

    /// Full rebuild from scratch. Call when the entity list is cleared/reloaded.
    /// O(n log n).
    void rebuild(const std::vector<Entity>& entities);

    /// Incrementally insert entities[fromIdx..end) into the existing index.
    /// O(batch_size) for the grid + O(n) for the merge — safe to call per chunk.
    void addEntities(const std::vector<Entity>& entities, size_t fromIdx);

    /// Find the nearest entity within radiusDeg of (lon, lat) in the map view.
    /// Returns the index into the entities vector, or -1 if none found.
    int pickMap(double lon, double lat, double radiusDeg) const;

    /// Find the nearest entity near (time, renderOffset) in the timeline view.
    /// timeRadius is in seconds; yRadius is in render_offset units ([-1,1] range).
    /// Returns the index into the entities vector, or -1 if none found.
    int pickTimeline(double time, float renderOffset,
                     double timeRadius, float yRadius) const;

    bool empty() const { return m_entities == nullptr || m_entities->empty(); }

private:
    struct GridCell { std::vector<int> indices; };

    const std::vector<Entity>* m_entities = nullptr;

    // Map: 2D flat hash grid in lat/lon space
    std::unordered_map<uint64_t, GridCell> m_mapGrid;

    // Timeline: entities sorted by time_mid for binary-search range queries
    std::vector<std::pair<double, int>> m_timeSorted; // (time_mid, entity_idx)

    static uint64_t cellKey(int32_t cx, int32_t cy)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32)
               | static_cast<uint32_t>(cy);
    }
};
