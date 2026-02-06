#include "HttpBackend.h"
#include <iostream>

HttpBackend::HttpBackend(const std::string& base_url, const std::string& entity_type)
    : m_api(std::make_unique<BackendAPI>(base_url))
    , m_entityType(entity_type)
{
    std::cout << "HttpBackend initialized with URL: " << base_url << std::endl;
    std::cout << "Entity type: " << entity_type << std::endl;
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
