#pragma once

/// Geographic bounds for the map view
struct SpatialExtent {
    double min_lat = 34.0;   // Default: LA area
    double max_lat = 34.1;
    double min_lon = -118.3;
    double max_lon = -118.2;

    double lat_span() const { return max_lat - min_lat; }
    double lon_span() const { return max_lon - min_lon; }
};

/// Central application state
/// Future: Will contain TimeExtent, Layers, selection state
class AppModel {
public:
    SpatialExtent spatial_extent;  // Geographic bounds for map view

    // Future additions:
    // TimeExtent time_extent;
    // std::vector<std::unique_ptr<Layer>> layers;
    // Entity* selected_entity;
};
