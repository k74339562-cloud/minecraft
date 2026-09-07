#pragma once

#include "WorldConstants.hpp"
#include "Block.hpp"
#include "Vertex.hpp"
#include "OpenGL.hpp"
#include <vector>

class World; // Forward declaration

class ChunkSection {
public:
    int m_chunkX, m_chunkY, m_chunkZ;

    ChunkSection(int chunkX, int chunkY, int chunkZ);
    ~ChunkSection();

    void setBlock(int x, int y, int z, BlockType type);
    BlockType getBlock(int x, int y, int z) const;

    // بناء مش منفصل للبلوكات الصلبة ومش منفصل للماء
    void buildMesh(const World* world = nullptr);
    void renderOpaque() const;
    void renderWater() const;

    bool isEmpty() const { return m_nonAirCount == 0; }

private:
    BlockType m_blocks[SECTION_VOLUME];
    int m_nonAirCount = 0;

    // مش البلوكات الصلبة
    GLuint m_opaqueVAO = 0, m_opaqueVBO = 0;
    GLsizei m_opaqueCount = 0;

    // مش الماء الشفاف
    GLuint m_waterVAO = 0, m_waterVBO = 0;
    GLsizei m_waterCount = 0;

    bool isFaceVisible(int x, int y, int z, Direction dir, const World* world) const;
    void addFace(std::vector<PackedVertex>& vertices, int x, int y, int z, Direction dir, int texLayer);
};
