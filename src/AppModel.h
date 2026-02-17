#pragma once

#include "core/Entity.h"
#include "core/TimeExtent.h"
#include "core/Vec2.h"
#include "core/RingBuffer.h"
#include <vector>
#include <array>
#include <chrono>
#include <atomic>

/// Geographic bounds for the map view
struct SpatialExtent {
    double min_lat = 33.95;   // Default: Full LA area with data
    double max_lat = 34.20;
    double min_lon = -118.80;
    double max_lon = -118.10;

    double lat_span() const { return max_lat - min_lat; }
    double lon_span() const { return max_lon - min_lon; }

    Vec2 to_normalized(double lat, double lon) const {
        return Vec2(
            static_cast<float>((lon - min_lon) / lon_span()),
            static_cast<float>((lat - min_lat) / lat_span())
        );
    }
};

/// Central application state
/// Contains entities, extents, and performance stats
class AppModel {
public:
    SpatialExtent spatial_extent;  // Geographic bounds for map view
    TimeExtent time_extent{0.0, 1770348932};  // Default: 1970 - now

    // Entity storage - all entities loaded into memory
    std::vector<Entity> entities;

    // Loading progress
    std::atomic<size_t> total_expected{0};     // From server stats
    std::atomic<bool> initial_load_complete{false};

    // Stats tracking
    LatencyRingBuffer<50> fetch_latencies;  // Last 50 fetch latencies in ms
    std::chrono::steady_clock::time_point last_fetch_start;
    bool is_fetching = false;

    // Methods
    void startFetch() {
        last_fetch_start = std::chrono::steady_clock::now();
        is_fetching = true;
    }

    void endFetch() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - last_fetch_start);
        fetch_latencies.push(static_cast<float>(duration.count()));
        is_fetching = false;
    }

    // Future additions:
    // std::vector<std::unique_ptr<Layer>> layers;
    // Entity* selected_entity;
};
