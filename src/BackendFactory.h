#pragma once

#include "Backend.h"
#include <memory>
#include <string>

/// Configuration for backend creation
struct BackendConfig {
    enum class Type { Fake, Http };
    Type type{Type::Http};
};

/// A complete set of backends, one per layer
struct BackendSet {
    std::unique_ptr<Backend> gps;
    std::unique_ptr<Backend> photo;
    std::unique_ptr<Backend> calendar;
    std::unique_ptr<Backend> googleTimeline;

    /// Cancel all in-progress fetches across all backends
    void cancelAll();

    /// Get a backend by layer index (0=gps, 1=photo, 2=calendar, 3=googleTimeline)
    Backend* byIndex(int i);
};

/// Create a full set of backends from configuration.
/// url is only used when config.type == Http.
BackendSet createBackends(const BackendConfig& config, const std::string& url = "");
