#include "tiles/TileRenderer.h"
#include "tiles/TileMath.h"
#include <algorithm>
#include <cmath>

void TileRenderer::init() {
    // Nothing to initialize — we use the shared LineRenderer
}

void TileRenderer::shutdown() {
    // TileCache cleans up in its destructor
}

void TileRenderer::render(const Camera& camera, LineRenderer& lines) {
    m_cache.processCompletedFetches();

    // Get visible bounds from camera
    Vec2 c = camera.center();
    float halfH = camera.zoom();
    float aspect = static_cast<float>(camera.width()) / static_cast<float>(camera.height());
    float halfW = halfH * aspect;

    float lonMin = c.x - halfW;
    float lonMax = c.x + halfW;
    float latMin = c.y - halfH;
    float latMax = c.y + halfH;

    // Clamp to valid Mercator lat range
    latMin = std::max(latMin, -85.05f);
    latMax = std::min(latMax, 85.05f);
    if (latMin >= latMax) return;

    // Pick zoom level
    int zoom = TileMath::zoomForExtent(halfH, camera.height());
    zoom = std::max(0, std::min(14, zoom)); // Cap at 14 for vector tiles

    // Convert bounds to tile coordinates
    int tileXMin = static_cast<int>(std::floor(TileMath::lonToTileX(lonMin, zoom)));
    int tileXMax = static_cast<int>(std::floor(TileMath::lonToTileX(lonMax, zoom)));
    int tileYMin = static_cast<int>(std::floor(TileMath::latToTileY(latMax, zoom))); // tileY inverted vs lat
    int tileYMax = static_cast<int>(std::floor(TileMath::latToTileY(latMin, zoom)));

    int maxTile = (1 << zoom) - 1;
    tileXMin = std::max(0, tileXMin);
    tileXMax = std::min(maxTile, tileXMax);
    tileYMin = std::max(0, tileYMin);
    tileYMax = std::min(maxTile, tileYMax);

    // Add lines from all visible cached tiles
    for (int ty = tileYMin; ty <= tileYMax; ty++) {
        for (int tx = tileXMin; tx <= tileXMax; tx++) {
            const auto* tileLines = m_cache.requestTile({zoom, tx, ty});
            if (!tileLines) continue;

            for (const auto& line : *tileLines) {
                lines.addLine(line.a, line.b, line.color);
            }
        }
    }

    m_cache.evictOldTiles();
}
