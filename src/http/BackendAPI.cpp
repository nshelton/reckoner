#include "BackendAPI.h"
#include "core/TimeUtils.h"
#include <iostream>

BackendAPI::BackendAPI(const std::string& base_url)
    : base_url_(base_url)
{
}

std::vector<Entity> BackendAPI::fetch_bbox(
    const std::string& type,
    const TimeExtent& time_extent,
    const SpatialExtent& spatial_extent,
    int limit
) {
    // Build request according to design doc
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

    std::cout << "Fetching from " << base_url_ << "/v1/query/bbox" << std::endl;
    std::cout << "Request: " << request.dump(2) << std::endl;

    try {
        nlohmann::json response = http_client_.post(base_url_ + "/v1/query/bbox", request);
        return parse_entities(response["entities"]);
    } catch (const std::exception& e) {
        std::cerr << "Backend fetch failed: " << e.what() << std::endl;
        return {};
    }
}

std::vector<Entity> BackendAPI::fetch_time(
    const std::string& type,
    const TimeExtent& time_extent,
    int limit
) {
    // Build request according to design doc
    nlohmann::json request = {
        {"types", nlohmann::json::array({type})},
        {"start", TimeUtils::to_iso8601(time_extent.start)},
        {"end", TimeUtils::to_iso8601(time_extent.end)},
        {"limit", limit},
        {"resample", {{"method", "none"}}}
    };

    std::cout << "Fetching from " << base_url_ << "/v1/query/time" << std::endl;
    std::cout << "Request: " << request.dump(2) << std::endl;

    try {
        nlohmann::json response = http_client_.post(base_url_ + "/v1/query/time", request);
        return parse_entities(response["entities"]);
    } catch (const std::exception& e) {
        std::cerr << "Backend fetch failed: " << e.what() << std::endl;
        return {};
    }
}

std::vector<Entity> BackendAPI::parse_entities(const nlohmann::json& json_array) {
    std::vector<Entity> result;

    for (const auto& j : json_array) {
        Entity e;
        e.id = j["id"].get<std::string>();

        // Parse timestamps
        e.time_start = TimeUtils::parse_iso8601(j["t_start"].get<std::string>());

        if (j["t_end"].is_null()) {
            e.time_end = e.time_start;  // Instant event
        } else {
            e.time_end = TimeUtils::parse_iso8601(j["t_end"].get<std::string>());
        }

        // Parse optional location
        if (!j["lat"].is_null()) {
            e.lat = j["lat"].get<double>();
        }
        if (!j["lon"].is_null()) {
            e.lon = j["lon"].get<double>();
        }

        // Parse optional display fields
        if (!j["name"].is_null()) {
            e.name = j["name"].get<std::string>();
        }

        if (!j["render_offset"].is_null()) {
            e.render_offset = j["render_offset"].get<float>();
        }

        result.push_back(std::move(e));
    }

    std::cout << "Parsed " << result.size() << " entities" << std::endl;
    return result;
}
