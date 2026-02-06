#pragma once

struct SpatialExtent {
    double min_lat = 34.0;
    double max_lat = 34.1;
    double min_lon = -118.3;
    double max_lon = -118.2;

    double lat_span() const { return max_lat - min_lat; }
    double lon_span() const { return max_lon - min_lon; }
};

class AppModel {
public:
    SpatialExtent spatial_extent;

    AppModel() {
        // Default to LA area
        spatial_extent.min_lat = 34.0;
        spatial_extent.max_lat = 34.1;
        spatial_extent.min_lon = -118.3;
        spatial_extent.max_lon = -118.2;
    }
};
