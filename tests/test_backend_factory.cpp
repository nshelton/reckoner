#include <catch2/catch_test_macros.hpp>
#include "BackendFactory.h"

TEST_CASE("BackendFactory creates fake backends", "[backend_factory]") {
    BackendConfig config{BackendConfig::Type::Fake};
    BackendSet set = createBackends(config);

    REQUIRE(set.gps != nullptr);
    REQUIRE(set.photo == nullptr);
    REQUIRE(set.calendar == nullptr);
    REQUIRE(set.googleTimeline == nullptr);
}

TEST_CASE("BackendFactory creates http backends", "[backend_factory]") {
    BackendConfig config{BackendConfig::Type::Http};
    BackendSet set = createBackends(config, "http://localhost:8000");

    REQUIRE(set.gps != nullptr);
    REQUIRE(set.photo != nullptr);
    REQUIRE(set.calendar != nullptr);
    REQUIRE(set.googleTimeline != nullptr);

    REQUIRE(set.gps->entityType() == "location.gps");
    REQUIRE(set.photo->entityType() == "photo");
    REQUIRE(set.calendar->entityType() == "calendar.event");
    REQUIRE(set.googleTimeline->entityType() == "location.googletimeline");
}

TEST_CASE("BackendSet byIndex returns correct backends", "[backend_factory]") {
    BackendConfig config{BackendConfig::Type::Http};
    BackendSet set = createBackends(config, "http://localhost:8000");

    REQUIRE(set.byIndex(0) == set.gps.get());
    REQUIRE(set.byIndex(1) == set.photo.get());
    REQUIRE(set.byIndex(2) == set.calendar.get());
    REQUIRE(set.byIndex(3) == set.googleTimeline.get());
    REQUIRE(set.byIndex(4) == nullptr);
    REQUIRE(set.byIndex(-1) == nullptr);
}

TEST_CASE("BackendSet cancelAll is safe on null backends", "[backend_factory]") {
    BackendConfig config{BackendConfig::Type::Fake};
    BackendSet set = createBackends(config);

    // Should not crash even though photo/calendar/googleTimeline are null
    set.cancelAll();
}
