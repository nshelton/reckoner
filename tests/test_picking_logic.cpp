#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/PickingLogic.h"

TEST_CASE("PickResult default is invalid", "[picking_logic]") {
    PickResult pr;
    REQUIRE_FALSE(pr.valid());
}

TEST_CASE("PickResult valid when both indices non-negative", "[picking_logic]") {
    PickResult pr{0, 0};
    REQUIRE(pr.valid());
}

TEST_CASE("PickResult equality", "[picking_logic]") {
    PickResult a{1, 2};
    PickResult b{1, 2};
    PickResult c{1, 3};
    REQUIRE(a == b);
    REQUIRE(a != c);
}

TEST_CASE("fmtTimestamp at Unix epoch", "[picking_logic]") {
    std::string result = PickingLogic::fmtTimestamp(0.0);
    REQUIRE(result.find("1970-01-01") != std::string::npos);
    REQUIRE(result.find("00:00:00") != std::string::npos);
}

TEST_CASE("fmtTimestamp without seconds", "[picking_logic]") {
    std::string result = PickingLogic::fmtTimestamp(0.0, false);
    REQUIRE(result.find("00:00 UTC") != std::string::npos);
    // Should NOT contain seconds
    REQUIRE(result.find("00:00:00") == std::string::npos);
}

TEST_CASE("fmtDuration instant", "[picking_logic]") {
    REQUIRE(PickingLogic::fmtDuration(0.0) == "instant");
    REQUIRE(PickingLogic::fmtDuration(-5.0) == "instant");
}

TEST_CASE("fmtDuration seconds", "[picking_logic]") {
    REQUIRE(PickingLogic::fmtDuration(45.0) == "45s");
}

TEST_CASE("fmtDuration minutes", "[picking_logic]") {
    std::string result = PickingLogic::fmtDuration(195.0);  // 3m 15s
    REQUIRE(result == "3m 15s");
}

TEST_CASE("fmtDuration hours", "[picking_logic]") {
    std::string result = PickingLogic::fmtDuration(7500.0);  // 2h 5m
    REQUIRE(result == "2h 5m");
}

TEST_CASE("fmtDuration days", "[picking_logic]") {
    std::string result = PickingLogic::fmtDuration(90000.0);  // 1d 1h
    REQUIRE(result == "1d 1h");
}

TEST_CASE("fmtLat north", "[picking_logic]") {
    std::string result = PickingLogic::fmtLat(34.05);
    REQUIRE(result.find("34.050000") != std::string::npos);
    REQUIRE(result.find("N") != std::string::npos);
}

TEST_CASE("fmtLat south", "[picking_logic]") {
    std::string result = PickingLogic::fmtLat(-33.87);
    REQUIRE(result.find("33.870000") != std::string::npos);
    REQUIRE(result.find("S") != std::string::npos);
}

TEST_CASE("fmtLon east", "[picking_logic]") {
    std::string result = PickingLogic::fmtLon(151.21);
    REQUIRE(result.find("E") != std::string::npos);
}

TEST_CASE("fmtLon west", "[picking_logic]") {
    std::string result = PickingLogic::fmtLon(-118.25);
    REQUIRE(result.find("W") != std::string::npos);
}
