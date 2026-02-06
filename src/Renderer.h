#pragma once

#include "core/Vec2.h"
#include "core/Mat3.h"
#include "core/Color.h"
#include "renderer/LineRenderer.h"
#include "render/PointRenderer.h"
#include "Interaction.h"
#include "AppModel.h"
#include "Camera.h"

#include <iostream>
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

    // Debug/stats
    int totalVertices() const { return static_cast<int>(m_lines.totalVertices()); }
    int totalPoints() const { return static_cast<int>(m_points.pointCount()); }

private:
    LineRenderer m_lines{};
    PointRenderer m_points{};

    void renderGrid(const Camera &camera, const AppModel &model);
    void renderEntities(const Camera &camera, const AppModel &model);
};
