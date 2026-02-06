#pragma once

#include "HttpClient.h"
#include "core/Entity.h"
#include "core/TimeExtent.h"
#include "AppModel.h"
#include <vector>
#include <string>

/// High-level API for fetching entities from the backend
class BackendAPI {
public:
    explicit BackendAPI(const std::string& base_url);

    /// Fetch entities via spatial + temporal query
    /// @param type Entity type (e.g., "location.gps")
    /// @param time_extent Temporal bounds
    /// @param spatial_extent Spatial bounds
    /// @param limit Maximum number of entities to return
    /// @return Vector of entities matching the query
    std::vector<Entity> fetch_bbox(
        const std::string& type,
        const TimeExtent& time_extent,
        const SpatialExtent& spatial_extent,
        int limit = 5000
    );

    /// Fetch entities via temporal-only query
    /// @param type Entity type (e.g., "calendar.event")
    /// @param time_extent Temporal bounds
    /// @param limit Maximum number of entities to return
    /// @return Vector of entities matching the query
    std::vector<Entity> fetch_time(
        const std::string& type,
        const TimeExtent& time_extent,
        int limit = 2000
    );

private:
    /// Parse entities from JSON response
    std::vector<Entity> parse_entities(const nlohmann::json& json_array);

    std::string base_url_;
    HttpClient http_client_;
};
