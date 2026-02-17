#pragma once

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TileMath {

inline double lonToTileX(double lon, int zoom) {
    return (lon + 180.0) / 360.0 * (1 << zoom);
}

inline double latToTileY(double lat, int zoom) {
    double latRad = lat * M_PI / 180.0;
    return (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 * (1 << zoom);
}

inline double tileXToLon(double x, int zoom) {
    return x / (1 << zoom) * 360.0 - 180.0;
}

inline double tileYToLat(double y, int zoom) {
    double n = M_PI - 2.0 * M_PI * y / (1 << zoom);
    return 180.0 / M_PI * std::atan(0.5 * (std::exp(n) - std::exp(-n)));
}

inline int zoomForExtent(double halfExtentDeg, int screenHeightPx) {
    double degreesPerPixel = (halfExtentDeg * 2.0) / screenHeightPx;
    for (int z = 18; z >= 0; --z) {
        double tileDegPerPx = (360.0 / (1 << z)) / 256.0;
        if (tileDegPerPx >= degreesPerPixel)
            return z;
    }
    return 0;
}

} // namespace TileMath
