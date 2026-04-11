#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/RingBuffer.h"

TEST_CASE("RingBuffer push and size", "[ring_buffer]") {
    RingBuffer<int, 4> buf;
    REQUIRE(buf.empty());
    REQUIRE(buf.size() == 0);

    buf.push(10);
    REQUIRE(buf.size() == 1);
    REQUIRE_FALSE(buf.empty());

    buf.push(20);
    buf.push(30);
    REQUIRE(buf.size() == 3);
    REQUIRE_FALSE(buf.full());

    buf.push(40);
    REQUIRE(buf.size() == 4);
    REQUIRE(buf.full());
}

TEST_CASE("RingBuffer overflow wraps", "[ring_buffer]") {
    RingBuffer<int, 3> buf;
    buf.push(1);
    buf.push(2);
    buf.push(3);
    REQUIRE(buf.full());

    // Push beyond capacity
    buf.push(4);
    REQUIRE(buf.size() == 3);
    REQUIRE(buf.full());
}

TEST_CASE("RingBuffer clear", "[ring_buffer]") {
    RingBuffer<int, 4> buf;
    buf.push(1);
    buf.push(2);
    buf.clear();
    REQUIRE(buf.empty());
    REQUIRE(buf.size() == 0);
}

TEST_CASE("LatencyRingBuffer statistics", "[ring_buffer]") {
    LatencyRingBuffer<4> buf;
    REQUIRE(buf.average() == 0.0f);
    REQUIRE(buf.min() == 0.0f);
    REQUIRE(buf.max() == 0.0f);

    buf.push(10.0f);
    buf.push(20.0f);
    buf.push(30.0f);
    buf.push(40.0f);

    REQUIRE(buf.average() == Catch::Approx(25.0f));
    REQUIRE(buf.min() == Catch::Approx(10.0f));
    REQUIRE(buf.max() == Catch::Approx(40.0f));
    REQUIRE(buf.count() == 4);
}

TEST_CASE("LatencyRingBuffer overflow keeps stats correct", "[ring_buffer]") {
    LatencyRingBuffer<3> buf;
    buf.push(10.0f);
    buf.push(20.0f);
    buf.push(30.0f);
    buf.push(100.0f);  // overwrites 10.0f

    REQUIRE(buf.count() == 3);
    // Buffer now contains 100, 20, 30 (100 overwrote slot 0)
    REQUIRE(buf.max() == Catch::Approx(100.0f));
}
