#pragma once

#include "core/Vec2.h"
#include "core/Color.h"
#include "core/Mat3.h"
#include <vector>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

/// GPU-accelerated point renderer using instanced rendering
/// Efficiently renders thousands of points with a single draw call
class PointRenderer {
public:
    PointRenderer();
    ~PointRenderer();

    /// Begin a new frame of point rendering
    void begin(const Mat3& viewProjection);

    /// Add a point to the batch (world space coordinates)
    void addPoint(const Vec2& pos, const Color& color, float size = 1.0f);

    /// Flush all points to GPU and render
    void end();

    /// Stats
    size_t pointCount() const { return m_points.size(); }

private:
    struct PointVertex {
        Vec2 position;
        Color color;
        float size;
    };

    std::vector<PointVertex> m_points;
    Mat3 m_viewProjection;

    // OpenGL resources
    GLuint m_vao = 0;
    GLuint m_quadVbo = 0;      // Static quad vertices
    GLuint m_instanceVbo = 0;  // Dynamic instance data
    GLuint m_shader = 0;

    void initShaders();
    void initBuffers();
    void cleanup();
};
