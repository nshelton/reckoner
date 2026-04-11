#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/Mat3.h"

TEST_CASE("Mat3 identity", "[mat3]") {
    Mat3 m;
    Vec2 p(3.0f, 7.0f);
    Vec2 result = m.apply(p);
    REQUIRE(result.x == Catch::Approx(3.0f));
    REQUIRE(result.y == Catch::Approx(7.0f));
}

TEST_CASE("Mat3 ortho projection", "[mat3]") {
    Mat3 m;
    m.setOrtho(-10.0f, 10.0f, -5.0f, 5.0f);

    // Center should map to origin
    Vec2 center = m.apply(Vec2(0.0f, 0.0f));
    REQUIRE(center.x == Catch::Approx(0.0f));
    REQUIRE(center.y == Catch::Approx(0.0f));

    // Right edge should map to +1
    Vec2 right = m.apply(Vec2(10.0f, 0.0f));
    REQUIRE(right.x == Catch::Approx(1.0f));

    // Top should map to +1
    Vec2 top = m.apply(Vec2(0.0f, 5.0f));
    REQUIRE(top.y == Catch::Approx(1.0f));

    // Left edge -> -1
    Vec2 left = m.apply(Vec2(-10.0f, 0.0f));
    REQUIRE(left.x == Catch::Approx(-1.0f));
}

TEST_CASE("Mat3 apply/applyInverse roundtrip", "[mat3]") {
    Mat3 m;
    m.setOrtho(-118.5f, -118.3f, 34.0f, 34.2f);

    Vec2 original(-118.4f, 34.1f);
    Vec2 ndc = m.apply(original);
    Vec2 recovered = m.applyInverse(ndc);

    REQUIRE(recovered.x == Catch::Approx(original.x).margin(1e-4f));
    REQUIRE(recovered.y == Catch::Approx(original.y).margin(1e-4f));
}

TEST_CASE("Mat3 translation", "[mat3]") {
    Mat3 m = Mat3::translation(5.0f, 3.0f);
    Vec2 p(1.0f, 2.0f);
    Vec2 result = m.apply(p);
    REQUIRE(result.x == Catch::Approx(6.0f));
    REQUIRE(result.y == Catch::Approx(5.0f));
}

TEST_CASE("Mat3 scale", "[mat3]") {
    Mat3 m = Mat3::scale(2.0f, 3.0f);
    Vec2 p(4.0f, 5.0f);
    Vec2 result = m.apply(p);
    REQUIRE(result.x == Catch::Approx(8.0f));
    REQUIRE(result.y == Catch::Approx(15.0f));
}

TEST_CASE("Mat3 multiplication", "[mat3]") {
    Mat3 s = Mat3::scale(2.0f, 2.0f);
    Mat3 t = Mat3::translation(1.0f, 1.0f);
    Mat3 combined = t * s;  // scale first, then translate
    Vec2 p(3.0f, 4.0f);
    Vec2 result = combined.apply(p);
    REQUIRE(result.x == Catch::Approx(7.0f));
    REQUIRE(result.y == Catch::Approx(9.0f));
}
