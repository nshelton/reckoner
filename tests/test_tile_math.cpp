#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "tiles/TileMath.h"

TEST_CASE("TileMath lon/lat to tile roundtrip", "[tile_math]") {
    int zoom = 10;
    double lon = -118.25;
    double lat = 34.05;

    double tileX = TileMath::lonToTileX(lon, zoom);
    double tileY = TileMath::latToTileY(lat, zoom);

    double recoveredLon = TileMath::tileXToLon(tileX, zoom);
    double recoveredLat = TileMath::tileYToLat(tileY, zoom);

    REQUIRE(recoveredLon == Catch::Approx(lon).margin(1e-6));
    REQUIRE(recoveredLat == Catch::Approx(lat).margin(1e-6));
}

TEST_CASE("TileMath lon 0 maps to center tile", "[tile_math]") {
    int zoom = 1;
    double tileX = TileMath::lonToTileX(0.0, zoom);
    // At zoom 1, there are 2 tiles. Lon 0 = middle = tile 1.0
    REQUIRE(tileX == Catch::Approx(1.0));
}

TEST_CASE("TileMath lat 0 maps to center tile", "[tile_math]") {
    int zoom = 1;
    double tileY = TileMath::latToTileY(0.0, zoom);
    REQUIRE(tileY == Catch::Approx(1.0));
}

TEST_CASE("TileMath zoomForExtent", "[tile_math]") {
    // A very zoomed-in view should yield a high zoom level
    int z = TileMath::zoomForExtent(0.01, 800);
    REQUIRE(z >= 10);

    // A very zoomed-out view should yield a low zoom level
    z = TileMath::zoomForExtent(50.0, 800);
    REQUIRE(z <= 5);
}
