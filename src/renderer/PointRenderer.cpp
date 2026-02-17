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
layout(location = 4) in float in_timestamp;

out vec4 v_color;
out vec2 v_coord;
out float v_t;  // Normalized time [0,1] for colormap

uniform mat3 u_viewProjection;
uniform float u_aspectRatio;
uniform float u_timeMin;
uniform float u_timeMax;

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

    // Normalize timestamp to [0,1] range (unclamped so fragment shader can detect out-of-range)
    // -1 signals "use vertex color" (when time range is zero)
    float range = u_timeMax - u_timeMin;
    v_t = (range > 0.0) ? (in_timestamp - u_timeMin) / range : -1.0;
}
)";

// Fragment shader - renders a circular point with turbo colormap
static const char* FRAGMENT_SHADER = R"(
#version 330 core

in vec4 v_color;
in vec2 v_coord;
in float v_t;

out vec4 out_color;

// Turbo colormap approximation (Google AI, Apache 2.0)
// https://ai.googleblog.com/2019/08/turbo-improved-rainbow-colormap-for.html
vec3 turbo(float t) {
    const vec4 kR = vec4(0.13572138, 4.61539260, -42.66032258, 132.13108234);
    const vec4 kG = vec4(0.09140261, 2.19418839, 4.84296658, -14.18503333);
    const vec4 kB = vec4(0.10667330, 12.64194608, -60.58204836, 110.36276771);
    const vec2 kRa = vec2(-152.94239396, 59.28637943);
    const vec2 kGa = vec2(4.27729857, 2.82956604);
    const vec2 kBa = vec2(-89.90310912, 27.34824973);

    t = clamp(t, 0.0, 1.0);
    vec4 v4 = vec4(1.0, t, t*t, t*t*t);
    vec2 v2 = v4.zw * v4.z;
    return vec3(
        dot(v4, kR) + dot(v2, kRa),
        dot(v4, kG) + dot(v2, kGa),
        dot(v4, kB) + dot(v2, kBa)
    );
}

void main() {
    // Circular point (distance from center)
    float dist = length(v_coord);
    if (dist > 1.0) discard;

    // Soft edge
    float alpha = 1.0 - smoothstep(0.8, 1.0, dist);

    // Gray for out-of-range, turbo for in-range, vertex color when no time mapping
    // v_t == -1: no time mapping; v_t < 0 or > 1: out of range; 0..1: in range
    vec3 color;
    if (v_t <= -0.5) {
        color = v_color.rgb;           // No time mapping (streaming fallback)
    } else if (v_t < 0.0 || v_t > 1.0) {
        color = vec3(0.1, 0.1, 0.1);  // Out of visible time range
    } else {
        color = turbo(v_t);            // In range — turbo colormap
    }
    out_color = vec4(color, v_color.a * alpha);
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

    // Attribute 4: timestamp (per-instance)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(PointVertex), (void*)offsetof(PointVertex, timestamp));
    glVertexAttribDivisor(4, 1);
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

    // No chunked buffers allocated yet — they grow on demand via ensureChunks()
}

void PointRenderer::allocateChunk() {
    GLuint vbo, vao;
    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, CHUNK_SIZE * sizeof(PointVertex), nullptr, GL_DYNAMIC_DRAW);

    glBindVertexArray(vao);

    // Attribute 0: quad vertex (shared, per-vertex)
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glVertexAttribDivisor(0, 0);

    setupInstanceAttribs(vbo);
    glBindVertexArray(0);

    m_chunkVaos.push_back(vao);
    m_chunkVbos.push_back(vbo);
    m_chunkPointCounts.push_back(0);
}

void PointRenderer::ensureChunks(size_t numChunks) {
    while (m_chunkVaos.size() < numChunks) {
        allocateChunk();
    }
}

void PointRenderer::cleanup() {
    if (m_quadVbo) glDeleteBuffers(1, &m_quadVbo);
    if (!m_chunkVbos.empty()) glDeleteBuffers(static_cast<GLsizei>(m_chunkVbos.size()), m_chunkVbos.data());
    if (!m_chunkVaos.empty()) glDeleteVertexArrays(static_cast<GLsizei>(m_chunkVaos.size()), m_chunkVaos.data());
    if (m_streamVbo) glDeleteBuffers(1, &m_streamVbo);
    if (m_streamVao) glDeleteVertexArrays(1, &m_streamVao);
    if (m_shader) glDeleteProgram(m_shader);
}

// --- Chunked mode ---

void PointRenderer::updateChunk(size_t chunkIndex, const PointVertex* data, size_t count) {
    if (chunkIndex >= m_chunkVaos.size()) return;

    m_chunkPointCounts[chunkIndex] = count;
    if (count == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, m_chunkVbos[chunkIndex]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(PointVertex), data);
}

void PointRenderer::drawChunked(const Mat3& viewProjection, float aspectRatio, size_t numActiveChunks,
                                float timeMin, float timeMax) {
    if (numActiveChunks == 0) return;

    glUseProgram(m_shader);

    GLint loc = glGetUniformLocation(m_shader, "u_viewProjection");
    glUniformMatrix3fv(loc, 1, GL_FALSE, viewProjection.m);

    GLint aspectLoc = glGetUniformLocation(m_shader, "u_aspectRatio");
    glUniform1f(aspectLoc, aspectRatio);

    glUniform1f(glGetUniformLocation(m_shader, "u_timeMin"), timeMin);
    glUniform1f(glGetUniformLocation(m_shader, "u_timeMax"), timeMax);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    size_t limit = numActiveChunks < m_chunkVaos.size() ? numActiveChunks : m_chunkVaos.size();
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

void PointRenderer::addPoint(const Vec2& pos, const Color& color, float size, float timestamp) {
    m_streamPoints.push_back({pos, color, size, timestamp});
}

void PointRenderer::end(float timeMin, float timeMax) {
    if (m_streamPoints.empty()) return;

    glUseProgram(m_shader);

    GLint loc = glGetUniformLocation(m_shader, "u_viewProjection");
    glUniformMatrix3fv(loc, 1, GL_FALSE, m_viewProjection.m);

    GLint aspectLoc = glGetUniformLocation(m_shader, "u_aspectRatio");
    glUniform1f(aspectLoc, m_aspectRatio);

    glUniform1f(glGetUniformLocation(m_shader, "u_timeMin"), timeMin);
    glUniform1f(glGetUniformLocation(m_shader, "u_timeMax"), timeMax);

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
    for (size_t i = 0; i < m_chunkPointCounts.size(); i++) {
        total += m_chunkPointCounts[i];
    }
    return total;
}
