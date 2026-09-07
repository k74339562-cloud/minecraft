#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>
#include "OpenGL.hpp"

class TextureArray {
public:
    TextureArray() = default;
    ~TextureArray();

    bool loadFromDirectory(const std::filesystem::path& dirPath, int tileWidth = 16, int tileHeight = 16);
    
    void bind(unsigned int slot = 0) const;
    void unbind() const;

    int getLayerIndex(const std::string& textureName) const;
    GLuint getID() const { return m_textureID; }

private:
    GLuint m_textureID = 0;
    std::unordered_map<std::string, int> m_layerIndices;
};
