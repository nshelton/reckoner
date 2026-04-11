#pragma once

#include "core/Entity.h"
#include "core/TimeExtent.h"
#include "AppModel.h"
#include <vector>
#include <string>
#include <functional>
#include <cstdint>

/// Stats returned by the /stats endpoint
struct ServerStats {
    int total_entities{0};
    std::vector<std::pair<std::string, int>> entities_by_type;
    std::string oldest_time;
    std::string newest_time;
    double db_size_mb{0.0};
    double uptime_seconds{0.0};
};

/// Backend interface for fetching entities.
/// Implementations can be real HTTP backends or fake data generators.
/// Methods beyond fetchEntities have default no-op implementations so that
/// simple backends (e.g. FakeBackend) don't need to override them.
class Backend {
public:
    virtual ~Backend() = default;

    virtual void fetchEntities(
        const TimeExtent& time,
        const SpatialExtent& space,
        std::function<void(std::vector<Entity>&&)> callback
    ) = 0;

    virtual void streamAllEntities(
        std::function<void(size_t total)> /*on_total*/,
        std::function<void(std::vector<Entity>&&)> /*batch_callback*/
    ) {}

    virtual void streamAllByType(
        double /*startTime*/,
        double /*endTime*/,
        std::function<void(std::vector<Entity>&&)> /*batch_callback*/
    ) {}

    virtual void cancelFetch() {}
    virtual std::vector<uint8_t> fetchPhotoThumb(const std::string& /*entityId*/) { return {}; }
    virtual ServerStats fetchStats() { return {}; }

    virtual const std::string& entityType() const {
        static const std::string empty;
        return empty;
    }
};
