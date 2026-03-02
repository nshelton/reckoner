#pragma once

#include "core/Entity.h"
#include "core/Color.h"
#include <string>
#include <vector>
#include <chrono>

/// A layer corresponds to one entity type (e.g. "location.gps", "photo").
/// Each layer has its own entity storage, visibility, and color.
struct Layer {
    std::string name;  // entity type string, e.g. "location.gps"
    bool visible = true;
    Color color{1.0f, 0.0f, 0.0f, 0.3f};
    int colorMode = 0;            // 0=turbo colormap, 1=solid layer color
    float yOffset = 0.0f;         // NDC Y offset applied post-projection (screen-space shift)
    std::vector<Entity> entities;

    // Per-layer fetch state
    bool is_fetching = false;
    std::chrono::steady_clock::time_point last_fetch_start;

    void startFetch() {
        last_fetch_start = std::chrono::steady_clock::now();
        is_fetching = true;
    }

    void endFetch() {
        is_fetching = false;
    }
};
