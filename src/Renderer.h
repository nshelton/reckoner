#pragma once

#include "core/Vec2.h"
#include "core/Mat3.h"
#include "core/Color.h"
#include "renderer/LineRenderer.h"
#include "renderer/PointRenderer.h"
#include "tiles/TileRenderer.h"
#include "tiles/RasterTileRenderer.h"
#include "Interaction.h"
#include "AppModel.h"
#include "Camera.h"

#include <vector>
#include <memory>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

/// Which tile layer to display behind entities.
enum class TileMode {
    None = 0,          ///< No map tiles
    Vector,            ///< Vector tiles (Versatiles OSM lines) — default
    OSM,               ///< OpenStreetMap raster (labeled)
    CartoDB_Light,     ///< CartoDB Positron — light theme with labels
    CartoDB_Dark,      ///< CartoDB Dark Matter — dark theme with labels
};


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

    /// Draw a highlight ring over a geographic position.
    /// Call after render() while the same GL viewport/scissor is still active.
    void drawMapHighlight(const Camera &camera, double lon, double lat);

    // Rendering configuration
    void setLineWidth(float w) { m_lines.setLineWidth(w); }
    float lineWidth() const { return m_lines.lineWidth(); }

    void setPointSize(float size);
    float pointSize() const { return m_pointSize; }

    void setTileMode(TileMode mode);
    TileMode tileMode() const { return m_tileMode; }

    // Access per-layer point renderer (used by TimelineRenderer)
    PointRenderer* layerRenderer(size_t layerIndex);
    size_t numLayerRenderers() const { return m_layerPoints.size(); }

    // Debug/stats
    int totalVertices() const { return static_cast<int>(m_lines.totalVertices()); }
    int totalPoints() const;

private:
    float m_pointSize = 0.4f;
    TileMode m_tileMode = TileMode::Vector;
    LineRenderer m_lines{};
    TileRenderer m_tiles{};
    RasterTileRenderer m_rasterTiles{};

    // One PointRenderer per layer (grown to match model.layers on demand)
    std::vector<std::unique_ptr<PointRenderer>> m_layerPoints;
    std::vector<size_t> m_layerEntityCounts;  // dirty-check per layer

    std::vector<PointVertex> m_chunkBuildBuf;  // Reusable scratch buffer

    void renderGrid(const Camera &camera, const AppModel &model);
    void renderEntities(const Camera &camera, const AppModel &model);
    void ensureLayerRenderer(size_t layerIndex);
    void rebuildLayerChunk(size_t layerIndex, size_t chunkIndex, const Layer &layer);
};
