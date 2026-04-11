#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "EntityPicker.h"

static Entity makeEntity(double timeMid, double lon, double lat, float renderOffset = 0.0f) {
    Entity e;
    e.time_start = timeMid;
    e.time_end = timeMid;
    e.lon = lon;
    e.lat = lat;
    e.render_offset = renderOffset;
    return e;
}

static Entity makeTimeOnlyEntity(double timeMid, float renderOffset = 0.0f) {
    Entity e;
    e.time_start = timeMid;
    e.time_end = timeMid;
    e.render_offset = renderOffset;
    return e;
}

TEST_CASE("EntityPicker pickMap finds nearest entity", "[entity_picker]") {
    std::vector<Entity> entities = {
        makeEntity(1000.0, -118.25, 34.05),
        makeEntity(2000.0, -118.30, 34.10),
        makeEntity(3000.0,  151.21, -33.87),
    };

    EntityPicker picker;
    picker.rebuild(entities);

    int idx = picker.pickMap(-118.26, 34.06, 0.1);
    REQUIRE(idx == 0);  // closest to first entity
}

TEST_CASE("EntityPicker pickMap returns -1 when out of radius", "[entity_picker]") {
    std::vector<Entity> entities = {
        makeEntity(1000.0, -118.25, 34.05),
    };

    EntityPicker picker;
    picker.rebuild(entities);

    int idx = picker.pickMap(0.0, 0.0, 0.01);  // far away
    REQUIRE(idx == -1);
}

TEST_CASE("EntityPicker pickMap on empty picker", "[entity_picker]") {
    EntityPicker picker;
    REQUIRE(picker.pickMap(0.0, 0.0, 1.0) == -1);
}

TEST_CASE("EntityPicker pickTimeline finds nearest by time", "[entity_picker]") {
    std::vector<Entity> entities = {
        makeTimeOnlyEntity(1000.0, 0.0f),
        makeTimeOnlyEntity(2000.0, 0.0f),
        makeTimeOnlyEntity(3000.0, 0.0f),
    };

    EntityPicker picker;
    picker.rebuild(entities);

    int idx = picker.pickTimeline(1950.0, 0.0f, 200.0, 0.5f);
    REQUIRE(idx == 1);  // closest to 2000.0
}

TEST_CASE("EntityPicker pickTimeline returns -1 when out of radius", "[entity_picker]") {
    std::vector<Entity> entities = {
        makeTimeOnlyEntity(1000.0, 0.0f),
    };

    EntityPicker picker;
    picker.rebuild(entities);

    int idx = picker.pickTimeline(5000.0, 0.0f, 100.0, 0.5f);
    REQUIRE(idx == -1);
}

TEST_CASE("EntityPicker pickTimeline on empty picker", "[entity_picker]") {
    EntityPicker picker;
    REQUIRE(picker.pickTimeline(0.0, 0.0f, 100.0, 0.5f) == -1);
}

TEST_CASE("EntityPicker addEntities incrementally", "[entity_picker]") {
    std::vector<Entity> entities = {
        makeEntity(1000.0, -118.25, 34.05),
    };

    EntityPicker picker;
    picker.rebuild(entities);

    // Add a second entity
    entities.push_back(makeEntity(2000.0, -118.30, 34.10));
    picker.addEntities(entities, 1);

    // Both should be findable
    REQUIRE(picker.pickMap(-118.25, 34.05, 0.02) == 0);
    REQUIRE(picker.pickMap(-118.30, 34.10, 0.02) == 1);
}

TEST_CASE("EntityPicker pickTimeline considers renderOffset", "[entity_picker]") {
    std::vector<Entity> entities = {
        makeTimeOnlyEntity(1000.0, -0.5f),
        makeTimeOnlyEntity(1000.0,  0.5f),
    };

    EntityPicker picker;
    picker.rebuild(entities);

    // Query near renderOffset 0.5 should find entity 1
    int idx = picker.pickTimeline(1000.0, 0.4f, 200.0, 0.5f);
    REQUIRE(idx == 1);

    // Query near renderOffset -0.5 should find entity 0
    idx = picker.pickTimeline(1000.0, -0.4f, 200.0, 0.5f);
    REQUIRE(idx == 0);
}
