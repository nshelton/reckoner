#include "TextRenderer.h"
#include <iostream>
#include <cstring>

// ── Embedded 8x8 bitmap font ──────────────────────────────────────────────
// Covers ASCII 32 (space) through 127 (DEL) = 96 characters.
// Each character is 8 bytes, one byte per row, MSB = leftmost pixel.
// Arranged in a 16x6 atlas (16 columns, 6 rows).
// clang-format off
static const uint8_t kFontData[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 ' '
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // 33 '!'
    {0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00}, // 34 '"'
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, // 35 '#'
    {0x18,0x7E,0xC0,0x7C,0x06,0xFC,0x18,0x00}, // 36 '$'
    {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00}, // 37 '%'
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // 38 '&'
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, // 39 '''
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // 40 '('
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // 41 ')'
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // 42 '*'
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // 43 '+'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // 44 ','
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // 45 '-'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // 46 '.'
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, // 47 '/'
    {0x7C,0xC6,0xCE,0xDE,0xF6,0xE6,0x7C,0x00}, // 48 '0'
    {0x18,0x38,0x78,0x18,0x18,0x18,0x7E,0x00}, // 49 '1'
    {0x7C,0xC6,0x06,0x1C,0x30,0x66,0xFE,0x00}, // 50 '2'
    {0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00}, // 51 '3'
    {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00}, // 52 '4'
    {0xFE,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00}, // 53 '5'
    {0x38,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00}, // 54 '6'
    {0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00}, // 55 '7'
    {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00}, // 56 '8'
    {0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00}, // 57 '9'
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // 58 ':'
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // 59 ';'
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // 60 '<'
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // 61 '='
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, // 62 '>'
    {0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00}, // 63 '?'
    {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00}, // 64 '@'
    {0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00}, // 65 'A'
    {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00}, // 66 'B'
    {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00}, // 67 'C'
    {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00}, // 68 'D'
    {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00}, // 69 'E'
    {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00}, // 70 'F'
    {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00}, // 71 'G'
    {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // 72 'H'
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 73 'I'
    {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00}, // 74 'J'
    {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00}, // 75 'K'
    {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00}, // 76 'L'
    {0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0x00}, // 77 'M'
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, // 78 'N'
    {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // 79 'O'
    {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00}, // 80 'P'
    {0x7C,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x06}, // 81 'Q'
    {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00}, // 82 'R'
    {0x7C,0xC6,0xC0,0x7C,0x06,0xC6,0x7C,0x00}, // 83 'S'
    {0x7E,0x5A,0x18,0x18,0x18,0x18,0x3C,0x00}, // 84 'T'
    {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // 85 'U'
    {0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00}, // 86 'V'
    {0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00}, // 87 'W'
    {0xC6,0xC6,0x6C,0x38,0x6C,0xC6,0xC6,0x00}, // 88 'X'
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00}, // 89 'Y'
    {0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00}, // 90 'Z'
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // 91 '['
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, // 92 '\'
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // 93 ']'
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, // 94 '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // 95 '_'
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // 96 '`'
    {0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00}, // 97 'a'
    {0xE0,0x60,0x7C,0x66,0x66,0x66,0xDC,0x00}, // 98 'b'
    {0x00,0x00,0x7C,0xC6,0xC0,0xC6,0x7C,0x00}, // 99 'c'
    {0x1C,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00}, // 100 'd'
    {0x00,0x00,0x7C,0xC6,0xFE,0xC0,0x7C,0x00}, // 101 'e'
    {0x1C,0x36,0x30,0x78,0x30,0x30,0x78,0x00}, // 102 'f'
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x78}, // 103 'g'
    {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00}, // 104 'h'
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // 105 'i'
    {0x06,0x00,0x0E,0x06,0x06,0x66,0x66,0x3C}, // 106 'j'
    {0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00}, // 107 'k'
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 108 'l'
    {0x00,0x00,0xCC,0xFE,0xD6,0xD6,0xC6,0x00}, // 109 'm'
    {0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x00}, // 110 'n'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0x00}, // 111 'o'
    {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0}, // 112 'p'
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E}, // 113 'q'
    {0x00,0x00,0xDC,0x76,0x60,0x60,0xF0,0x00}, // 114 'r'
    {0x00,0x00,0x7C,0xC0,0x7C,0x06,0xFC,0x00}, // 115 's'
    {0x30,0x30,0x7C,0x30,0x30,0x36,0x1C,0x00}, // 116 't'
    {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00}, // 117 'u'
    {0x00,0x00,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, // 118 'v'
    {0x00,0x00,0xC6,0xD6,0xD6,0xFE,0x6C,0x00}, // 119 'w'
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, // 120 'x'
    {0x00,0x00,0xC6,0xC6,0xCE,0x76,0x06,0x7C}, // 121 'y'
    {0x00,0x00,0xFE,0x0C,0x38,0x60,0xFE,0x00}, // 122 'z'
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // 123 '{'
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // 124 '|'
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // 125 '}'
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}, // 126 '~'
    {0x00,0x10,0x38,0x6C,0xC6,0xC6,0xFE,0x00}, // 127 DEL
};
// clang-format on

