#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/Vec2.h"

TEST_CASE("Vec2 addition", "[vec2]") {
    Vec2 a(1.0f, 2.0f);
    Vec2 b(3.0f, 4.0f);
    Vec2 c = a + b;
    REQUIRE(c.x == 4.0f);
    REQUIRE(c.y == 6.0f);
}

TEST_CASE("Vec2 subtraction", "[vec2]") {
    Vec2 a(5.0f, 7.0f);
    Vec2 b(2.0f, 3.0f);
    Vec2 c = a - b;
    REQUIRE(c.x == 3.0f);
    REQUIRE(c.y == 4.0f);
}

TEST_CASE("Vec2 scalar multiply", "[vec2]") {
    Vec2 a(2.0f, 3.0f);
    Vec2 c = a * 2.0f;
    REQUIRE(c.x == 4.0f);
    REQUIRE(c.y == 6.0f);
}

TEST_CASE("Vec2 scalar divide", "[vec2]") {
    Vec2 a(6.0f, 8.0f);
    Vec2 c = a / 2.0f;
    REQUIRE(c.x == 3.0f);
    REQUIRE(c.y == 4.0f);
}

TEST_CASE("Vec2 component-wise multiply", "[vec2]") {
    Vec2 a(2.0f, 3.0f);
    Vec2 b(4.0f, 5.0f);
    Vec2 c = a * b;
    REQUIRE(c.x == 8.0f);
    REQUIRE(c.y == 15.0f);
}

TEST_CASE("Vec2 negation", "[vec2]") {
    Vec2 a(3.0f, -4.0f);
    Vec2 c = -a;
    REQUIRE(c.x == -3.0f);
    REQUIRE(c.y == 4.0f);
}

TEST_CASE("Vec2 compound assignment", "[vec2]") {
    Vec2 a(1.0f, 2.0f);
    a += Vec2(3.0f, 4.0f);
    REQUIRE(a.x == 4.0f);
    REQUIRE(a.y == 6.0f);

    a -= Vec2(1.0f, 1.0f);
    REQUIRE(a.x == 3.0f);
    REQUIRE(a.y == 5.0f);

    a *= 2.0f;
    REQUIRE(a.x == 6.0f);
    REQUIRE(a.y == 10.0f);

    a /= 2.0f;
    REQUIRE(a.x == 3.0f);
    REQUIRE(a.y == 5.0f);
}

TEST_CASE("Vec2 default constructor is zero", "[vec2]") {
    Vec2 v;
    REQUIRE(v.x == 0.0f);
    REQUIRE(v.y == 0.0f);
}
