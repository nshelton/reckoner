#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Camera.h"

TEST_CASE("Camera screenToWorld roundtrip at center", "[camera]") {
    Camera cam;
    cam.setSize(800, 600);
    cam.setCenterAndZoom(Vec2(-118.25f, 34.05f), 0.1f);

    // Center of screen should map to camera center
    Vec2 world = cam.screenToWorld(Vec2(400.0f, 300.0f));
    REQUIRE(world.x == Catch::Approx(-118.25f).margin(0.01f));
    REQUIRE(world.y == Catch::Approx(34.05f).margin(0.01f));
}

TEST_CASE("Camera screenToWorld maps corners consistently", "[camera]") {
    Camera cam;
    cam.setSize(800, 600);
    cam.setCenterAndZoom(Vec2(0.0f, 0.0f), 1.0f);

    // Top-left pixel -> should be negative X (left), positive Y (top)
    Vec2 topLeft = cam.screenToWorld(Vec2(0.0f, 0.0f));
    REQUIRE(topLeft.y > 0.0f);  // top of screen = positive lat

    // Bottom-right pixel -> should be positive X, negative Y
    Vec2 botRight = cam.screenToWorld(Vec2(800.0f, 600.0f));
    REQUIRE(botRight.y < 0.0f);
}

TEST_CASE("Camera zoomAtPixel preserves anchor", "[camera]") {
    Camera cam;
    cam.setSize(800, 600);
    cam.setCenterAndZoom(Vec2(-118.25f, 34.05f), 0.1f);

    Vec2 anchorPx(200.0f, 150.0f);
    Vec2 worldBefore = cam.screenToWorld(anchorPx);

    cam.zoomAtPixel(anchorPx, 2.0f);  // zoom in

    Vec2 worldAfter = cam.screenToWorld(anchorPx);
    REQUIRE(worldAfter.x == Catch::Approx(worldBefore.x).margin(0.001f));
    REQUIRE(worldAfter.y == Catch::Approx(worldBefore.y).margin(0.001f));
}

TEST_CASE("Camera move shifts center", "[camera]") {
    Camera cam;
    cam.setSize(800, 600);
    cam.setCenterAndZoom(Vec2(0.0f, 0.0f), 1.0f);

    Vec2 before = cam.center();
    cam.move(Vec2(0.5f, 0.5f));
    Vec2 after = cam.center();

    REQUIRE(after.x == Catch::Approx(before.x + 0.5f).margin(0.01f));
    REQUIRE(after.y == Catch::Approx(before.y + 0.5f).margin(0.01f));
}

TEST_CASE("Camera zoom getter", "[camera]") {
    Camera cam;
    cam.setSize(800, 600);
    cam.setCenterAndZoom(Vec2(0.0f, 0.0f), 0.5f);
    REQUIRE(cam.zoom() == Catch::Approx(0.5f));
}
