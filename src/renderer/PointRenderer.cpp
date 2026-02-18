#include "PointRenderer.h"
#include <algorithm>
#include <iostream>
#include <cstring>

// Fallback: look next to the binary (or cwd) if CMake didn't inject the path.
#ifndef SHADER_BASE_DIR
#define SHADER_BASE_DIR "src/shaders"
#endif

PointRenderer::PointRenderer() {
    initShaders();
    initBuffers();
}

PointRenderer::~PointRenderer() {
    cleanup();
}

void PointRenderer::initShaders() {
    m_shader = Shader::fromFiles(
        SHADER_BASE_DIR "/point.vert",
        SHADER_BASE_DIR "/point.frag"
    );
}

void PointRenderer::setupInstanceAttribs(GLuint vbo) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // Attribute 1: position (per-instance)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(PointVertex), (void*)offsetof(PointVertex, position));
    glVertexAttribDivisor(1, 1);

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
    // m_shader is a Shader object – its destructor calls glDeleteProgram automatically
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

    m_shader.use();
    m_shader.setMat3 ("u_viewProjection", viewProjection.m);
    m_shader.setFloat("u_aspectRatio",    aspectRatio);
    m_shader.setFloat("u_timeMin",        timeMin);
    m_shader.setFloat("u_timeMax",        timeMax);
    m_shader.setFloat("u_size",           m_size);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    size_t limit = std::min(numActiveChunks, m_chunkVaos.size());
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

void PointRenderer::addPoint(const Vec2& pos, float timestamp) {
    m_streamPoints.push_back({pos, timestamp});
}

void PointRenderer::end(float timeMin, float timeMax) {
    if (m_streamPoints.empty()) return;

    glBindVertexArray(m_streamVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_streamVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_streamPoints.size() * sizeof(PointVertex)),
                 m_streamPoints.data(), GL_STREAM_DRAW);

    m_shader.use();
    m_shader.setMat3 ("u_viewProjection", m_viewProjection.m);
    m_shader.setFloat("u_aspectRatio",    m_aspectRatio);
    m_shader.setFloat("u_timeMin",        timeMin);
    m_shader.setFloat("u_timeMax",        timeMax);
    m_shader.setFloat("u_size",           m_size);

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
    for (size_t count : m_chunkPointCounts) total += count;
    return total;
}
