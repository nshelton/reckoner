// Must define GL_GLEXT_PROTOTYPES before the first GL include in this TU
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include "tiles/RasterTileRenderer.h"
#include "tiles/TileMath.h"
#include <algorithm>
#include <cmath>
#include <vector>

#ifndef SHADER_BASE_DIR
#define SHADER_BASE_DIR "src/shaders"
#endif

// Unit quad covering UV [0,1]x[0,1]: two triangles, six vertices (x,y == u,v)
static const float kQuadVertices[] = {
    0.f, 0.f,
    1.f, 0.f,
    1.f, 1.f,
    0.f, 0.f,
    1.f, 1.f,
    0.f, 1.f,
};

void RasterTileRenderer::init() {
    m_shader = Shader::fromFiles(
        SHADER_BASE_DIR "/raster_tile.vert",
        SHADER_BASE_DIR "/raster_tile.frag"
    );

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);

    // Attribute 0: vec2 UV
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);
}

void RasterTileRenderer::shutdown() {
    deleteAllTextures();
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo);      m_vbo = 0; }
    m_cache.reset();
}

void RasterTileRenderer::setUrlTemplate(const std::string& urlTemplate) {
    deleteAllTextures();
    m_frameCounter = 0;
    m_cache = std::make_unique<RasterTileCache>(urlTemplate);
}

void RasterTileRenderer::deleteAllTextures() {
    for (auto& [key, info] : m_textures) {
        if (info.texId) glDeleteTextures(1, &info.texId);
    }
    m_textures.clear();
}

GLuint RasterTileRenderer::createTexture(const RasterTileEntry& entry) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 entry.width, entry.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, entry.pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void RasterTileRenderer::render(const Camera& camera) {
    if (!m_cache || !m_shader.valid() || !m_vao) return;

    m_cache->processCompletedFetches();
    m_frameCounter++;

    float lonMin = camera.lonLeft();
    float lonMax = camera.lonRight();
    float latMin = std::max(camera.latBottom(), -85.05f);
    float latMax = std::min(camera.latTop(),     85.05f);
    if (latMin >= latMax) return;

    // Raster tiles are 256 px; cap zoom at 19
    int zoom = TileMath::zoomForExtent(camera.zoom(), camera.height());
    zoom = std::max(0, std::min(19, zoom));

    int tileXMin = static_cast<int>(std::floor(TileMath::lonToTileX(lonMin, zoom)));
    int tileXMax = static_cast<int>(std::floor(TileMath::lonToTileX(lonMax, zoom)));
    int tileYMin = static_cast<int>(std::floor(TileMath::latToTileY(latMax, zoom)));
    int tileYMax = static_cast<int>(std::floor(TileMath::latToTileY(latMin, zoom)));

    int maxTile = (1 << zoom) - 1;
    tileXMin = std::max(0, tileXMin);
    tileXMax = std::min(maxTile, tileXMax);
    tileYMin = std::max(0, tileYMin);
    tileYMax = std::min(maxTile, tileYMax);

    const float* vp = camera.Transform().m;

    m_shader.use();
    m_shader.setMat3("u_viewProjection", vp);

    glActiveTexture(GL_TEXTURE0);
    m_shader.setInt("u_texture", 0);

    glBindVertexArray(m_vao);

    for (int ty = tileYMin; ty <= tileYMax; ty++) {
        for (int tx = tileXMin; tx <= tileXMax; tx++) {
            TileKey key{zoom, tx, ty};

            const RasterTileEntry* entry = m_cache->requestTile(key);
            if (!entry) continue;

            // Create GL texture on first use
            auto& info = m_textures[key];
            if (!info.texId) {
                info.texId = createTexture(*entry);
            }
            info.lastUsedFrame = m_frameCounter;

            // Geographic bounds of this tile
            float lonL = static_cast<float>(TileMath::tileXToLon(tx,     zoom));
            float lonR = static_cast<float>(TileMath::tileXToLon(tx + 1, zoom));
            float latT = static_cast<float>(TileMath::tileYToLat(ty,     zoom));
            float latB = static_cast<float>(TileMath::tileYToLat(ty + 1, zoom));

            m_shader.setFloat("u_lonLeft",   lonL);
            m_shader.setFloat("u_lonRight",  lonR);
            m_shader.setFloat("u_latBottom", latB);
            m_shader.setFloat("u_latTop",    latT);

            glBindTexture(GL_TEXTURE_2D, info.texId);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Evict pixel data from CPU cache
    m_cache->evictOldTiles();

    // Evict GL textures that haven't been used for many frames
    static constexpr uint64_t kTextureEvictAge = 300; // ~5s at 60fps
    static constexpr size_t   kMaxTextures     = 256;

    if (m_textures.size() > kMaxTextures) {
        std::vector<TileKey> stale;
        for (const auto& [key, info] : m_textures) {
            if (m_frameCounter - info.lastUsedFrame > kTextureEvictAge)
                stale.push_back(key);
        }
        for (const auto& key : stale) {
            GLuint tex = m_textures[key].texId;
            if (tex) glDeleteTextures(1, &tex);
            m_textures.erase(key);
        }
    }
}
