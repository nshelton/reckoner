#include "HttpBackend.h"
#include "core/EnvLoader.h"
#include <iostream>

HttpBackend::HttpBackend(const std::string& base_url, const std::string& entity_type)
    : m_entityType(entity_type)
{
    // Load API key from env file
    auto env = EnvLoader::load("external/spatiotemporal_db/.env");
    std::string api_key = EnvLoader::get(env, "API_KEY");

    if (api_key.empty()) {
        std::cerr << "Warning: No API_KEY found in external/spatiotemporal_db/.env" << std::endl;
    } else {
        std::cout << "Loaded API key from env file (length: " << api_key.length() << ")" << std::endl;
    }

    m_api = std::make_unique<BackendAPI>(base_url, api_key);

    std::cout << "HttpBackend initialized with URL: " << base_url << std::endl;
    std::cout << "Entity type: " << entity_type << std::endl;
}

HttpBackend::HttpBackend(const std::string& base_url, const std::string& api_key, const std::string& entity_type)
    : m_api(std::make_unique<BackendAPI>(base_url, api_key))
    , m_entityType(entity_type)
{
    std::cout << "HttpBackend initialized with URL: " << base_url << std::endl;
    std::cout << "Entity type: " << entity_type << std::endl;
    if (!api_key.empty()) {
        std::cout << "Using explicit API key (length: " << api_key.length() << ")" << std::endl;
    }
}

void HttpBackend::fetchEntities(
    const TimeExtent& time,
    const SpatialExtent& space,
    std::function<void(std::vector<Entity>&&)> callback
) {
    std::cout << "HttpBackend: Fetching entities of type '" << m_entityType << "'" << std::endl;
    std::cout << "  Time: " << time.start << " to " << time.end << std::endl;
    std::cout << "  Space: [" << space.min_lat << ", " << space.min_lon
              << "] to [" << space.max_lat << ", " << space.max_lon << "]" << std::endl;

    try {
        // Fetch via bbox query (assumes entity type has spatial data)
        std::vector<Entity> entities = m_api->fetch_bbox(m_entityType, time, space);
        std::cout << "HttpBackend: Fetched " << entities.size() << " entities" << std::endl;
        callback(std::move(entities));
    } catch (const std::exception& e) {
        std::cerr << "HttpBackend: Fetch failed: " << e.what() << std::endl;
        // Return empty vector on error
        callback(std::vector<Entity>{});
    }
}
