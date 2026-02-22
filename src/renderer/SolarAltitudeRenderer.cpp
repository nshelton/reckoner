#include "SolarAltitudeRenderer.h"
#include "core/SolarCalculations.h"

#include <algorithm>
#include <cmath>

#ifndef SHADER_BASE_DIR
#define SHADER_BASE_DIR "src/shaders"
#endif

void SolarAltitudeRenderer::init()
{
    m_shader = Shader::fromFiles(
        SHADER_BASE_DIR "/histogram.vert",
        SHADER_BASE_DIR "/histogram.frag"
    );

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    glBindVertexArray(0);
}

void SolarAltitudeRenderer::shutdown()
{
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo);       m_vbo = 0; }
}

void SolarAltitudeRenderer::uploadAndDraw(float r, float g, float b, float a,
                                           const Mat3& viewProjection)
{
    if (m_vertices.empty()) return;

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
    glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(m_vertices.size()));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glUseProgram(0);
}

void SolarAltitudeRenderer::draw(const Mat3& viewProjection,
                                  double timeStart, double timeEnd,
                                  double latDeg, double lonDeg,
                                  int numSamples)
{
    if (!m_shader.valid() || numSamples < 2 || timeStart >= timeEnd)
        return;

    // Sample the solar altitude at evenly-spaced time steps.
    double step = (timeEnd - timeStart) / (numSamples - 1);
    std::vector<float> times(numSamples);
    std::vector<float> alts(numSamples);   // in NDC: [-1, 1] = [-90°, 90°]

    for (int i = 0; i < numSamples; i++) {
        double t   = timeStart + i * step;
        double alt = SolarCalc::solarAltitudeDeg(latDeg, lonDeg, t);
        times[i] = static_cast<float>(t);
        alts[i]  = static_cast<float>(std::clamp(alt / 90.0, -1.0, 1.0));
    }

    // --- Above-horizon fill (altitude > 0): warm amber ---
    // Triangle strip from y=0 up to altitude for samples where alt > 0.
    // We emit degenerate triangles (duplicate vertices) to restart the strip
    // without actually breaking it into multiple draw calls.
    m_vertices.clear();
    m_vertices.reserve(numSamples * 2 + 4);

    bool inStrip = false;
    for (int i = 0; i < numSamples; i++) {
        float yTop = std::max(0.0f, alts[i]);

        if (yTop > 0.0f) {
            if (!inStrip) {
                // Start a new degenerate section by duplicating the first vertex
                if (!m_vertices.empty()) {
                    m_vertices.push_back(m_vertices.back()); // degenerate
                    m_vertices.push_back({times[i], 0.0f});   // degenerate
                }
                inStrip = true;
            }
            m_vertices.push_back({times[i], 0.0f});
            m_vertices.push_back({times[i], yTop});
        } else {
            inStrip = false;
        }
    }

    // Warm amber/gold: rgba(0.98, 0.72, 0.15, 0.30)
    uploadAndDraw(0.98f, 0.72f, 0.15f, 0.30f, viewProjection);

    // --- Below-horizon fill (altitude < 0): cool navy ---
    m_vertices.clear();
    m_vertices.reserve(numSamples * 2 + 4);

    inStrip = false;
    for (int i = 0; i < numSamples; i++) {
        float yBot = std::min(0.0f, alts[i]);

        if (yBot < 0.0f) {
            if (!inStrip) {
                if (!m_vertices.empty()) {
                    m_vertices.push_back(m_vertices.back());
                    m_vertices.push_back({times[i], yBot});
                }
                inStrip = true;
            }
            m_vertices.push_back({times[i], yBot});
            m_vertices.push_back({times[i], 0.0f});
        } else {
            inStrip = false;
        }
    }

    // Cool navy: rgba(0.15, 0.30, 0.70, 0.22)
    uploadAndDraw(0.15f, 0.30f, 0.70f, 0.22f, viewProjection);
}
