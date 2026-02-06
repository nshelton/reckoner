#pragma once

#include <vector>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include "core/Vec2.h"
#include "core/Mat3.h"
#include "core/Color.h"

class LineRenderer {
public:
    bool init();
    void shutdown();

    void clear();
    void setLineWidth(float w) { m_lineWidth = w; }
    float lineWidth() const { return m_lineWidth; }

    // Add a colored line segment in plot mm page space
    void addLine(Vec2 a, Vec2 b, Color c);

    int totalVertices() const { return static_cast<int>(m_vertices.size()); }

    void draw(const Mat3 &t);

private:
    struct GLVertex { float x, y, r, g, b, a; };

    GLuint m_program{0};
    GLuint m_vao{0};
    GLuint m_vbo{0};
    GLuint m_uProjMat{0};

    float m_lineWidth{1.0f};

    std::vector<GLVertex> m_vertices; // line vertices (as pairs)

    static GLuint compileShader(GLenum type, const char* src);
    static GLuint linkProgram(GLuint vs, GLuint fs);
};

