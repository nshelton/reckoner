#include "MoonAltitudeRenderer.h"
#include "core/SolarCalculations.h"

#include <cmath>

#ifndef SHADER_BASE_DIR
#define SHADER_BASE_DIR "src/shaders"
#endif

void MoonAltitudeRenderer::init()
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

void MoonAltitudeRenderer::shutdown()
{
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo);       m_vbo = 0; }
}

void MoonAltitudeRenderer::draw(const Mat3& viewProjection,
                                 double timeStart, double timeEnd,
                                 int numSamples)
{
    if (!m_shader.valid() || numSamples < 2 || timeStart >= timeEnd)
        return;

    // Build a triangle strip from (t, -1) to (t, y_phase) for each sample.
    // illumination in [0,1] maps to NDC y in [-1, +1]: y = 2*illum - 1
    double step = (timeEnd - timeStart) / (numSamples - 1);

    m_vertices.clear();
    m_vertices.reserve(numSamples * 2);

    for (int i = 0; i < numSamples; i++) {
        double t     = timeStart + i * step;
        double illum = SolarCalc::moonIlluminationFraction(t);
        float  yTop  = static_cast<float>(2.0 * illum - 1.0);  // [-1, +1]

        m_vertices.push_back({static_cast<float>(t), -1.0f});
        m_vertices.push_back({static_cast<float>(t),  yTop});
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
                 m_vertices.data(),
                 GL_DYNAMIC_DRAW);

    m_shader.use();
    m_shader.setMat3("u_viewProjection", viewProjection.m);
    m_shader.setVec4("u_color", 0.82f, 0.85f, 0.90f, 0.25f);  // silver-white

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(m_vertices.size()));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glUseProgram(0);
}
