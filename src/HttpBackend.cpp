#include "HttpBackend.h"
#include "core/EnvLoader.h"
#include <iostream>

HttpBackend::HttpBackend(const std::string& base_url, const std::string& entity_type)
    : m_entityType(entity_type)
{
    // Load API key from env file (try project root, then parent dir for build/)
    auto env = EnvLoader::load(".env");
    std::string api_key = EnvLoader::get(env, "API_KEY");
    if (api_key.empty()) {
        env = EnvLoader::load("../.env");
        api_key = EnvLoader::get(env, "API_KEY");
    }

    if (api_key.empty()) {
        std::cerr << "Warning: No API_KEY found in .env or ../.env" << std::endl;
    }

    m_api = std::make_unique<BackendAPI>(base_url, api_key);
}

HttpBackend::HttpBackend(const std::string& base_url, const std::string& api_key, const std::string& entity_type)
    : m_api(std::make_unique<BackendAPI>(base_url, api_key))
    , m_entityType(entity_type)
{
}

void HttpBackend::fetchEntities(
    const TimeExtent& time,
    const SpatialExtent& space,
    std::function<void(std::vector<Entity>&&)> callback
) {
    try {
        std::vector<Entity> entities = m_api->fetch_bbox(m_entityType, time, space);
        callback(std::move(entities));
    } catch (const std::exception& e) {
        std::cerr << "HttpBackend: Fetch failed: " << e.what() << std::endl;
        callback(std::vector<Entity>{});
    }
}
