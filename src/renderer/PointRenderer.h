#pragma once

#include "core/Vec2.h"
#include "core/Color.h"
#include "core/Mat3.h"
#include "renderer/Shader.h"
#include <vector>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

/// Vertex data for a single point instance
struct PointVertex {
    Vec2 position;
    float timestamp;  // Unix epoch seconds, used for turbo colormap
};

/// GPU-accelerated point renderer using instanced rendering.
///
/// Supports two modes:
/// 1. Chunked mode: dynamically allocated GPU VBOs, upload each chunk once.
///    Use updateChunk() + drawChunked().
/// 2. Streaming mode: rebuild every frame via begin/addPoint/end.
///    Used when the visible set changes each frame (e.g. timeline).
class PointRenderer {
public:
    static constexpr size_t CHUNK_SIZE = 50000;

    PointRenderer();
    ~PointRenderer();

    // --- Chunked mode (map renderer) ---
    /// Ensure at least numChunks GPU buffers are allocated
    void ensureChunks(size_t numChunks);
    void updateChunk(size_t chunkIndex, const PointVertex* data, size_t count);
    void drawChunked(const Mat3& viewProjection, float aspectRatio, size_t numActiveChunks,
                     float timeMin = 0.0f, float timeMax = 1.0f);

    // --- Streaming mode (timeline renderer) ---
    void begin(const Mat3& viewProjection, float aspectRatio);
    void addPoint(const Vec2& pos, float timestamp = 0.0f);
    void end(float timeMin = 0.0f, float timeMax = 0.0f);

    /// Total points across all chunked buffers
    size_t pointCount() const;

    /// Number of allocated chunks
    size_t numChunks() const { return m_chunkVaos.size(); }

    float getPointSize() const { return m_size; }
    void setPointSize(float size) { this->m_size = size; }


private:
    // Shared
    GLuint m_quadVbo = 0;
    Shader m_shader;

    // Chunked mode state (dynamically sized)
    std::vector<GLuint> m_chunkVaos;
    std::vector<GLuint> m_chunkVbos;
    std::vector<size_t> m_chunkPointCounts;

    // Streaming mode state
    GLuint m_streamVao = 0;
    GLuint m_streamVbo = 0;
    std::vector<PointVertex> m_streamPoints;
    Mat3 m_viewProjection;
    float m_aspectRatio = 1.0f;

    float m_size = 1;

    void initShaders();
    void initBuffers();
    void cleanup();
    void setupInstanceAttribs(GLuint vbo);
    void allocateChunk();  // Allocate one new chunk VAO/VBO
};
