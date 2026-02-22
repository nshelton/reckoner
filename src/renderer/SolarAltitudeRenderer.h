#pragma once

#include "core/Mat3.h"
#include "renderer/Shader.h"

#include <vector>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

/// Renders the solar altitude curve as a filled area on the timeline.
///
/// For a given observer lat/lon the sun's elevation angle is sampled at
/// regular intervals across the visible time window.  The result is drawn
/// as two translucent filled regions:
///   - Above-horizon fill (altitude > 0°): warm amber, grows upward from y = 0.
///   - Below-horizon fill (altitude < 0°): cool blue, grows downward from y = 0.
///
/// Y = 0 corresponds to the horizon (altitude = 0°), Y = ±1 to ±90°.
/// The same histogram.vert / histogram.frag shaders are reused.
class SolarAltitudeRenderer {
public:
    void init();
    void shutdown();

    /// Sample solar altitude across [timeStart, timeEnd] for the given location
    /// and draw the filled curve into the current timeline viewport.
    ///
    /// @param viewProjection  Timeline camera transform (same Mat3 as HistogramRenderer).
    /// @param timeStart       Left edge of visible range (Unix seconds).
    /// @param timeEnd         Right edge of visible range (Unix seconds).
    /// @param latDeg          Observer latitude  in decimal degrees.
    /// @param lonDeg          Observer longitude in decimal degrees.
    /// @param numSamples      Curve resolution (higher = smoother, default 300).
    void draw(const Mat3& viewProjection,
              double timeStart, double timeEnd,
              double latDeg, double lonDeg,
              int numSamples = 300);

private:
    struct Vertex { float x, y; };

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    Shader m_shader;

    std::vector<Vertex> m_vertices;

    void uploadAndDraw(float r, float g, float b, float a,
                       const Mat3& viewProjection);
};
