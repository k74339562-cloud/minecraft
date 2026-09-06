#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>
#include <glad/glad.h>

class TextureArray {
public:
    TextureArray() = default;
    ~TextureArray();

    // يمسح المجلد ويحمل كل صور الـ 16x16 كطبقات
    bool loadFromDirectory(const std::filesystem::path& dirPath, int tileWidth = 16, int tileHeight = 16);
    
    void bind(unsigned int slot = 0) const;
    void unbind() const;

    // الحصول على رقم الطبقة (Layer ID) باستخدام اسم الصورة
    int getLayerIndex(const std::string& textureName) const;

    GLuint getID() const { return m_textureID; }

private:
    GLuint m_textureID = 0;
    std::unordered_map<std::string, int> m_layerIndices;
};
