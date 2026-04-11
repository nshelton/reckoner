#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/Entity.h"

TEST_CASE("Entity time_mid", "[entity]") {
    Entity e;
    e.time_start = 100.0;
    e.time_end = 200.0;
    REQUIRE(e.time_mid() == Catch::Approx(150.0));
}

TEST_CASE("Entity duration", "[entity]") {
    Entity e;
    e.time_start = 100.0;
    e.time_end = 300.0;
    REQUIRE(e.duration() == Catch::Approx(200.0));
}

TEST_CASE("Entity is_instant", "[entity]") {
    Entity e;
    e.time_start = 100.0;
    e.time_end = 100.0;
    REQUIRE(e.is_instant());

    e.time_end = 101.0;
    REQUIRE_FALSE(e.is_instant());
}

TEST_CASE("Entity has_location", "[entity]") {
    Entity e;
    e.time_start = 0;
    e.time_end = 0;
    REQUIRE_FALSE(e.has_location());

    e.lat = 34.0;
    REQUIRE_FALSE(e.has_location());

    e.lon = -118.0;
    REQUIRE(e.has_location());
}

TEST_CASE("Entity spatial_contains", "[entity]") {
    Entity e;
    e.time_start = 0;
    e.time_end = 0;
    e.lat = 34.0;
    e.lon = -118.0;

    // Point right on top
    REQUIRE(e.spatial_contains(34.0, -118.0, 0.01));

    // Point within radius
    REQUIRE(e.spatial_contains(34.005, -118.005, 0.01));

    // Point outside radius
    REQUIRE_FALSE(e.spatial_contains(35.0, -118.0, 0.01));
}

TEST_CASE("Entity spatial_contains without location", "[entity]") {
    Entity e;
    e.time_start = 0;
    e.time_end = 0;
    REQUIRE_FALSE(e.spatial_contains(34.0, -118.0, 1.0));
}
