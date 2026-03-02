#pragma once

#include "tiles/RasterTileCache.h"
#include "Camera.h"
#include "renderer/Shader.h"

#include <unordered_map>
#include <string>
#include <memory>
#include <cstdint>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

/// Renders raster (PNG) map tiles as textured quads positioned in lon/lat space.
///
/// Usage:
///   init()                  — create GL objects, compile shader
///   setUrlTemplate(tmpl)    — switch tile source (clears existing cache + textures)
///   render(camera) per frame
///   shutdown()              — release GL resources
class RasterTileRenderer {
public:
    RasterTileRenderer() = default;
    ~RasterTileRenderer() { shutdown(); }

    void init();
    void shutdown();

    /// Change the raster tile URL template (e.g. "https://…/{z}/{x}/{y}.png").
    /// Clears all cached pixel data and GL textures.
    void setUrlTemplate(const std::string& urlTemplate);

    /// Render all visible raster tiles for the given camera view.
    void render(const Camera& camera);

private:
    struct TexInfo {
        GLuint   texId         = 0;
        uint64_t lastUsedFrame = 0;
    };

    std::unique_ptr<RasterTileCache> m_cache;
    std::unordered_map<TileKey, TexInfo, TileKeyHash> m_textures;
    uint64_t m_frameCounter = 0;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    Shader m_shader;

    void deleteAllTextures();
    GLuint createTexture(const RasterTileEntry& entry);
};
