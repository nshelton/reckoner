#pragma once

#include "core/Vec2.h"
#include "core/Color.h"
#include <vector>
#include <string>
#include <cstdint>

struct TileLine {
    Vec2 a, b;
    Color color;
};

/// Minimal Mapbox Vector Tile (MVT/protobuf) decoder.
/// Extracts line geometry from transportation, boundary, water layers.
namespace MvtDecoder {

/// Decode an MVT protobuf blob into line segments in lat/lon coordinates.
/// tileX, tileY, zoom define the tile's geographic position.
std::vector<TileLine> decode(const uint8_t* data, size_t size,
                              int tileX, int tileY, int zoom);

} // namespace MvtDecoder
