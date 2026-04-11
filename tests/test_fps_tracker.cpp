#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/FpsTracker.h"

TEST_CASE("FpsTracker reports zero before first window", "[fps_tracker]") {
    FpsTracker tracker;
    REQUIRE(tracker.fps() == 0.0);
    REQUIRE(tracker.frameMs() == 0.0);
}

TEST_CASE("FpsTracker computes correct FPS at 60Hz", "[fps_tracker]") {
    FpsTracker tracker;
    // 31 frames at 60fps exceeds the 0.5s window
    for (int i = 0; i < 31; ++i)
        tracker.tick(1.0 / 60.0);

    REQUIRE(tracker.fps() == Catch::Approx(60.0).epsilon(0.05));
    REQUIRE(tracker.frameMs() == Catch::Approx(1000.0 / 60.0).epsilon(0.5));
}

TEST_CASE("FpsTracker computes correct FPS at 30Hz", "[fps_tracker]") {
    FpsTracker tracker;
    // 16 frames at 30fps exceeds the 0.5s window
    for (int i = 0; i < 16; ++i)
        tracker.tick(1.0 / 30.0);

    REQUIRE(tracker.fps() == Catch::Approx(30.0).epsilon(0.05));
    REQUIRE(tracker.frameMs() == Catch::Approx(1000.0 / 30.0).epsilon(0.5));
}

TEST_CASE("FpsTracker resets after window", "[fps_tracker]") {
    FpsTracker tracker;

    // First window: 60fps
    for (int i = 0; i < 31; ++i)
        tracker.tick(1.0 / 60.0);
    REQUIRE(tracker.fps() == Catch::Approx(60.0).epsilon(0.05));

    // Second window: 30fps
    for (int i = 0; i < 16; ++i)
        tracker.tick(1.0 / 30.0);
    REQUIRE(tracker.fps() == Catch::Approx(30.0).epsilon(0.05));
}
