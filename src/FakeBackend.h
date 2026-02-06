#pragma once

#include "Backend.h"
#include <random>
#include <chrono>
#include <thread>

/// Fake backend that generates random points within the spatial extent
/// Useful for testing rendering and performance before real backend is ready
class FakeBackend : public Backend {
public:
    FakeBackend(int num_points = 1000)
        : m_numPoints(num_points)
        , m_rng(std::random_device{}())
    {}

    void fetchEntities(
        const TimeExtent& time,
        const SpatialExtent& space,
        std::function<void(std::vector<Entity>&&)> callback
    ) override {
        // Simulate network latency
        std::this_thread::sleep_for(std::chrono::milliseconds(50 + (m_rng() % 100)));

        std::vector<Entity> entities;
        entities.reserve(m_numPoints);

        std::uniform_real_distribution<double> lat_dist(space.min_lat, space.max_lat);
        std::uniform_real_distribution<double> lon_dist(space.min_lon, space.max_lon);
        std::uniform_real_distribution<double> time_dist(time.start, time.end);

        for (int i = 0; i < m_numPoints; ++i) {
            Entity e;
            e.id = "fake_" + std::to_string(i);
            e.lat = lat_dist(m_rng);
            e.lon = lon_dist(m_rng);
            e.time_start = time_dist(m_rng);
            e.time_end = e.time_start;  // instant events
            entities.push_back(std::move(e));
        }

        callback(std::move(entities));
    }

    void setNumPoints(int n) { m_numPoints = n; }

private:
    int m_numPoints;
    std::mt19937 m_rng;
};
