#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "FakeBackend.h"

TEST_CASE("FakeBackend generates requested number of entities", "[fake_backend]") {
    FakeBackend backend(500, 0);

    TimeExtent time{1700000000.0, 1700100000.0};
    SpatialExtent space;
    space.min_lat = 33.0;
    space.max_lat = 35.0;
    space.min_lon = -119.0;
    space.max_lon = -117.0;

    std::vector<Entity> result;
    backend.fetchEntities(time, space, [&](std::vector<Entity>&& entities) {
        result = std::move(entities);
    });

    REQUIRE(result.size() == 500);
}

TEST_CASE("FakeBackend entities are within spatial extent", "[fake_backend]") {
    FakeBackend backend(100, 0);

    SpatialExtent space;
    space.min_lat = 33.0;
    space.max_lat = 35.0;
    space.min_lon = -119.0;
    space.max_lon = -117.0;
    TimeExtent time{1700000000.0, 1700100000.0};

    std::vector<Entity> result;
    backend.fetchEntities(time, space, [&](std::vector<Entity>&& entities) {
        result = std::move(entities);
    });

    for (const auto& e : result) {
        REQUIRE(e.has_location());
        REQUIRE(*e.lat >= space.min_lat);
        REQUIRE(*e.lat <= space.max_lat);
        REQUIRE(*e.lon >= space.min_lon);
        REQUIRE(*e.lon <= space.max_lon);
    }
}

TEST_CASE("FakeBackend entities are within time extent", "[fake_backend]") {
    FakeBackend backend(100, 0);

    TimeExtent time{1700000000.0, 1700100000.0};
    SpatialExtent space;
    space.min_lat = -90.0;
    space.max_lat = 90.0;
    space.min_lon = -180.0;
    space.max_lon = 180.0;

    std::vector<Entity> result;
    backend.fetchEntities(time, space, [&](std::vector<Entity>&& entities) {
        result = std::move(entities);
    });

    for (const auto& e : result) {
        REQUIRE(e.time_start >= time.start);
        REQUIRE(e.time_start <= time.end);
    }
}

TEST_CASE("FakeBackend default virtual methods are no-ops", "[fake_backend]") {
    FakeBackend backend(10, 0);

    // cancelFetch should not crash
    backend.cancelFetch();

    // fetchStats should return empty stats
    ServerStats stats = backend.fetchStats();
    REQUIRE(stats.total_entities == 0);

    // fetchPhotoThumb should return empty
    auto thumb = backend.fetchPhotoThumb("test_id");
    REQUIRE(thumb.empty());

    // entityType should return empty
    REQUIRE(backend.entityType().empty());

    // streamAllEntities should be a no-op
    bool called = false;
    backend.streamAllEntities(
        [](size_t) {},
        [&](std::vector<Entity>&&) { called = true; }
    );
    REQUIRE_FALSE(called);

    // streamAllByType should be a no-op
    called = false;
    backend.streamAllByType(0.0, 1.0, [&](std::vector<Entity>&&) { called = true; });
    REQUIRE_FALSE(called);
}
