#include "Shader.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

Shader::~Shader() {
    if (m_programID != 0) {
        glDeleteProgram(m_programID);
    }
}

std::string Shader::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Shader Error] Failed to open file: " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint Shader::compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    
    // ضبط إصدار الـ GLSL تلقائياً: OpenGL Core للكمبيوتر و ES 3.0 للأندرويد
#ifdef __ANDROID__
    std::string header = "#version 300 es\n";
#else
    std::string header = "#version 330 core\n";
#endif
    std::string fullSource = header + source;
    const char* srcPtr = fullSource.c_str();

    glShaderSource(shader, 1, &srcPtr, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "[Shader Compile Error] " << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT")
                  << "\n" << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::loadFromFiles(const std::string& vertPath, const std::string& fragPath) {
    std::string vertCode = readFile(vertPath);
    std::string fragCode = readFile(fragPath);

    if (vertCode.empty() || fragCode.empty()) return false;

    GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertCode);
    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragCode);

    if (vertShader == 0 || fragShader == 0) return false;

    m_programID = glCreateProgram();
    glAttachShader(m_programID, vertShader);
    glAttachShader(m_programID, fragShader);
    glLinkProgram(m_programID);

    GLint success;
    glGetProgramiv(m_programID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_programID, 512, nullptr, infoLog);
        std::cerr << "[Shader Link Error]\n" << infoLog << std::endl;
        return false;
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    return true;
}

void Shader::use() const {
    glUseProgram(m_programID);
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const {
    glUniformMatrix4fv(glGetUniformLocation(m_programID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setInt(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(m_programID, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) const {
    glUniform1f(glGetUniformLocation(m_programID, name.c_str()), value);
}
