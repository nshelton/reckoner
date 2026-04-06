#pragma once

#include "tiles/TileCache.h"
#include "Camera.h"

#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#ifdef __APPLE__
#include <OpenGL/glext.h>
#else
#include <GL/glext.h>
#endif

#include <unordered_map>
#include <cstdint>

/// Renders vector tile data as lines, with one VAO/VBO per tile uploaded once to GPU.
/// Tiles are drawn directly each frame with no CPU-side line copying.
class TileRenderer {
public:
    TileRenderer() = default;
    ~TileRenderer() { shutdown(); }

    void init();
    void shutdown();

    /// Determine visible tiles, upload new arrivals to GPU, and draw all visible tiles.
    void render(const Camera& camera);

private:
    struct GLVertex { float x, y, r, g, b, a; };

    struct TileGPUData {
        GLuint   vao           = 0;
        GLuint   vbo           = 0;
        GLsizei  vertexCount   = 0;
        uint64_t lastUsedFrame = 0;
    };

    TileCache m_cache;
    std::unordered_map<TileKey, TileGPUData, TileKeyHash> m_gpuData;
    uint64_t m_frameCounter = 0;

    GLuint m_program  = 0;
    GLint  m_uProjLoc = -1;

    /// Upload a tile's lines to a new VAO/VBO. Called once per tile arrival.
    TileGPUData uploadTile(const std::vector<TileLine>& lines);

    /// Delete GL objects for one tile.
    void deleteGPUData(TileGPUData& gpu);

    /// LRU-evict GPU data for tiles that haven't been drawn recently.
    void evictGPUData();
};
