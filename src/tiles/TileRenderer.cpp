#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include "tiles/TileRenderer.h"
#include "tiles/TileMath.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

// ---------------------------------------------------------------------------
// Shader source — same vertex format as LineRenderer (vec2 pos + vec4 color)
// ---------------------------------------------------------------------------
static const char* kVertSrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aColor;
uniform mat3 uProjectMat;
out vec4 vColor;
void main() {
    vec3 p = uProjectMat * vec3(aPos, 1.0);
    gl_Position = vec4(p.xy, 0.0, 1.0);
    vColor = aColor;
}
)";

static const char* kFragSrc = R"(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() { FragColor = vColor; }
)";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::cerr << "TileRenderer shader error: " << log << "\n";
        glDeleteShader(s); return 0;
    }
    return s;
}

static GLuint linkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(p, len, nullptr, log.data());
        std::cerr << "TileRenderer link error: " << log << "\n";
        glDeleteProgram(p); return 0;
    }
    return p;
}

// ---------------------------------------------------------------------------

void TileRenderer::init() {
    GLuint vs = compileShader(GL_VERTEX_SHADER,   kVertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragSrc);
    if (vs && fs) {
        m_program  = linkProgram(vs, fs);
        m_uProjLoc = glGetUniformLocation(m_program, "uProjectMat");
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
}

void TileRenderer::shutdown() {
    for (auto& [key, gpu] : m_gpuData)
        deleteGPUData(gpu);
    m_gpuData.clear();

    if (m_program) { glDeleteProgram(m_program); m_program = 0; }
}

// ---------------------------------------------------------------------------

TileRenderer::TileGPUData TileRenderer::uploadTile(const std::vector<TileLine>& lines) {
    // Pack TileLines into interleaved vertex pairs
    std::vector<GLVertex> verts;
    verts.reserve(lines.size() * 2);
    for (const auto& ln : lines) {
        verts.push_back({ln.a.x, ln.a.y, ln.color.r, ln.color.g, ln.color.b, ln.color.a});
        verts.push_back({ln.b.x, ln.b.y, ln.color.r, ln.color.g, ln.color.b, ln.color.a});
    }

    TileGPUData gpu;
    gpu.vertexCount = static_cast<GLsizei>(verts.size());

    glGenVertexArrays(1, &gpu.vao);
    glGenBuffers(1, &gpu.vbo);

    glBindVertexArray(gpu.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(GLVertex)),
                 verts.data(), GL_STATIC_DRAW);

    // aPos  — location 0, vec2
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)0);
    // aColor — location 1, vec4
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                          (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    return gpu;
}

void TileRenderer::deleteGPUData(TileGPUData& gpu) {
    if (gpu.vao) { glDeleteVertexArrays(1, &gpu.vao); gpu.vao = 0; }
    if (gpu.vbo) { glDeleteBuffers(1,      &gpu.vbo); gpu.vbo = 0; }
    gpu.vertexCount = 0;
}

void TileRenderer::evictGPUData() {
    static constexpr uint64_t kEvictAge  = 300; // ~5 s at 60 fps
    static constexpr size_t   kMaxTiles  = 256;

    if (m_gpuData.size() <= kMaxTiles) return;

    std::vector<TileKey> stale;
    for (const auto& [key, gpu] : m_gpuData) {
        if (m_frameCounter - gpu.lastUsedFrame > kEvictAge)
            stale.push_back(key);
    }
    for (const auto& key : stale) {
        deleteGPUData(m_gpuData[key]);
        m_gpuData.erase(key);
    }
}

// ---------------------------------------------------------------------------

void TileRenderer::render(const Camera& camera) {
    if (!m_program) return;

    m_cache.processCompletedFetches();
    m_frameCounter++;

    float lonMin = camera.lonLeft();
    float lonMax = camera.lonRight();
    float latMin = std::max(camera.latBottom(), -85.05f);
    float latMax = std::min(camera.latTop(),     85.05f);
    if (latMin >= latMax) return;

    int zoom = TileMath::zoomForExtent(camera.zoom(), camera.height());
    zoom = std::max(0, std::min(14, zoom));

    int tileXMin = static_cast<int>(std::floor(TileMath::lonToTileX(lonMin, zoom)));
    int tileXMax = static_cast<int>(std::floor(TileMath::lonToTileX(lonMax, zoom)));
    int tileYMin = static_cast<int>(std::floor(TileMath::latToTileY(latMax, zoom)));
    int tileYMax = static_cast<int>(std::floor(TileMath::latToTileY(latMin, zoom)));

    int maxTile = (1 << zoom) - 1;
    tileXMin = std::max(0, tileXMin); tileXMax = std::min(maxTile, tileXMax);
    tileYMin = std::max(0, tileYMin); tileYMax = std::min(maxTile, tileYMax);

    glUseProgram(m_program);
    glUniformMatrix3fv(m_uProjLoc, 1, GL_FALSE, camera.Transform().m);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int ty = tileYMin; ty <= tileYMax; ty++) {
        for (int tx = tileXMin; tx <= tileXMax; tx++) {
            TileKey key{zoom, tx, ty};

            const auto* tileLines = m_cache.requestTile(key);
            if (!tileLines || tileLines->empty()) continue;

            // Upload to GPU on first use
            auto it = m_gpuData.find(key);
            if (it == m_gpuData.end()) {
                m_gpuData[key] = uploadTile(*tileLines);
                it = m_gpuData.find(key);
            }

            TileGPUData& gpu = it->second;
            gpu.lastUsedFrame = m_frameCounter;

            glBindVertexArray(gpu.vao);
            glDrawArrays(GL_LINES, 0, gpu.vertexCount);
        }
    }

    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);

    m_cache.evictOldTiles();
    evictGPUData();
}
