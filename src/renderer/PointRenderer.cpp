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
uniform float u_aspectRatio;

void main() {
    // Transform center position to NDC first
    vec3 center_ndc = u_viewProjection * vec3(in_position, 1.0);

    // Apply size offset in NDC space (after projection)
    // Scale by 0.01 to make size values reasonable (1.0 = ~1% of screen)
    vec2 offset_ndc = in_quad_vertex * in_size * 0.01;

    // Compensate for aspect ratio to keep points circular
    offset_ndc.x /= u_aspectRatio;

    gl_Position = vec4(center_ndc.xy + offset_ndc, 0.0, 1.0);

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

void PointRenderer::setupInstanceAttribs(GLuint vbo) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

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
}

void PointRenderer::initBuffers() {
    // Quad vertices shared by all VAOs (triangle strip: BL, BR, TL, TR)
    float quadVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };

    glGenBuffers(1, &m_quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // --- Chunked mode: per-chunk VBOs and VAOs ---
    glGenBuffers(NUM_CHUNKS, m_chunkVbos);
    glGenVertexArrays(NUM_CHUNKS, m_chunkVaos);

    for (size_t i = 0; i < NUM_CHUNKS; i++) {
        glBindBuffer(GL_ARRAY_BUFFER, m_chunkVbos[i]);
        glBufferData(GL_ARRAY_BUFFER, CHUNK_SIZE * sizeof(PointVertex), nullptr, GL_DYNAMIC_DRAW);

        glBindVertexArray(m_chunkVaos[i]);

        // Attribute 0: quad vertex (shared, per-vertex)
        glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glVertexAttribDivisor(0, 0);

        setupInstanceAttribs(m_chunkVbos[i]);
        glBindVertexArray(0);
    }

    // --- Streaming mode: single dynamic VBO + VAO ---
    glGenBuffers(1, &m_streamVbo);
    glGenVertexArrays(1, &m_streamVao);

    glBindVertexArray(m_streamVao);

    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glVertexAttribDivisor(0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, m_streamVbo);
    setupInstanceAttribs(m_streamVbo);

    glBindVertexArray(0);
}

void PointRenderer::cleanup() {
    if (m_quadVbo) glDeleteBuffers(1, &m_quadVbo);
    glDeleteBuffers(NUM_CHUNKS, m_chunkVbos);
    glDeleteVertexArrays(NUM_CHUNKS, m_chunkVaos);
    if (m_streamVbo) glDeleteBuffers(1, &m_streamVbo);
    if (m_streamVao) glDeleteVertexArrays(1, &m_streamVao);
    if (m_shader) glDeleteProgram(m_shader);
}

// --- Chunked mode ---

void PointRenderer::updateChunk(size_t chunkIndex, const PointVertex* data, size_t count) {
    if (chunkIndex >= NUM_CHUNKS) return;

    m_chunkPointCounts[chunkIndex] = count;
    if (count == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, m_chunkVbos[chunkIndex]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(PointVertex), data);
}

void PointRenderer::drawChunked(const Mat3& viewProjection, float aspectRatio, size_t numActiveChunks) {
    if (numActiveChunks == 0) return;

    glUseProgram(m_shader);

    GLint loc = glGetUniformLocation(m_shader, "u_viewProjection");
    glUniformMatrix3fv(loc, 1, GL_FALSE, viewProjection.m);

    GLint aspectLoc = glGetUniformLocation(m_shader, "u_aspectRatio");
    glUniform1f(aspectLoc, aspectRatio);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    size_t limit = numActiveChunks < NUM_CHUNKS ? numActiveChunks : NUM_CHUNKS;
    for (size_t i = 0; i < limit; i++) {
        if (m_chunkPointCounts[i] == 0) continue;
        glBindVertexArray(m_chunkVaos[i]);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(m_chunkPointCounts[i]));
    }

    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);
}

// --- Streaming mode ---

void PointRenderer::begin(const Mat3& viewProjection, float aspectRatio) {
    m_viewProjection = viewProjection;
    m_aspectRatio = aspectRatio;
    m_streamPoints.clear();
}

void PointRenderer::addPoint(const Vec2& pos, const Color& color, float size) {
    m_streamPoints.push_back({pos, color, size});
}

void PointRenderer::end() {
    if (m_streamPoints.empty()) return;

    glUseProgram(m_shader);

    GLint loc = glGetUniformLocation(m_shader, "u_viewProjection");
    glUniformMatrix3fv(loc, 1, GL_FALSE, m_viewProjection.m);

    GLint aspectLoc = glGetUniformLocation(m_shader, "u_aspectRatio");
    glUniform1f(aspectLoc, m_aspectRatio);

    glBindVertexArray(m_streamVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_streamVbo);
    glBufferData(GL_ARRAY_BUFFER, m_streamPoints.size() * sizeof(PointVertex), m_streamPoints.data(), GL_STREAM_DRAW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(m_streamPoints.size()));

    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);
}

// --- Stats ---

size_t PointRenderer::pointCount() const {
    size_t total = 0;
    for (size_t i = 0; i < NUM_CHUNKS; i++) {
        total += m_chunkPointCounts[i];
    }
    return total;
}
