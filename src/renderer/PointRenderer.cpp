#include "PointRenderer.h"
#include <algorithm>
#include <iostream>

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
    m_mapShader = Shader::fromFiles(
        SHADER_BASE_DIR "/point_map.vert",
        SHADER_BASE_DIR "/point.frag"
    );
    m_timelineShader = Shader::fromFiles(
        SHADER_BASE_DIR "/point_timeline.vert",
        SHADER_BASE_DIR "/point.frag"
    );
}

void PointRenderer::initBuffers() {
    float quadVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    glGenBuffers(1, &m_quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
}

void PointRenderer::allocateChunk() {
    GLuint vbo, vao;
    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, CHUNK_SIZE * sizeof(PointVertex), nullptr, GL_DYNAMIC_DRAW);

    glBindVertexArray(vao);

    // Attrib 0: quad vertex (per-vertex, divisor=0)
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glVertexAttribDivisor(0, 0);

    // Attrib 1: geo_pos vec2 (per-instance, divisor=1)
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(PointVertex),
                          (void*)offsetof(PointVertex, geo_pos));
    glVertexAttribDivisor(1, 1);

    // Attrib 2: time_mid float (per-instance, divisor=1)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(PointVertex),
                          (void*)offsetof(PointVertex, time_mid));
    glVertexAttribDivisor(2, 1);

    // Attrib 3: render_offset float (per-instance, divisor=1)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(PointVertex),
                          (void*)offsetof(PointVertex, render_offset));
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);

    m_chunkVaos.push_back(vao);
    m_chunkVbos.push_back(vbo);
    m_chunkPointCounts.push_back(0);
}

void PointRenderer::ensureChunks(size_t numChunks) {
    while (m_chunkVaos.size() < numChunks) allocateChunk();
}

void PointRenderer::updateChunk(size_t chunkIndex, const PointVertex* data, size_t count) {
    if (chunkIndex >= m_chunkVaos.size()) return;
    m_chunkPointCounts[chunkIndex] = count;
    if (count == 0) return;
    glBindBuffer(GL_ARRAY_BUFFER, m_chunkVbos[chunkIndex]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(PointVertex), data);
}

// Draw instances — caller is responsible for shader bind, uniform setup, and blend state.
void PointRenderer::drawChunkLoop(size_t numActiveChunks) {
    size_t limit = std::min(numActiveChunks, m_chunkVaos.size());
    for (size_t i = 0; i < limit; i++) {
        if (m_chunkPointCounts[i] == 0) continue;
        glBindVertexArray(m_chunkVaos[i]);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
                              static_cast<GLsizei>(m_chunkPointCounts[i]));
    }
    glBindVertexArray(0);
}

void PointRenderer::drawChunked(const Mat3& viewProjection, float aspectRatio,
                                 size_t numActiveChunks, float timeMin, float timeMax) {
    if (numActiveChunks == 0) return;

    m_mapShader.use();
    m_mapShader.setMat3 ("u_viewProjection", viewProjection.m);
    m_mapShader.setFloat("u_aspectRatio",    aspectRatio);
    m_mapShader.setFloat("u_timeMin",        timeMin);
    m_mapShader.setFloat("u_timeMax",        timeMax);
    m_mapShader.setFloat("u_size",           m_size);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawChunkLoop(numActiveChunks);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

void PointRenderer::drawForTimeline(const Mat3& viewProjection, float aspectRatio,
                                     size_t numActiveChunks, float timeMin, float timeMax,
                                     const MapExtent& mapExtent) {
    if (numActiveChunks == 0) return;

    // Set all uniforms once — u_filterMode will be changed between passes
    m_timelineShader.use();
    m_timelineShader.setMat3 ("u_viewProjection", viewProjection.m);
    m_timelineShader.setFloat("u_aspectRatio",    aspectRatio);
    m_timelineShader.setFloat("u_timeMin",        timeMin);
    m_timelineShader.setFloat("u_timeMax",        timeMax);
    m_timelineShader.setFloat("u_size",           m_size);
    m_timelineShader.setFloat("u_mapMinLon",      mapExtent.minLon);
    m_timelineShader.setFloat("u_mapMaxLon",      mapExtent.maxLon);
    m_timelineShader.setFloat("u_mapMinLat",      mapExtent.minLat);
    m_timelineShader.setFloat("u_mapMaxLat",      mapExtent.maxLat);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Pass 1: out-of-map points (gray) drawn first so they sit behind in-map color
    m_timelineShader.setInt("u_filterMode", 0);
    drawChunkLoop(numActiveChunks);

    // Pass 2: in-map points (full turbo color) drawn on top
    m_timelineShader.setInt("u_filterMode", 1);
    drawChunkLoop(numActiveChunks);

    glDisable(GL_BLEND);
    glUseProgram(0);
}

size_t PointRenderer::pointCount() const {
    size_t total = 0;
    for (size_t c : m_chunkPointCounts) total += c;
    return total;
}

void PointRenderer::cleanup() {
    if (m_quadVbo) glDeleteBuffers(1, &m_quadVbo);
    if (!m_chunkVbos.empty())
        glDeleteBuffers(static_cast<GLsizei>(m_chunkVbos.size()), m_chunkVbos.data());
    if (!m_chunkVaos.empty())
        glDeleteVertexArrays(static_cast<GLsizei>(m_chunkVaos.size()), m_chunkVaos.data());
}
