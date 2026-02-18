#include "BackendAPI.h"
#include "core/TimeUtils.h"
#include <iostream>

BackendAPI::BackendAPI(const std::string& base_url, const std::string& api_key)
    : base_url_(base_url)
    , http_client_(api_key)
{
}

ServerStats BackendAPI::fetch_stats() {
    ServerStats stats;
    try {
        nlohmann::json response = http_client_.get(base_url_ + "/stats");

        stats.total_entities = response.value("total_entities", 0);
        stats.db_size_mb = response.value("database", nlohmann::json::object()).value("size_mb", 0.0);
        stats.uptime_seconds = response.value("uptime_seconds", 0.0);

        auto time_cov = response.value("time_coverage", nlohmann::json::object());
        if (!time_cov["oldest"].is_null()) stats.oldest_time = time_cov["oldest"].get<std::string>();
        if (!time_cov["newest"].is_null()) stats.newest_time = time_cov["newest"].get<std::string>();

        for (const auto& entry : response.value("entities_by_type", nlohmann::json::array())) {
            stats.entities_by_type.emplace_back(
                entry["type"].get<std::string>(),
                entry["count"].get<int>()
            );
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to fetch stats: " << e.what() << std::endl;
    }
    return stats;
}

std::vector<Entity> BackendAPI::fetch_bbox(
    const std::string& type,
    const TimeExtent& time_extent,
    const SpatialExtent& spatial_extent,
    int limit,
    const std::string& order
) {
    // Build request according to design doc
    // Note: API key is sent via HTTP header, not in the JSON body
    nlohmann::json request = {
        {"types", nlohmann::json::array({type})},  // single-element array
        {"bbox", nlohmann::json::array({
            spatial_extent.min_lon,
            spatial_extent.min_lat,
            spatial_extent.max_lon,
            spatial_extent.max_lat
        })},
        {"time", {
            {"start", TimeUtils::to_iso8601(time_extent.start)},
            {"end", TimeUtils::to_iso8601(time_extent.end)}
        }},
        {"limit", limit}
    };

    if (!order.empty()) {
        request["order"] = order;
    }

    try {
        nlohmann::json response = http_client_.post(base_url_ + "/v1/query/bbox", request);
        return parse_entities(response["entities"]);
    } catch (const std::exception& e) {
        std::cerr << "Backend fetch_bbox failed: " << e.what() << std::endl;
        return {};
    }
}

std::vector<Entity> BackendAPI::fetch_time(
    const std::string& type,
    const TimeExtent& time_extent,
    int limit,
    const std::string& order
) {
    // Build request according to design doc
    nlohmann::json request = {
        {"types", nlohmann::json::array({type})},
        {"start", TimeUtils::to_iso8601(time_extent.start)},
        {"end", TimeUtils::to_iso8601(time_extent.end)},
        {"limit", limit}
    };

    if (!order.empty()) {
        request["order"] = order;
    }

    try {
        nlohmann::json response = http_client_.post(base_url_ + "/v1/query/time", request);
        return parse_entities(response["entities"]);
    } catch (const std::exception& e) {
        std::cerr << "Backend fetch_time failed: " << e.what() << std::endl;
        return {};
    }
}

void BackendAPI::fetch_export(
    std::function<void(size_t)> on_total,
    std::function<bool(Entity&&)> on_entity)
{
    bool first_line = true;

    http_client_.get_stream(base_url_ + "/v1/query/export",
        [&](const std::string& line) -> bool {
            try {
                auto j = nlohmann::json::parse(line);

                if (first_line) {
                    first_line = false;
                    if (j.contains("total")) {
                        on_total(j["total"].get<size_t>());
                        return true;
                    }
                    // Unexpected first line — fall through and parse as entity
                }

                return on_entity(parse_entity(j));
            } catch (const nlohmann::json::exception& ex) {
                std::cerr << "Export JSON parse error: " << ex.what() << std::endl;
                return true;  // Skip bad line, keep streaming
            }
        });
}

Entity BackendAPI::parse_entity(const nlohmann::json& j) {
    Entity e;
    e.id = j["id"].get<std::string>();

    e.time_start = TimeUtils::parse_iso8601(j["t_start"].get<std::string>());
    if (j["t_end"].is_null()) {
        e.time_end = e.time_start;
    } else {
        e.time_end = TimeUtils::parse_iso8601(j["t_end"].get<std::string>());
    }

    if (!j["lat"].is_null()) e.lat = j["lat"].get<double>();
    if (!j["lon"].is_null()) e.lon = j["lon"].get<double>();
    if (!j["name"].is_null()) e.name = j["name"].get<std::string>();
    if (!j["render_offset"].is_null()) e.render_offset = j["render_offset"].get<float>();

    return e;
}

std::vector<Entity> BackendAPI::parse_entities(const nlohmann::json& json_array) {
    std::vector<Entity> result;
    result.reserve(json_array.size());
    for (const auto& j : json_array) {
        result.push_back(parse_entity(j));
    }
    return result;
}
