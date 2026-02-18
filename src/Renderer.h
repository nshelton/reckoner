#pragma once

#include "core/Vec2.h"
#include "core/Mat3.h"
#include "core/Color.h"
#include "renderer/LineRenderer.h"
#include "renderer/PointRenderer.h"
#include "tiles/TileRenderer.h"
#include "Interaction.h"
#include "AppModel.h"
#include "Camera.h"

#include <iostream>
#include <vector>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

/// High-level rendering coordinator
/// Delegates to specialized renderers (LineRenderer, etc.)
class Renderer
{
public:
    Renderer();
    void shutdown();

    void setSize(int width, int height)
    {
        glViewport(0, 0, width, height);
    }

    /// Main render call - draws the scene
    void render(const Camera &camera, const AppModel &model, const InteractionState &uiState);

    // Rendering configuration
    void setLineWidth(float w) { m_lines.setLineWidth(w); }
    float lineWidth() const { return m_lines.lineWidth(); }

    void setPointSize(float size) { m_points.setPointSize(size); }
    float pointSize() const { return m_points.getPointSize(); }

    void setTilesEnabled(bool enabled) { m_tilesEnabled = enabled; }
    bool tilesEnabled() const { return m_tilesEnabled; }

    // Shared access to the point renderer (used by TimelineRenderer)
    PointRenderer& points() { return m_points; }

    // Debug/stats
    int totalVertices() const { return static_cast<int>(m_lines.totalVertices()); }
    int totalPoints() const { return static_cast<int>(m_points.pointCount()); }

private:
    float m_pointSize = 0.4;
    bool m_tilesEnabled = true;
    LineRenderer m_lines{};
    PointRenderer m_points{};
    TileRenderer m_tiles{};

    // Track how many entities have been uploaded to GPU chunks
    size_t m_lastEntityCount = 0;
    std::vector<PointVertex> m_chunkBuildBuf;  // Reusable scratch buffer

    void renderGrid(const Camera &camera, const AppModel &model);
    void renderEntities(const Camera &camera, const AppModel &model);
    void rebuildChunk(size_t chunkIndex, const AppModel &model);
};
