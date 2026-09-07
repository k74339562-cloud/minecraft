#pragma once

#include <string>
#include <glm/glm.hpp>
#include "OpenGL.hpp"

class Shader {
public:
    Shader() = default;
    ~Shader();

    bool loadFromFiles(const std::string& vertPath, const std::string& fragPath);
    void use() const;

    void setMat4(const std::string& name, const glm::mat4& mat) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;

    GLuint getID() const { return m_programID; }

private:
    GLuint m_programID = 0;
    std::string readFile(const std::string& path);
    GLuint compileShader(GLenum type, const std::string& source);
};
