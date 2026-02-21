#pragma once

#include <string>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

/// First-class GLSL shader program loaded from files on disk.
///
/// Features:
///   - Load vertex + fragment stages from .vert/.frag files
///   - Compilation errors annotated with source context (surrounding lines)
///   - Hot-reload: re-read files and relink without restarting (reload())
///   - Uniform setters that silently ignore missing locations
///   - Move-only: safe to store by value, destroyed with the object
class Shader {
public:
    /// Load a vertex + fragment shader pair from files.
    /// On any error the returned Shader is invalid (valid() == false).
    static Shader fromFiles(const std::string& vertPath, const std::string& fragPath);

    bool   valid() const { return m_program != 0; }
    GLuint id()    const { return m_program; }

    void use() const;

    /// Re-read the source files and recompile/relink.
    /// Keeps the existing program intact if the new build fails.
    /// Returns true on success.
    bool reload();

    // Uniform setters – silently skipped if the name doesn't exist.
    void setMat3 (const char* name, const float* colMajor9) const;
    void setFloat(const char* name, float v)                const;
    void setInt  (const char* name, int   v)                const;
    void setVec4 (const char* name, float x, float y, float z, float w) const;

    /// Constructs an empty, invalid shader (valid() == false).
    /// Becomes valid after move-assignment from Shader::fromFiles().
    Shader() = default;

    ~Shader();
    Shader(Shader&&) noexcept;
    Shader& operator=(Shader&&) noexcept;
    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;

private:

    GLuint      m_program  = 0;
    std::string m_vertPath;
    std::string m_fragPath;

    // Compile + link from source strings. Returns 0 on failure.
    static GLuint buildProgram(const std::string& vertSrc,  const std::string& fragSrc,
                               const std::string& vertLabel, const std::string& fragLabel);

    static std::string readFile   (const std::string& path);
    static GLuint      compileStage(GLenum type, const std::string& src, const std::string& label);

    // Print annotated compilation errors: for each error line, show the
    // failing source line plus a few lines of surrounding context.
    static void reportErrors(const std::string& infoLog,
                             const std::string& src,
                             const std::string& label);
};
