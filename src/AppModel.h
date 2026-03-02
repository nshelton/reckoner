#pragma once

#include "core/Entity.h"
#include "core/TimeExtent.h"
#include "core/Vec2.h"
#include "core/RingBuffer.h"
#include "Layer.h"
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
/// Contains layers (each with their own entities), extents, and performance stats
class AppModel {
public:
    SpatialExtent spatial_extent;  // Geographic bounds for map view
    TimeExtent time_extent{0.0, 1770348932};  // Default: 1970 - now

    // Layer storage — layers[0] = location.gps, layers[1] = photo
    std::vector<Layer> layers;

    // Loading progress
    std::atomic<size_t> total_expected{0};     // From server stats
    std::atomic<bool> initial_load_complete{false};

    // Stats tracking
    LatencyRingBuffer<50> fetch_latencies;  // Last 50 fetch latencies in ms

    AppModel() {
        // GPS layer — uses turbo colormap (colorMode=0)
        Layer gps;
        gps.name      = "location.gps";
        gps.colorMode = 0;  // turbo
        gps.color     = Color{1.0f, 1.0f, 1.0f, 0.5f};
        layers.push_back(std::move(gps));

        // Photo layer — solid gold (colorMode=1), shifted up 20% in timeline
        Layer photos;
        photos.name      = "photo";
        photos.colorMode = 1;  // solid
        photos.color     = Color{1.0f, 0.843f, 0.0f, 0.7f};
        photos.yOffset   = 0.4f;  // 20% of NDC height above GPS layer
        layers.push_back(std::move(photos));
    }

    // Convenience: find a layer by name (-1 if not found)
    int layerIndex(const std::string& name) const {
        for (int i = 0; i < (int)layers.size(); ++i)
            if (layers[i].name == name) return i;
        return -1;
    }
};
