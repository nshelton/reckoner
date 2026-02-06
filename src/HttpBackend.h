#pragma once

#include "Backend.h"
#include "http/BackendAPI.h"
#include <memory>

/// Real HTTP backend that fetches entities from the API server
class HttpBackend : public Backend {
public:
    /// Create HTTP backend with base URL (e.g., "http://localhost:8000")
    explicit HttpBackend(const std::string& base_url, const std::string& entity_type = "location.gps");

    void fetchEntities(
        const TimeExtent& time,
        const SpatialExtent& space,
        std::function<void(std::vector<Entity>&&)> callback
    ) override;

    /// Set the entity type to fetch (e.g., "location.gps", "photo")
    void setEntityType(const std::string& type) { m_entityType = type; }

    /// Get the current entity type
    const std::string& entityType() const { return m_entityType; }

private:
    std::unique_ptr<BackendAPI> m_api;
    std::string m_entityType;
};