// ── Atlas layout: 16 columns x 6 rows, each cell 8x8 pixels ──
static const int kAtlasCols = 16;
static const int kAtlasRows = 6;
static const int kAtlasWidth = kAtlasCols * 8;   // 128
static const int kAtlasHeight = kAtlasRows * 8;  // 48

// ── Shaders ────────────────────────────────────────────────────────────────

static const char *TEXT_VERTEX_SHADER = R"(
#version 330 core

// Per-vertex (quad corner)
layout(location = 0) in vec2 in_quad_vertex;  // 0..1 range

// Per-instance
layout(location = 1) in vec2 in_position;     // world-space anchor position
layout(location = 2) in vec4 in_color;
layout(location = 3) in float in_size;        // NDC height (fraction of viewport)
layout(location = 4) in float in_charCol;     // atlas column (0-15)
layout(location = 5) in float in_charRow;     // atlas row (0-5)
layout(location = 6) in float in_glyphIndex;  // horizontal position in string

out vec4 v_color;
out vec2 v_texCoord;

uniform mat3 u_viewProjection;
uniform float u_aspectRatio;

void main() {
    // Transform anchor position to NDC
    vec3 center_ndc = u_viewProjection * vec3(in_position, 1.0);

    // Glyph cell size in NDC
    float cellW = in_size / u_aspectRatio;  // width adjusted for aspect
    float cellH = in_size;                   // height

    // Position this glyph: offset by glyphIndex horizontally, then apply quad vertex
    vec2 offset_ndc = vec2(
        (in_glyphIndex + in_quad_vertex.x) * cellW,
        in_quad_vertex.y * cellH
    );

    gl_Position = vec4(center_ndc.xy + offset_ndc, 0.0, 1.0);

    // Compute texture coordinates into the atlas
    float cellU = 1.0 / 16.0;
    float cellV = 1.0 / 6.0;

    v_texCoord = vec2(
        (in_charCol + in_quad_vertex.x) * cellU,
        (in_charRow + (1.0 - in_quad_vertex.y)) * cellV
    );

    v_color = in_color;
}
)";

static const char *TEXT_FRAGMENT_SHADER = R"(
#version 330 core

in vec4 v_color;
in vec2 v_texCoord;

out vec4 out_color;

uniform sampler2D u_fontAtlas;

void main() {
    float texel = texture(u_fontAtlas, v_texCoord).r;
    if (texel < 0.5) discard;
    out_color = v_color;
}
)";

// ── Helper ─────────────────────────────────────────────────────────────────

static GLuint compileShader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "TextRenderer shader compilation failed: " << log << std::endl;
        return 0;
    }
    return shader;
}

// ── TextRenderer implementation ────────────────────────────────────────────

TextRenderer::TextRenderer() {}

TextRenderer::~TextRenderer()
{
    cleanup();
}

void TextRenderer::init()
{
    initFontTexture();
    initShaders();
    initBuffers();
}

void TextRenderer::shutdown()
{
    cleanup();
}

