#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/SolarCalculations.h"

TEST_CASE("Solar altitude at known noon", "[solar]") {
    // Summer solstice 2024, solar noon in LA (approx 12:50 PM PDT = 19:50 UTC)
    // June 20, 2024 19:50 UTC ≈ 1718912990
    double unixTime = 1718912990.0;
    double alt = SolarCalc::solarAltitudeDeg(34.05, -118.25, unixTime);

    // At summer solstice solar noon in LA, sun should be very high (>70°)
    REQUIRE(alt > 70.0);
    REQUIRE(alt < 90.0);
}

TEST_CASE("Solar altitude at midnight is negative", "[solar]") {
    // Jan 1, 2024 08:00 UTC = midnight PST in LA
    double unixTime = 1704096000.0;
    double alt = SolarCalc::solarAltitudeDeg(34.05, -118.25, unixTime);

    REQUIRE(alt < 0.0);
}

TEST_CASE("Moon illumination is in range", "[solar]") {
    double frac = SolarCalc::moonIlluminationFraction(1704096000.0);
    REQUIRE(frac >= 0.0);
    REQUIRE(frac <= 1.0);
}

TEST_CASE("Moon illumination at known full moon", "[solar]") {
    // Dec 26, 2023 was a full moon. Unix: ~1703548800
    double frac = SolarCalc::moonIlluminationFraction(1703548800.0);
    REQUIRE(frac > 0.9);
}
