#include "PointRenderer.h"
#include <iostream>
#include <cstring>

// Vertex shader - receives quad vertex + per-instance data
static const char* VERTEX_SHADER = R"(
#version 330 core

// Per-vertex attribute (quad corner)
layout(location = 0) in vec2 in_quad_vertex;

// Per-instance attributes
layout(location = 1) in vec2 in_position;
layout(location = 2) in vec4 in_color;
layout(location = 3) in float in_size;

out vec4 v_color;
out vec2 v_coord;

uniform mat3 u_viewProjection;

void main() {
    // Offset by quad vertex * size
    vec2 offset = in_quad_vertex * in_size;

    // Transform position
    vec3 pos = u_viewProjection * vec3(in_position + offset, 1.0);
    gl_Position = vec4(pos.xy, 0.0, 1.0);

    v_color = in_color;
    v_coord = in_quad_vertex;  // -1 to 1 for circular rendering
}
)";

// Fragment shader - renders a circular point
static const char* FRAGMENT_SHADER = R"(
#version 330 core

in vec4 v_color;
in vec2 v_coord;

out vec4 out_color;

void main() {
    // Circular point (distance from center)
    float dist = length(v_coord);
    if (dist > 1.0) discard;

    // Soft edge
    float alpha = 1.0 - smoothstep(0.8, 1.0, dist);
    out_color = v_color * vec4(1.0, 1.0, 1.0, alpha);
}
)";

static GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compilation failed: " << log << std::endl;
        return 0;
    }
    return shader;
}

PointRenderer::PointRenderer() {
    initShaders();
    initBuffers();
}

void PointRenderer::initBuffers() {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_quadVbo);
    glGenBuffers(1, &m_instanceVbo);

    // Quad vertices (triangle strip order: BL, BR, TL, TR)
    float quadVertices[] = {
        -1.0f, -1.0f,  // bottom-left
         1.0f, -1.0f,  // bottom-right
        -1.0f,  1.0f,  // top-left
         1.0f,  1.0f   // top-right
    };

    glBindVertexArray(m_vao);

    // Upload static quad vertices
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Attribute 0: quad vertex (per-vertex, not per-instance)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glVertexAttribDivisor(0, 0);  // 0 = per vertex

    // Set up instance buffer (will be filled in end())
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);

    // Attribute 1: position (per-instance)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(PointVertex), (void*)offsetof(PointVertex, position));
    glVertexAttribDivisor(1, 1);

    // Attribute 2: color (per-instance)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(PointVertex), (void*)offsetof(PointVertex, color));
    glVertexAttribDivisor(2, 1);

    // Attribute 3: size (per-instance)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(PointVertex), (void*)offsetof(PointVertex, size));
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
}

PointRenderer::~PointRenderer() {
    cleanup();
}

void PointRenderer::initShaders() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);

    m_shader = glCreateProgram();
    glAttachShader(m_shader, vs);
    glAttachShader(m_shader, fs);
    glLinkProgram(m_shader);

    GLint success;
    glGetProgramiv(m_shader, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(m_shader, 512, nullptr, log);
        std::cerr << "Shader linking failed: " << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

void PointRenderer::cleanup() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_quadVbo) glDeleteBuffers(1, &m_quadVbo);
    if (m_instanceVbo) glDeleteBuffers(1, &m_instanceVbo);
    if (m_shader) glDeleteProgram(m_shader);
}

void PointRenderer::begin(const Mat3& viewProjection) {
    m_viewProjection = viewProjection;
    m_points.clear();
}

void PointRenderer::addPoint(const Vec2& pos, const Color& color, float size) {
    m_points.push_back({pos, color, size});
}

void PointRenderer::end() {
    if (m_points.empty()) return;

    // Debug output
    static bool first_call = true;
    if (first_call) {
        std::cout << "PointRenderer::end() called with " << m_points.size() << " points" << std::endl;
        first_call = false;
    }

    glUseProgram(m_shader);

    // Upload view-projection matrix
    GLint loc = glGetUniformLocation(m_shader, "u_viewProjection");
    glUniformMatrix3fv(loc, 1, GL_FALSE, m_viewProjection.m);

    // Upload instance data
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, m_points.size() * sizeof(PointVertex), m_points.data(), GL_STREAM_DRAW);

    // Enable blending for smooth points
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw instances: 4 vertices per quad, N instances
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(m_points.size()));

    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);
}
