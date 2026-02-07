#pragma once

#include "core/Vec2.h"
#include "core/Color.h"
#include "core/Mat3.h"
#include <vector>
#include <cstdint>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

/// GPU-accelerated bitmap text renderer using an embedded 8x8 monospace font.
/// Glyph anchor positions are in world space (transformed by viewProjection).
/// Glyph sizing is in NDC space (constant screen size regardless of zoom).
class TextRenderer
{
public:
    TextRenderer();
    ~TextRenderer();

    void init();
    void shutdown();

    /// Begin a text rendering batch.
    /// aspectRatio = viewport width / height (for square glyphs on screen)
    void begin(const Mat3 &viewProjection, float aspectRatio);

    /// Add a string at a world-space position.
    /// size: glyph height as fraction of viewport height (e.g. 0.04 = 4%)
    /// anchor: 0.0 = left-aligned, 0.5 = centered, 1.0 = right-aligned
    void addText(const char *text, Vec2 pos, Color color, float size, float anchor = 0.0f);

    /// Flush all text to GPU and render
    void end();

private:
    struct GlyphVertex
    {
        Vec2 position;    // world-space anchor position (same for all glyphs in a string)
        Color color;
        float size;        // NDC height (fraction of viewport)
        float charCol;     // column in the atlas (0-15)
        float charRow;     // row in the atlas (0-5)
        float glyphIndex;  // horizontal offset index within the string
    };

    std::vector<GlyphVertex> m_glyphs;
    Mat3 m_viewProjection;
    float m_aspectRatio{1.0f};

    GLuint m_vao = 0;
    GLuint m_quadVbo = 0;
    GLuint m_instanceVbo = 0;
    GLuint m_shader = 0;
    GLuint m_fontTexture = 0;

    void initShaders();
    void initBuffers();
    void initFontTexture();
    void cleanup();
};
