#pragma once

#include "core/Entity.h"
#include "core/TimeExtent.h"
#include "AppModel.h"
#include <vector>
#include <functional>

/// Backend interface for fetching entities
/// Implementations can be real HTTP backends or fake data generators
class Backend {
public:
    virtual ~Backend() = default;

    /// Fetch entities within the given spatiotemporal extent
    /// Calls the callback when data is ready (may be async)
    virtual void fetchEntities(
        const TimeExtent& time,
        const SpatialExtent& space,
        std::function<void(std::vector<Entity>&&)> callback
    ) = 0;
};
