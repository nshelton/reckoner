#pragma once

#include <string>

/// Identifies a specific entity within a specific layer; invalid if layerIndex < 0.
struct PickResult {
    int layerIndex{-1};
    int entityIndex{-1};
    bool valid() const { return layerIndex >= 0 && entityIndex >= 0; }
    bool operator==(const PickResult& o) const { return layerIndex == o.layerIndex && entityIndex == o.entityIndex; }
    bool operator!=(const PickResult& o) const { return !(*this == o); }
};

/// Formatting helpers for entity display (pure logic, no ImGui dependency).
namespace PickingLogic {

/// Format a Unix timestamp as "YYYY-MM-DD  HH:MM:SS UTC" (or without seconds).
std::string fmtTimestamp(double t, bool includeSeconds = true);

/// Format a duration in seconds as a human-readable string (e.g. "3h 15m").
std::string fmtDuration(double seconds);

/// Format latitude with N/S suffix.
std::string fmtLat(double lat);

/// Format longitude with E/W suffix.
std::string fmtLon(double lon);

} // namespace PickingLogic
