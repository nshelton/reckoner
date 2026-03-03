#pragma once

#include "core/Entity.h"
#include "core/Mat3.h"
#include "renderer/Shader.h"
#include <vector>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

/// Per-instance GPU data for one calendar event.
struct CalendarEventVertex {
    float time_start;
    float time_end;
    float render_offset;
    float r, g, b, a;
};

/// Renders calendar events as colored rectangles on the timeline.
///
/// Unlike PointRenderer (which renders circular points via time_mid), this
/// renderer stretches each quad from time_start to time_end using per-instance
/// color from the entity's color field.
class CalendarRenderer {
public:
    void init();
    void shutdown();

    /// Build instance buffer from entities and draw.
    /// yOffset: screen-space NDC shift applied to the whole layer (matches Layer::yOffset).
    /// viewportWidth: used to compute minimum bar width in NDC (avoids zero-width bars).
    void draw(const Mat3& viewProjection,
              const std::vector<Entity>& entities,
              float yOffset = 0.0f,
              int   viewportWidth = 1000);

private:
    Shader  m_shader;
    GLuint  m_quadVbo       = 0;
    GLuint  m_vao           = 0;
    GLuint  m_instanceVbo   = 0;
    size_t  m_instanceCapacity = 0;

    std::vector<CalendarEventVertex> m_buf;  // scratch buffer, reused each frame

    void initShaders();
    void initBuffers();
};
