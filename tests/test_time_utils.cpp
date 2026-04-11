#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/TimeUtils.h"

TEST_CASE("TimeUtils parse_iso8601 known epoch", "[time_utils]") {
    // 2020-01-01T00:00:00Z = 1577836800
    double ts = TimeUtils::parse_iso8601("2020-01-01T00:00:00Z");
    REQUIRE(ts == Catch::Approx(1577836800.0));
}

TEST_CASE("TimeUtils to_iso8601 known epoch", "[time_utils]") {
    std::string s = TimeUtils::to_iso8601(1577836800.0);
    REQUIRE(s == "2020-01-01T00:00:00Z");
}

TEST_CASE("TimeUtils roundtrip", "[time_utils]") {
    std::string original = "2024-06-15T12:30:00Z";
    double ts = TimeUtils::parse_iso8601(original);
    std::string recovered = TimeUtils::to_iso8601(ts);
    REQUIRE(recovered == original);
}

TEST_CASE("TimeUtils parse without Z suffix", "[time_utils]") {
    double ts = TimeUtils::parse_iso8601("2020-01-01T00:00:00");
    REQUIRE(ts == Catch::Approx(1577836800.0));
}

TEST_CASE("TimeUtils unix epoch", "[time_utils]") {
    double ts = TimeUtils::parse_iso8601("1970-01-01T00:00:00Z");
    REQUIRE(ts == Catch::Approx(0.0));
}
