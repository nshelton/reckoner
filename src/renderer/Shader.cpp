#include "renderer/Shader.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cctype>

// ---------------------------------------------------------------------------
// ANSI terminal colours (fall back gracefully on terminals that ignore them)
// ---------------------------------------------------------------------------
static constexpr const char* RED    = "\033[31m";
static constexpr const char* YELLOW = "\033[33m";
static constexpr const char* CYAN   = "\033[36m";
static constexpr const char* BOLD   = "\033[1m";
static constexpr const char* RESET  = "\033[0m";

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// Extract the first GLSL source line number from an info-log line.
/// Handles the common driver formats:
///   Mesa/Intel : "0:LINE(col): error: ..."
///   NVIDIA     : "0(LINE) : error C..."
///   AMD        : "ERROR: 0:LINE: ..."
/// Returns -1 if no line number can be found.
static int extractLineNumber(const std::string& msg) {
    for (size_t i = 0; i + 1 < msg.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(msg[i])) &&
            (msg[i + 1] == ':' || msg[i + 1] == '('))
        {
            // Digits after the separator
            size_t j = i + 2;
            if (j < msg.size() && std::isdigit(static_cast<unsigned char>(msg[j]))) {
                size_t k = j;
                while (k < msg.size() && std::isdigit(static_cast<unsigned char>(msg[k]))) ++k;
                char term = k < msg.size() ? msg[k] : '\0';
                if (term == ':' || term == '(' || term == ')' || term == ' ' || term == '\0') {
                    try { return std::stoi(msg.substr(j, k - j)); } catch (...) {}
                }
            }
        }
    }
    return -1;
}

static std::string basename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

// ---------------------------------------------------------------------------
// Shader implementation
// ---------------------------------------------------------------------------

std::string Shader::readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << BOLD << RED
                  << "Shader: cannot open '" << path << "'\n"
                  << RESET;
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void Shader::reportErrors(const std::string& log,
                          const std::string& src,
                          const std::string& label)
{
    // Index source into 1-based lines
    std::vector<std::string> lines;
    {
        std::istringstream ss(src);
        std::string line;
        while (std::getline(ss, line)) lines.push_back(line);
    }

    std::cerr << BOLD << RED << "\n--- Shader error [" << label << "] ---\n" << RESET;

    std::istringstream logStream(log);
    std::string errLine;
    while (std::getline(logStream, errLine)) {
        if (errLine.empty()) continue;

        int lineNum = extractLineNumber(errLine);
        std::cerr << RED << errLine << RESET << "\n";

        if (lineNum > 0 && lineNum <= static_cast<int>(lines.size())) {
            int ctxStart = std::max(1, lineNum - 2);
            int ctxEnd   = std::min(static_cast<int>(lines.size()), lineNum + 2);

            for (int i = ctxStart; i <= ctxEnd; ++i) {
                if (i == lineNum)
                    std::cerr << BOLD << YELLOW
                              << ">" << std::setw(4) << i << " | " << lines[i - 1]
                              << RESET << "\n";
                else
                    std::cerr << " " << std::setw(4) << i << " | " << lines[i - 1] << "\n";
            }
            std::cerr << "\n";
        }
    }

    std::cerr << BOLD << RED << "---\n" << RESET;
}

GLuint Shader::compileStage(GLenum type, const std::string& src, const std::string& label) {
    if (src.empty()) return 0;

    const char* cstr  = src.c_str();
    GLuint      shader = glCreateShader(type);
    glShaderSource(shader, 1, &cstr, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len), '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        reportErrors(log, src, label);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint Shader::buildProgram(const std::string& vertSrc,  const std::string& fragSrc,
                            const std::string& vertLabel, const std::string& fragLabel)
{
    GLuint vs = compileStage(GL_VERTEX_SHADER,   vertSrc, vertLabel);
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragSrc, fragLabel);

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    // Shaders are copied into the program at link time; delete the stage objects.
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len), '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << BOLD << RED
                  << "\n--- Shader link error [" << vertLabel << " + " << fragLabel << "] ---\n"
                  << log
                  << "---\n" << RESET;
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

Shader Shader::fromFiles(const std::string& vertPath, const std::string& fragPath) {
    Shader s;
    s.m_vertPath = vertPath;
    s.m_fragPath = fragPath;

    const std::string vertSrc = readFile(vertPath);
    const std::string fragSrc = readFile(fragPath);

    s.m_program = buildProgram(vertSrc, fragSrc, basename(vertPath), basename(fragPath));

    if (s.m_program)
        std::cerr << CYAN << "Shader loaded: "
                  << basename(vertPath) << " + " << basename(fragPath)
                  << RESET << "\n";

    return s;
}

bool Shader::reload() {
    const std::string vertSrc = readFile(m_vertPath);
    const std::string fragSrc = readFile(m_fragPath);

    GLuint newProg = buildProgram(vertSrc, fragSrc,
                                  basename(m_vertPath), basename(m_fragPath));
    if (!newProg) return false;

    if (m_program) glDeleteProgram(m_program);
    m_program = newProg;
    std::cerr << CYAN << "Shader reloaded: "
              << basename(m_vertPath) << " + " << basename(m_fragPath)
              << RESET << "\n";
    return true;
}

void Shader::use() const {
    glUseProgram(m_program);
}

void Shader::setMat3(const char* name, const float* v) const {
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) glUniformMatrix3fv(loc, 1, GL_FALSE, v);
}

void Shader::setFloat(const char* name, float v) const {
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) glUniform1f(loc, v);
}

void Shader::setInt(const char* name, int v) const {
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) glUniform1i(loc, v);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

Shader::~Shader() {
    if (m_program) glDeleteProgram(m_program);
}

Shader::Shader(Shader&& o) noexcept
    : m_program(o.m_program),
      m_vertPath(std::move(o.m_vertPath)),
      m_fragPath(std::move(o.m_fragPath))
{
    o.m_program = 0;
}

Shader& Shader::operator=(Shader&& o) noexcept {
    if (this != &o) {
        if (m_program) glDeleteProgram(m_program);
        m_program  = o.m_program;
        m_vertPath = std::move(o.m_vertPath);
        m_fragPath = std::move(o.m_fragPath);
        o.m_program = 0;
    }
    return *this;
}