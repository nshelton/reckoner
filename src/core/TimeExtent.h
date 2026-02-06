#pragma once

/// Temporal bounds for the timeline view
struct TimeExtent {
    double start;  // unix timestamp
    double end;

    double duration() const { return end - start; }
    bool contains(double t) const { return t >= start && t <= end; }
    double to_normalized(double t) const { return (t - start) / duration(); }
};
