#include "HistogramRenderer.h"

#include <algorithm>

#ifndef SHADER_BASE_DIR
#define SHADER_BASE_DIR "src/shaders"
#endif

void HistogramRenderer::init() {
    m_shader = Shader::fromFiles(
        SHADER_BASE_DIR "/histogram.vert",
        SHADER_BASE_DIR "/histogram.frag"
    );

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Attrib 0: vec2 position (time_x, y)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    glBindVertexArray(0);
}

void HistogramRenderer::shutdown() {
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
}

void HistogramRenderer::draw(const Mat3& viewProjection,
                              const std::vector<Entity>& entities,
                              double timeStart, double timeEnd,
                              int numBins) {
    if (!m_shader.valid() || entities.empty() || numBins <= 0 || timeStart >= timeEnd)
        return;

    // --- Bin entities by time_mid ---
    std::vector<int> bins(numBins, 0);
    double range = timeEnd - timeStart;
    for (const auto& e : entities) {
        double t = e.time_mid();
        if (t < timeStart || t >= timeEnd) continue;
        int bin = static_cast<int>((t - timeStart) / range * numBins);
        bin = std::clamp(bin, 0, numBins - 1);
        bins[bin]++;
    }

    int maxCount = *std::max_element(bins.begin(), bins.end());
    if (maxCount == 0) return;

    // --- Build bar geometry (2 triangles = 6 vertices per bin) ---
    // The timeline camera maps Y in [-1, 1] linearly to NDC, so we work directly
    // in that space: bars grow from yBot=-1 (bottom edge) up to yTop.
    m_vertices.clear();
    m_vertices.reserve(numBins * 6);

    double binWidth = range / numBins;
    for (int i = 0; i < numBins; i++) {
        if (bins[i] == 0) continue;

        float x0   = static_cast<float>(timeStart + i       * binWidth);
        float x1   = static_cast<float>(timeStart + (i + 1) * binWidth);
        float yBot = -1.0f;
        float yTop = -1.0f + 2.0f * static_cast<float>(bins[i]) / static_cast<float>(maxCount);

        // Triangle 1
        m_vertices.push_back({x0, yBot});
        m_vertices.push_back({x1, yBot});
        m_vertices.push_back({x0, yTop});
        // Triangle 2
        m_vertices.push_back({x1, yBot});
        m_vertices.push_back({x1, yTop});
        m_vertices.push_back({x0, yTop});
    }

    if (m_vertices.empty()) return;

    // --- Upload ---
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
                 m_vertices.data(),
                 GL_DYNAMIC_DRAW);

    // --- Draw ---
    m_shader.use();
    m_shader.setMat3("u_viewProjection", viewProjection.m);
    m_shader.setVec4("u_color", 0.35f, 0.65f, 1.0f, 0.35f);  // translucent blue

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glUseProgram(0);
}

void HistogramRenderer::drawRects(const Mat3& viewProjection,
                                   const std::vector<TimeRange>& rects,
                                   float y0, float y1,
                                   float r, float g, float b, float a)
{
    if (!m_shader.valid() || rects.empty()) return;

    m_vertices.clear();
    m_vertices.reserve(rects.size() * 6);

    for (const auto& rect : rects) {
        m_vertices.push_back({rect.x0, y0});
        m_vertices.push_back({rect.x1, y0});
        m_vertices.push_back({rect.x0, y1});
        m_vertices.push_back({rect.x1, y0});
        m_vertices.push_back({rect.x1, y1});
        m_vertices.push_back({rect.x0, y1});
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
                 m_vertices.data(),
                 GL_DYNAMIC_DRAW);

    m_shader.use();
    m_shader.setMat3("u_viewProjection", viewProjection.m);
    m_shader.setVec4("u_color", r, g, b, a);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glUseProgram(0);
}
