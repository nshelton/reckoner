#pragma once

#include "Backend.h"
#include "http/BackendAPI.h"
#include <memory>
#include <atomic>
#include <functional>

/// Real HTTP backend that fetches entities from the API server
class HttpBackend : public Backend {
public:
    /// Create HTTP backend with base URL (e.g., "http://n3k0.local:8000")
    /// Automatically loads API key from .env
    explicit HttpBackend(const std::string& base_url, const std::string& entity_type = "location.gps");

    /// Create HTTP backend with explicit API key
    HttpBackend(const std::string& base_url, const std::string& api_key, const std::string& entity_type);

    void fetchEntities(
        const TimeExtent& time,
        const SpatialExtent& space,
        std::function<void(std::vector<Entity>&&)> callback
    ) override;

    /// Stream all entities from GET /v1/query/export, calling on_total once with
    /// the count from the first NDJSON line, then batch_callback for each chunk.
    /// Blocks until the stream ends or cancelFetch() is called.
    void streamAllEntities(
        std::function<void(size_t total)> on_total,
        std::function<void(std::vector<Entity>&&)> batch_callback
    );

    /// Cancel an in-progress fetchAllEntities loop
    void cancelFetch() { m_cancelled.store(true); }

    /// Fetch server statistics from /stats endpoint
    ServerStats fetchStats() { return m_api->fetch_stats(); }

    /// Set the entity type to fetch (e.g., "location.gps", "photo")
    void setEntityType(const std::string& type) { m_entityType = type; }

    /// Get the current entity type
    const std::string& entityType() const { return m_entityType; }

private:
    std::unique_ptr<BackendAPI> m_api;
    std::string m_entityType;
    std::atomic<bool> m_cancelled{false};
};
