#pragma once

#include "core/Vec2.h"
#include "core/Color.h"
#include "core/Mat3.h"
#include <vector>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

/// Vertex data for a single point instance
struct PointVertex {
    Vec2 position;
    Color color;
    float size;
};

/// GPU-accelerated point renderer using instanced rendering.
///
/// Supports two modes:
/// 1. Chunked mode: a ring buffer of GPU VBOs, upload each chunk once.
///    Use updateChunk() + drawChunked().
/// 2. Streaming mode: rebuild every frame via begin/addPoint/end.
///    Used when the visible set changes each frame (e.g. timeline).
class PointRenderer {
public:
    static constexpr size_t CHUNK_SIZE = 5000;
    static constexpr size_t NUM_CHUNKS = 10;

    PointRenderer();
    ~PointRenderer();

    // --- Chunked mode (map renderer) ---
    void updateChunk(size_t chunkIndex, const PointVertex* data, size_t count);
    void drawChunked(const Mat3& viewProjection, float aspectRatio, size_t numActiveChunks);

    // --- Streaming mode (timeline renderer) ---
    void begin(const Mat3& viewProjection, float aspectRatio);
    void addPoint(const Vec2& pos, const Color& color, float size = 1.0f);
    void end();

    /// Total points across all chunked buffers
    size_t pointCount() const;

private:
    // Shared
    GLuint m_quadVbo = 0;
    GLuint m_shader = 0;

    // Chunked mode state
    GLuint m_chunkVaos[NUM_CHUNKS] = {};
    GLuint m_chunkVbos[NUM_CHUNKS] = {};
    size_t m_chunkPointCounts[NUM_CHUNKS] = {};

    // Streaming mode state
    GLuint m_streamVao = 0;
    GLuint m_streamVbo = 0;
    std::vector<PointVertex> m_streamPoints;
    Mat3 m_viewProjection;
    float m_aspectRatio = 1.0f;

    void initShaders();
    void initBuffers();
    void cleanup();
    void setupInstanceAttribs(GLuint vbo);
};
