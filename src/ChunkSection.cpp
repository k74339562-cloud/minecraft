#pragma once

#include "WorldConstants.hpp"
#include "Block.hpp"
#include "Vertex.hpp"
#include "OpenGL.hpp"
#include <vector>

constexpr int SECTION_VOLUME = SECTION_SIZE * SECTION_SIZE * SECTION_SIZE;

class ChunkSection {
public:
    ChunkSection(int chunkX, int chunkY, int chunkZ);
    ~ChunkSection();

    void setBlock(int x, int y, int z, BlockType type);
    BlockType getBlock(int x, int y, int z) const;

    void buildMesh();
    void render() const;

    bool isEmpty() const { return m_nonAirCount == 0; }

private:
    int m_chunkX, m_chunkY, m_chunkZ;
    BlockType m_blocks[SECTION_VOLUME];
    int m_nonAirCount = 0;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLsizei m_vertexCount = 0;

    bool isFaceVisible(int x, int y, int z, Direction dir) const;
    void addFace(std::vector<PackedVertex>& vertices, 
                 int x, int y, int z, Direction dir, int texLayer);
};
