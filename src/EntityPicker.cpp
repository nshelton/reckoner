#include "EntityPicker.h"
#include <algorithm>
#include <cmath>
#include <limits>

void EntityPicker::rebuild(const std::vector<Entity>& entities)
{
    m_entities = &entities;
    m_mapGrid.clear();
    m_timeSorted.clear();
    m_timeSorted.reserve(entities.size());

    for (int i = 0; i < static_cast<int>(entities.size()); ++i) {
        const auto& e = entities[i];

        // Timeline index: all entities (time always present)
        m_timeSorted.push_back({e.time_mid(), i});

        // Map index: only entities with a location
        if (e.has_location()) {
            int32_t cx = static_cast<int32_t>(std::floor(*e.lon / MAP_CELL_SIZE));
            int32_t cy = static_cast<int32_t>(std::floor(*e.lat / MAP_CELL_SIZE));
            m_mapGrid[cellKey(cx, cy)].indices.push_back(i);
        }
    }

    std::sort(m_timeSorted.begin(), m_timeSorted.end());
}

void EntityPicker::addEntities(const std::vector<Entity>& entities, size_t fromIdx)
{
    m_entities = &entities;

    // Remember where the already-sorted portion ends
    auto mid = static_cast<ptrdiff_t>(m_timeSorted.size());

    for (size_t i = fromIdx; i < entities.size(); ++i) {
        const auto& e = entities[i];
        int idx = static_cast<int>(i);

        m_timeSorted.push_back({e.time_mid(), idx});

        if (e.has_location()) {
            int32_t cx = static_cast<int32_t>(std::floor(*e.lon / MAP_CELL_SIZE));
            int32_t cy = static_cast<int32_t>(std::floor(*e.lat / MAP_CELL_SIZE));
            m_mapGrid[cellKey(cx, cy)].indices.push_back(idx);
        }
    }

    // Sort the new chunk, then merge it with the already-sorted front in O(n)
    std::sort(m_timeSorted.begin() + mid, m_timeSorted.end());
    std::inplace_merge(m_timeSorted.begin(), m_timeSorted.begin() + mid, m_timeSorted.end());
}

int EntityPicker::pickMap(double lon, double lat, double radiusDeg) const
{
    if (!m_entities || m_mapGrid.empty()) return -1;

    int32_t cx = static_cast<int32_t>(std::floor(lon / MAP_CELL_SIZE));
    int32_t cy = static_cast<int32_t>(std::floor(lat / MAP_CELL_SIZE));
    // How many cells to check in each direction
    int32_t r = static_cast<int32_t>(std::ceil(radiusDeg / MAP_CELL_SIZE)) + 1;

    int bestIdx = -1;
    double bestDist2 = radiusDeg * radiusDeg;

    for (int32_t dy = -r; dy <= r; ++dy) {
        for (int32_t dx = -r; dx <= r; ++dx) {
            auto it = m_mapGrid.find(cellKey(cx + dx, cy + dy));
            if (it == m_mapGrid.end()) continue;

            for (int idx : it->second.indices) {
                const auto& e = (*m_entities)[idx];
                double dlon = *e.lon - lon;
                double dlat = *e.lat - lat;
                double d2 = dlon * dlon + dlat * dlat;
                if (d2 < bestDist2) {
                    bestDist2 = d2;
                    bestIdx = idx;
                }
            }
        }
    }

    return bestIdx;
}

int EntityPicker::pickTimeline(double time, float renderOffset,
                                double timeRadius, float yRadius) const
{
    if (!m_entities || m_timeSorted.empty()) return -1;

    // Binary search for the time window [time - timeRadius, time + timeRadius]
    auto lo = std::lower_bound(m_timeSorted.begin(), m_timeSorted.end(),
                               std::make_pair(time - timeRadius,
                                              std::numeric_limits<int>::min()));
    auto hi = std::upper_bound(m_timeSorted.begin(), m_timeSorted.end(),
                               std::make_pair(time + timeRadius,
                                              std::numeric_limits<int>::max()));

    int bestIdx = -1;
    double bestDist2 = 2.0; // normalized distance threshold > 1 means "none"

    for (auto it = lo; it != hi; ++it) {
        int idx = it->second;
        const auto& e = (*m_entities)[idx];

        // Normalize both axes: 1.0 = at the edge of the search radius
        double dt = (e.time_mid() - time) / timeRadius;
        double dy = (static_cast<double>(e.render_offset) - renderOffset) / yRadius;
        double d2 = dt * dt + dy * dy;

        if (d2 < bestDist2) {
            bestDist2 = d2;
            bestIdx = idx;
        }
    }

    return bestIdx;
}
