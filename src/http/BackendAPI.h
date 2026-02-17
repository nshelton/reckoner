#pragma once

#include "HttpClient.h"
#include "core/Entity.h"
#include "core/TimeExtent.h"
#include "AppModel.h"
#include <vector>
#include <string>

/// Stats returned by the /stats endpoint
struct ServerStats {
    int total_entities{0};
    std::vector<std::pair<std::string, int>> entities_by_type;
    std::string oldest_time;
    std::string newest_time;
    double db_size_mb{0.0};
    double uptime_seconds{0.0};
};

/// High-level API for fetching entities from the backend
class BackendAPI {
public:
    explicit BackendAPI(const std::string& base_url, const std::string& api_key = "");

    /// Fetch server statistics (GET /stats, no auth required)
    ServerStats fetch_stats();

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
        int limit = 5000,
        const std::string& order = ""
    );

    /// Fetch entities via temporal-only query
    /// @param type Entity type (e.g., "calendar.event")
    /// @param time_extent Temporal bounds
    /// @param limit Maximum number of entities to return
    /// @return Vector of entities matching the query
    std::vector<Entity> fetch_time(
        const std::string& type,
        const TimeExtent& time_extent,
        int limit = 2000,
        const std::string& order = ""
    );

private:
    /// Parse entities from JSON response
    std::vector<Entity> parse_entities(const nlohmann::json& json_array);

    std::string base_url_;
    HttpClient http_client_;
};