void TextRenderer::initFontTexture()
{
    // Build atlas: 128x48, one byte per pixel (GL_RED)
    uint8_t atlas[kAtlasHeight][kAtlasWidth];
    std::memset(atlas, 0, sizeof(atlas));

    for (int ch = 0; ch < 96; ch++)
    {
        int col = ch % kAtlasCols;
        int row = ch / kAtlasCols;
        int baseX = col * 8;
        int baseY = row * 8;

        for (int y = 0; y < 8; y++)
        {
            uint8_t rowBits = kFontData[ch][y];
            for (int x = 0; x < 8; x++)
            {
                if (rowBits & (0x80 >> x))
                    atlas[baseY + y][baseX + x] = 255;
            }
        }
    }

    glGenTextures(1, &m_fontTexture);
    glBindTexture(GL_TEXTURE_2D, m_fontTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, kAtlasWidth, kAtlasHeight, 0,
                 GL_RED, GL_UNSIGNED_BYTE, atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void TextRenderer::initShaders()
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, TEXT_VERTEX_SHADER);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, TEXT_FRAGMENT_SHADER);

    m_shader = glCreateProgram();
    glAttachShader(m_shader, vs);
    glAttachShader(m_shader, fs);
    glLinkProgram(m_shader);

    GLint success;
    glGetProgramiv(m_shader, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(m_shader, 512, nullptr, log);
        std::cerr << "TextRenderer shader linking failed: " << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

void TextRenderer::initBuffers()
{
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_quadVbo);
    glGenBuffers(1, &m_instanceVbo);

    // Unit quad: BL, BR, TL, TR (triangle strip)
    float quadVerts[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    };

    glBindVertexArray(m_vao);

    // Static quad
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glVertexAttribDivisor(0, 0);

    // Instance buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);

    // location 1: position
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GlyphVertex),
                          (void *)offsetof(GlyphVertex, position));
    glVertexAttribDivisor(1, 1);

    // location 2: color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GlyphVertex),
                          (void *)offsetof(GlyphVertex, color));
    glVertexAttribDivisor(2, 1);

    // location 3: size
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GlyphVertex),
                          (void *)offsetof(GlyphVertex, size));
    glVertexAttribDivisor(3, 1);

    // location 4: charCol
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(GlyphVertex),
                          (void *)offsetof(GlyphVertex, charCol));
    glVertexAttribDivisor(4, 1);

    // location 5: charRow
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(GlyphVertex),
                          (void *)offsetof(GlyphVertex, charRow));
    glVertexAttribDivisor(5, 1);

    // location 6: glyphIndex
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(GlyphVertex),
                          (void *)offsetof(GlyphVertex, glyphIndex));
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);
}

void TextRenderer::cleanup()
{
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_quadVbo) { glDeleteBuffers(1, &m_quadVbo); m_quadVbo = 0; }
    if (m_instanceVbo) { glDeleteBuffers(1, &m_instanceVbo); m_instanceVbo = 0; }
    if (m_shader) { glDeleteProgram(m_shader); m_shader = 0; }
    if (m_fontTexture) { glDeleteTextures(1, &m_fontTexture); m_fontTexture = 0; }
}

void TextRenderer::begin(const Mat3 &viewProjection, float aspectRatio)
{
    m_viewProjection = viewProjection;
    m_aspectRatio = aspectRatio;
    m_glyphs.clear();
}

void TextRenderer::addText(const char *text, Vec2 pos, Color color, float size, float anchor)
{
    if (!text) return;

    int len = 0;
    for (const char *p = text; *p; p++) len++;

    // Anchor offset in glyph units (applied in NDC by shader)
    float anchorOffset = -anchor * static_cast<float>(len);

    int i = 0;
    for (const char *p = text; *p; p++, i++)
    {
        int ch = static_cast<unsigned char>(*p);
        if (ch < 32 || ch > 127) ch = '?';
        int idx = ch - 32;

        GlyphVertex g;
        g.position = pos;  // all glyphs share the same world anchor
        g.color = color;
        g.size = size;
        g.charCol = static_cast<float>(idx % kAtlasCols);
        g.charRow = static_cast<float>(idx / kAtlasCols);
        g.glyphIndex = anchorOffset + static_cast<float>(i);

        m_glyphs.push_back(g);
    }
}

void TextRenderer::end()
{
    if (m_glyphs.empty()) return;

    glUseProgram(m_shader);

    GLint loc = glGetUniformLocation(m_shader, "u_viewProjection");
    glUniformMatrix3fv(loc, 1, GL_FALSE, m_viewProjection.m);

    GLint aspectLoc = glGetUniformLocation(m_shader, "u_aspectRatio");
    glUniform1f(aspectLoc, m_aspectRatio);

    GLint texLoc = glGetUniformLocation(m_shader, "u_fontAtlas");
    glUniform1i(texLoc, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fontTexture);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 m_glyphs.size() * sizeof(GlyphVertex),
                 m_glyphs.data(), GL_STREAM_DRAW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
                          static_cast<GLsizei>(m_glyphs.size()));

    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}
