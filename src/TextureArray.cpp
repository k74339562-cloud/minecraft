#include "TextureArray.hpp"
#include <iostream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

TextureArray::~TextureArray() {
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
    }
}

bool TextureArray::loadFromDirectory(const std::filesystem::path& dirPath, int tileWidth, int tileHeight) {
    namespace fs = std::filesystem;

    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        std::cerr << "[TextureArray Error] Directory not found: " << dirPath << std::endl;
        return false;
    }

    // 1. جمع كل ملفات الـ PNG داخل المجلد
    std::vector<fs::path> imageFiles;
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            imageFiles.push_back(entry.path());
        }
    }

    if (imageFiles.empty()) {
        std::cerr << "[TextureArray Error] No PNG images found in: " << dirPath << std::endl;
        return false;
    }

    int layerCount = static_cast<int>(imageFiles.size());

    // 2. إنشاء كائن الـ Texture Array في OpenGL
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_textureID);

    // حجز مساحة الذاكرة في كرت الشاشة لكل الطبقات دفعة واحدة
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 
                 tileWidth, tileHeight, layerCount, 
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // 3. تحميل كل صورة ووضعها في طبقتها الخاصة (Layer)
    stbi_set_flip_vertically_on_load(1);

    for (int layer = 0; layer < layerCount; ++layer) {
        const auto& path = imageFiles[layer];
        std::string filename = path.stem().string(); // الاسم بدون .png

        int width, height, channels;
        unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);

        if (!data) {
            std::cerr << "[TextureArray Error] Failed to load image: " << path << std::endl;
            continue;
        }

        if (width != tileWidth || height != tileHeight) {
            std::cerr << "[TextureArray Warning] " << filename << " dimensions (" 
                      << width << "x" << height << ") do not match standard " 
                      << tileWidth << "x" << tileHeight << std::endl;
        }

        // إرسال الصورة كطبقة محددة في الـ 3D Texture
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 
                        0, 0, layer, 
                        tileWidth, tileHeight, 1, 
                        GL_RGBA, GL_UNSIGNED_BYTE, data);

        stbi_image_free(data);

        // ربط اسم الصورة برقم الطبقة
        m_layerIndices[filename] = layer;
        std::cout << "[TextureArray] Loaded: " << filename << " -> Layer " << layer << std::endl;
    }

    // 4. ضبط فلترة البكسلات (لتكون حادة ونقية 100% مثل ماينكرافت)
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // توليد الـ Mipmaps للنعومة عند النظر من مسافات بعيدة
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    return true;
}

void TextureArray::bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_textureID);
}

void TextureArray::unbind() const {
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

int TextureArray::getLayerIndex(const std::string& textureName) const {
    auto it = m_layerIndices.find(textureName);
    if (it != m_layerIndices.end()) {
        return it->second;
    }
    std::cerr << "[TextureArray Warning] Texture not found: " << textureName << std::endl;
    return 0; // إرجاع الطبقة الافتراضية
}
