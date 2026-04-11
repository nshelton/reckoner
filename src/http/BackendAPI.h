#pragma once

#include "HttpClient.h"
#include "core/Entity.h"
#include "core/TimeExtent.h"
#include "AppModel.h"
#include "Backend.h"
#include <vector>
#include <string>
#include <functional>

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

    /// Fetch the thumbnail image for a photo entity as raw JPEG/PNG bytes.
    /// Endpoint: GET /v1/photo/{entity_id}/thumb (X-API-Key header auth).
    std::vector<uint8_t> fetch_photo_thumb(const std::string& entity_id);

    /// The configured API key (for external URL construction if needed).
    const std::string& apiKey() const { return http_client_.apiKey(); }

    /// Stream all entities from GET /v1/query/export (NDJSON).
    /// First line: {"total": N} — calls on_total once.
    /// Subsequent lines: one entity JSON per line — calls on_entity for each.
    /// Return false from on_entity to cancel the stream early.
    void fetch_export(
        std::function<void(size_t total)> on_total,
        std::function<bool(Entity&&)> on_entity
    );

private:
    /// Parse a single entity from a JSON object
    Entity parse_entity(const nlohmann::json& j);

    /// Parse entities from JSON response
    std::vector<Entity> parse_entities(const nlohmann::json& json_array);

    std::string base_url_;
    HttpClient http_client_;
};
