#pragma once

#include "core/Vec2.h"
#include "core/Mat3.h"
#include "renderer/Shader.h"
#include <vector>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

/// Vertex data for a single point instance.
/// Stores both coordinate spaces so the same VBO can be drawn by the map
/// and the timeline using different shaders.
struct PointVertex {
    Vec2  geo_pos;       // (lon, lat)               — map shader, attrib 1
    float time_mid;      // (time_start+time_end)/2  — timeline x + color, attrib 2
    float render_offset; // entity.render_offset      — timeline y, attrib 3
};

/// GPU-accelerated point renderer using instanced rendering.
///
/// Entities are split into fixed-size chunks; each chunk is uploaded once
/// and reused every frame.  Two draw paths share the same VBOs:
///   drawChunked()      — map view    (geo_pos → viewProjection transform)
///   drawForTimeline()  — timeline    (time_mid/render_offset → timeline transform)
class PointRenderer {
public:
    static constexpr size_t CHUNK_SIZE = 50000;

    PointRenderer();
    ~PointRenderer();

    // --- Chunk management ---
    void ensureChunks(size_t numChunks);
    void updateChunk(size_t chunkIndex, const PointVertex* data, size_t count);

    // --- Draw paths ---
    /// Map view: transforms geo_pos with viewProjection.
    /// colorMode: 0=turbo colormap, 1=solid baseColor (r,g,b,a).
    void drawChunked(const Mat3& viewProjection, float aspectRatio, size_t numActiveChunks,
                     float timeMin = 0.0f, float timeMax = 1.0f,
                     int colorMode = 0, float br = 1, float bg = 1, float bb = 1, float ba = 1);

    /// Map viewport bounds passed to the timeline shader so it can dim points outside the map view
    struct MapExtent {
        float minLon = -180.f, maxLon = 180.f;
        float minLat =  -90.f, maxLat =  90.f;
    };

    /// Timeline view: transforms (time_mid, render_offset) with viewProjection.
    /// Points whose geo_pos falls outside mapExtent are desaturated.
    /// colorMode: 0=turbo colormap, 1=solid baseColor (r,g,b,a).
    /// yOffset: screen-space NDC Y shift applied after projection (positive = up).
    void drawForTimeline(const Mat3& viewProjection, float aspectRatio, size_t numActiveChunks,
                         float timeMin, float timeMax, const MapExtent& mapExtent,
                         int colorMode = 0, float br = 1, float bg = 1, float bb = 1, float ba = 1,
                         float yOffset = 0.0f);

    // --- Stats ---
    size_t pointCount() const;
    size_t numChunks() const { return m_chunkVaos.size(); }

    float getPointSize() const { return m_size; }
    void  setPointSize(float size) { m_size = size; }

private:
    GLuint m_quadVbo = 0;
    Shader m_mapShader;
    Shader m_timelineShader;

    std::vector<GLuint> m_chunkVaos;
    std::vector<GLuint> m_chunkVbos;
    std::vector<size_t> m_chunkPointCounts;

    float m_size = 1.0f;

    void initShaders();
    void initBuffers();
    void cleanup();
    void allocateChunk();
    // Shared draw loop — shader must already be bound and uniforms set
    void drawChunkLoop(size_t numActiveChunks);
};
