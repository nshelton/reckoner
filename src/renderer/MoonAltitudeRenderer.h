#pragma once

#include "core/Mat3.h"
#include "renderer/Shader.h"

#include <vector>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

/// Renders the moon phase as a filled area on the timeline.
///
/// The illuminated fraction (0 = new moon, 1 = full moon) is sampled across
/// the visible time window and mapped to Y in [-1, 1]:
///   Y = -1  →  new moon  (nothing lit)
///   Y =  0  →  quarter moon
///   Y = +1  →  full moon (completely lit)
///
/// The fill grows from the bottom up, showing the ~29.5-day synodic cycle
/// as a slow wave.  Phase is global — no observer location needed.
class MoonAltitudeRenderer {
public:
    void init();
    void shutdown();

    /// Sample moon phase across [timeStart, timeEnd] and draw the filled curve.
    void draw(const Mat3& viewProjection,
              double timeStart, double timeEnd,
              int numSamples = 300);

private:
    struct Vertex { float x, y; };

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    Shader m_shader;

    std::vector<Vertex> m_vertices;
};
