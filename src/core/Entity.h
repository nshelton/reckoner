#pragma once

#include <string>
#include <optional>

/// Represents a spatiotemporal entity
/// Always has a time extent, optionally has a location
struct Entity {
    std::string id;

    // Time — always an extent, instants have start == end
    double time_start;
    double time_end;

    // Space — optional, some entities have no location
    std::optional<double> lat;
    std::optional<double> lon;

    // Display
    std::optional<std::string> name;
    float render_offset = 0.0f;  // vertical offset for timeline stacking

    // Helpers
    bool is_instant() const { return time_start == time_end; }
    double duration() const { return time_end - time_start; }
    double time_mid() const { return (time_start + time_end) / 2.0; }

    bool has_location() const { return lat.has_value() && lon.has_value(); }

    bool spatial_contains(double query_lat, double query_lon, double radius_deg) const {
        if (!has_location()) return false;
        double dlat = *lat - query_lat;
        double dlon = *lon - query_lon;
        return (dlat * dlat + dlon * dlon) <= (radius_deg * radius_deg);
    }
};
