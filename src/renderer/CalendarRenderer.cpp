#include "CalendarRenderer.h"
#include <cstddef>

#ifndef SHADER_BASE_DIR
#define SHADER_BASE_DIR "src/shaders"
#endif

// Parse a "#RRGGBB" hex color string into float RGB components [0, 1].
// Falls back to a neutral gray if the string is malformed.
static void parseHexColor(const std::string& hex, float& r, float& g, float& b) {
    r = g = b = 0.5f;
    if (hex.size() < 7 || hex[0] != '#') return;

    auto hv = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };

    r = static_cast<float>(hv(hex[1]) * 16 + hv(hex[2])) / 255.0f;
    g = static_cast<float>(hv(hex[3]) * 16 + hv(hex[4])) / 255.0f;
    b = static_cast<float>(hv(hex[5]) * 16 + hv(hex[6])) / 255.0f;
}

void CalendarRenderer::init() {
    initShaders();
    initBuffers();
}

void CalendarRenderer::initShaders() {
    m_shader = Shader::fromFiles(
        SHADER_BASE_DIR "/calendar_timeline.vert",
        SHADER_BASE_DIR "/calendar.frag"
    );
}

void CalendarRenderer::initBuffers() {
    // Unit quad (triangle strip): two triangles covering [-1, 1] x [-1, 1]
    float quad[] = { -1.f, -1.f,  1.f, -1.f,  -1.f,  1.f,  1.f,  1.f };

    glGenBuffers(1, &m_quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    // Instance VBO — sized lazily on first draw
    glGenBuffers(1, &m_instanceVbo);

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // Attrib 0: quad corner (per-vertex, divisor=0)
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glVertexAttribDivisor(0, 0);

    // Attribs 1-4: per-instance data from m_instanceVbo
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);

    glEnableVertexAttribArray(1);  // time_start
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(CalendarEventVertex),
                          (void*)offsetof(CalendarEventVertex, time_start));
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);  // time_end
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(CalendarEventVertex),
                          (void*)offsetof(CalendarEventVertex, time_end));
    glVertexAttribDivisor(2, 1);

    glEnableVertexAttribArray(3);  // render_offset
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(CalendarEventVertex),
                          (void*)offsetof(CalendarEventVertex, render_offset));
    glVertexAttribDivisor(3, 1);

    glEnableVertexAttribArray(4);  // r, g, b, a
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(CalendarEventVertex),
                          (void*)offsetof(CalendarEventVertex, r));
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);
}

void CalendarRenderer::draw(const Mat3& viewProjection,
                             const std::vector<Entity>& entities,
                             float yOffset,
                             int   viewportWidth) {
    if (entities.empty() || !m_shader.valid()) return;

    // Build instance data
    m_buf.clear();
    m_buf.reserve(entities.size());

    for (const auto& e : entities) {
        float r, g, b;
        if (e.color.has_value()) {
            parseHexColor(*e.color, r, g, b);
        } else {
            r = 0.298f; g = 0.686f; b = 0.314f;  // #4CAF50 fallback
        }

        float t_end = (e.time_end > e.time_start) ? static_cast<float>(e.time_end)
                                                   : static_cast<float>(e.time_start) + 1.0f;
        m_buf.push_back({
            static_cast<float>(e.time_start),
            t_end,
            e.render_offset,
            r, g, b, 0.85f
        });
    }

    // Upload — reallocate only when capacity is exceeded
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    size_t needed = m_buf.size() * sizeof(CalendarEventVertex);
    if (m_buf.size() > m_instanceCapacity) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(needed),
                     m_buf.data(), GL_STREAM_DRAW);
        m_instanceCapacity = m_buf.size();
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(needed), m_buf.data());
    }

    // Minimum half-width: at least 3px in NDC so tiny/instant events stay visible
    float minHalfW = (viewportWidth > 0) ? (3.0f / static_cast<float>(viewportWidth)) : 0.003f;

    m_shader.use();
    m_shader.setMat3 ("u_viewProjection",  viewProjection.m);
    m_shader.setFloat("u_yOffset",         yOffset);
    m_shader.setFloat("u_barHalfHeight",   0.04f);
    m_shader.setFloat("u_minBarHalfWidth", minHalfW);

    glBindVertexArray(m_vao);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
                          static_cast<GLsizei>(m_buf.size()));
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);
}

void CalendarRenderer::shutdown() {
    if (m_vao)         { glDeleteVertexArrays(1, &m_vao);         m_vao = 0; }
    if (m_quadVbo)     { glDeleteBuffers(1, &m_quadVbo);          m_quadVbo = 0; }
    if (m_instanceVbo) { glDeleteBuffers(1, &m_instanceVbo);      m_instanceVbo = 0; }
}
