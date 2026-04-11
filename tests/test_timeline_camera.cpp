#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TimelineCamera.h"

TEST_CASE("TimelineCamera screenToTime roundtrip at center", "[timeline_camera]") {
    TimelineCamera cam;
    cam.setSize(800, 200);
    cam.setCenter(1700000000.0);

    // Center pixel should map to center time
    float centerPx = 400.0f;
    double t = cam.screenToTime(centerPx);
    REQUIRE(t == Catch::Approx(1700000000.0).margin(1.0));
}

TEST_CASE("TimelineCamera screenToTime/timeToScreen roundtrip", "[timeline_camera]") {
    TimelineCamera cam;
    cam.setSize(1000, 200);
    cam.setCenter(1700000000.0);

    double testTime = 1700000000.0 + 3600.0;  // 1 hour ahead
    float px = cam.timeToScreen(testTime);
    double recovered = cam.screenToTime(px);

    REQUIRE(recovered == Catch::Approx(testTime).margin(0.1));
}

TEST_CASE("TimelineCamera getTimeExtent", "[timeline_camera]") {
    TimelineCamera cam;
    cam.setSize(800, 200);
    cam.setCenter(1700000000.0);

    TimeExtent ext = cam.getTimeExtent();
    REQUIRE(ext.start < 1700000000.0);
    REQUIRE(ext.end > 1700000000.0);
    REQUIRE(ext.contains(1700000000.0));
    REQUIRE(ext.duration() > 0.0);
}

TEST_CASE("TimelineCamera zoomAtPixel changes zoom", "[timeline_camera]") {
    TimelineCamera cam;
    cam.setSize(800, 200);
    cam.setCenter(1700000000.0);

    double zoomBefore = cam.zoom();
    cam.zoomAtPixel(400.0f, 3.0f);  // zoom in
    double zoomAfter = cam.zoom();

    REQUIRE(zoomAfter < zoomBefore);  // zooming in = smaller zoom value
}

TEST_CASE("TimelineCamera panByPixels shifts center", "[timeline_camera]") {
    TimelineCamera cam;
    cam.setSize(800, 200);
    cam.setCenter(1700000000.0);

    double before = cam.center();
    cam.panByPixels(100.0f);  // pan right = earlier in time
    double after = cam.center();

    REQUIRE(after < before);
}
