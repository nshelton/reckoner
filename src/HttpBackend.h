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

    void streamAllEntities(
        std::function<void(size_t total)> on_total,
        std::function<void(std::vector<Entity>&&)> batch_callback
    ) override;

    void streamAllByType(
        double startTime,
        double endTime,
        std::function<void(std::vector<Entity>&&)> batch_callback
    ) override;

    void cancelFetch() override { m_cancelled.store(true); }

    std::vector<uint8_t> fetchPhotoThumb(const std::string& entityId) override {
        return m_api->fetch_photo_thumb(entityId);
    }

    ServerStats fetchStats() override { return m_api->fetch_stats(); }

    const std::string& entityType() const override { return m_entityType; }

    /// The configured API key (for building URLs externally if needed).
    const std::string& apiKey() const { return m_api->apiKey(); }

private:
    std::unique_ptr<BackendAPI> m_api;
    std::string m_entityType;
    std::atomic<bool> m_cancelled{false};
};
