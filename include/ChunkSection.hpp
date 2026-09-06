#pragma once

#include "Block.hpp"
#include "Vertex.hpp"
#include <vector>
#include <glad/glad.h>

constexpr int SECTION_SIZE = 16;
constexpr int SECTION_VOLUME = SECTION_SIZE * SECTION_SIZE * SECTION_SIZE;

class ChunkSection {
public:
    ChunkSection(int chunkX, int chunkY, int chunkZ);
    ~ChunkSection();

    // ضبط وقراءة نوع البلوكة في الإحداثيات المحلية (0 إلى 15)
    void setBlock(int x, int y, int z, BlockType type);
    BlockType getBlock(int x, int y, int z) const;

    // بناء المش مع إخفاء كل الأوجه غير المرئية (Face Culling)
    void buildMesh();

    // رسم المش في كرت الشاشة
    void render() const;

    bool isEmpty() const { return m_nonAirCount == 0; }

private:
    int m_chunkX, m_chunkY, m_chunkZ;
    BlockType m_blocks[SECTION_VOLUME];
    int m_nonAirCount = 0;

    // أدوات OpenGL للمش المضغوط
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLsizei m_vertexCount = 0;

    // فحص هل البلوكة المجاورة صلبة لحجب الوجه
    bool isFaceVisible(int x, int y, int z, Direction dir) const;

    // إضافة وجه مكعب مضغوط بالكامل
    void addFace(std::vector<PackedVertex>& vertices, 
                 int x, int y, int z, Direction dir, int texLayer);
};
