#pragma once

#include "core/Mat3.h"
#include "core/Entity.h"
#include "renderer/Shader.h"

#include <vector>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

/// Renders a histogram overlay on the timeline view.
///
/// Entities are binned by time_mid over the visible [timeStart, timeEnd] range.
/// Each bin becomes a filled rectangle whose height is proportional to its count
/// relative to the peak bin.  Bars grow upward from the bottom of the timeline.
class HistogramRenderer {
public:
    struct TimeRange { float x0, x1; };

    void init();
    void shutdown();

    /// Bin entities by time_mid and draw filled bars in timeline coordinate space.
    /// @param viewProjection  The timeline camera transform (same Mat3 passed to PointRenderer).
    /// @param entities        All loaded entities (iterated each frame; fast for <1M points).
    /// @param timeStart       Left edge of the visible time range (Unix seconds).
    /// @param timeEnd         Right edge of the visible time range (Unix seconds).
    /// @param numBins         Number of histogram columns (default 100).
    void draw(const Mat3& viewProjection,
              const std::vector<Entity>& entities,
              double timeStart, double timeEnd,
              int numBins = 100);

    /// Draw a list of filled x-spans as solid rectangles using the same shader.
    /// @param rects  List of (x0, x1) time-coordinate spans.
    /// @param y0/y1  Vertical extent in timeline NDC space (e.g. -1 to +1).
    /// @param r,g,b,a  Fill color with alpha.
    void drawRects(const Mat3& viewProjection,
                   const std::vector<TimeRange>& rects,
                   float y0, float y1,
                   float r, float g, float b, float a);

private:
    struct Vertex { float x, y; };

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    Shader m_shader;

    std::vector<Vertex> m_vertices;
};
