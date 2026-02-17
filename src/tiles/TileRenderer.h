#pragma once

#include "tiles/TileCache.h"
#include "renderer/LineRenderer.h"
#include "Camera.h"

/// Renders vector tile data as lines using the existing LineRenderer.
class TileRenderer {
public:
    TileRenderer() = default;
    ~TileRenderer() = default;

    void init();
    void shutdown();

    /// Determine visible tiles, fetch/cache, and add lines to the LineRenderer.
    /// Call this before LineRenderer::draw().
    void render(const Camera& camera, LineRenderer& lines);

private:
    TileCache m_cache;
};
