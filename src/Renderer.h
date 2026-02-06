#pragma once

#include "core/Vec2.h"
#include "core/Mat3.h"
#include "core/Color.h"
#include "renderer/LineRenderer.h"
#include "Interaction.h"
#include "AppModel.h"
#include "Camera.h"

#include <iostream>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>


#define HANDLE_RENDER_RADIUS_MM 3.0f
class Renderer
{
public:
    Renderer();

    void setSize(int width, int height)
    {
        std::cout << "GL size set to " << width << "x" << height << std::endl;
        glViewport(0, 0, width, height);
    }

    void render(const Camera &camera, const AppModel &model, const InteractionState &uiState);

    void setLineWidth(float w) { m_lines.setLineWidth(w); }
    void setNodeDiameterPx(float d) { m_nodeDiameterPx = d; m_lines.setPointDiameterPx(d); }
    float lineWidth() const { return m_lines.lineWidth(); }
    float nodeDiameterPx() const { return m_nodeDiameterPx; }

    void shutdown();

    int totalVertices() const { return static_cast<int>(m_lines.totalVertices()); }

    // Add a line directly to the renderer (for overlays like plot progress)
    void addLine(Vec2 a, Vec2 b, Color c) { m_lines.addLine(a, b, c); }

private:
    LineRenderer m_lines{};
    float m_nodeDiameterPx{8.0f};

    void renderPage(const Camera &camera, const AppModel &model);
    void drawRect(const Vec2 &min, const Vec2 &max, const Color &col);
    void drawHandle(const Vec2 &center, float sizeMm, const Color &col);
    void drawCircle(const Vec2 &center, float radiusMm, const Color &col);
};
