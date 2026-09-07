#pragma once
#include <unordered_map>
#include <memory>
#include <vector>
#include "ChunkColumn.hpp"
#include "TerrainGenerator.hpp"

inline uint64_t getChunkKey(int cx, int cz) {
    return ((uint64_t)(uint32_t)cx << 32) | (uint32_t)cz;
}

class World {
public:
    std::unordered_map<uint64_t, std::unique_ptr<ChunkColumn>> chunks;
    const int viewDistance = 2; // يولد شبكة 5x5 Chunks عملاقة حول اللاعب!

    void generateColumn(int cx, int cz) {
        uint64_t key = getChunkKey(cx, cz);
        if (chunks.find(key) != chunks.end()) return;

        auto col = std::make_unique<ChunkColumn>(cx, cz);

        for (int x = 0; x < SECTION_SIZE; ++x) {
            for (int z = 0; z < SECTION_SIZE; ++z) {
                int wx = cx * SECTION_SIZE + x;
                int wz = cz * SECTION_SIZE + z;
                int h = getTerrainHeight(wx, wz);

                // 1. حجر الأساس Bedrock عند القاع -64
                col->setBlock(x, WORLD_MIN_Y, z, BlockType::Bedrock);
                col->setBlock(x, WORLD_MIN_Y + 1, z, BlockType::Bedrock);

                // 2. طبقات الصخور العميقة حتى ما قبل السطح
                for (int y = WORLD_MIN_Y + 2; y < h - 3; ++y) {
                    col->setBlock(x, y, z, BlockType::Stone);
                }

                // 3. الشواطئ والبحار أو التلال الخضراء
                if (h <= SEA_LEVEL + 1) {
                    for (int y = std::max(WORLD_MIN_Y + 2, h - 3); y <= h; ++y) {
                        col->setBlock(x, y, z, BlockType::Sand);
                    }
                    for (int y = h + 1; y <= SEA_LEVEL; ++y) {
                        col->setBlock(x, y, z, BlockType::Water);
                    }
                } else {
                    for (int y = h - 3; y < h; ++y) {
                        col->setBlock(x, y, z, BlockType::Dirt);
                    }
                    col->setBlock(x, h, z, BlockType::Grass);
                }

                // 4. توليد أشجار طبيعية متناثرة فوق التلال
                if (h > SEA_LEVEL + 2 && (hash2D(wx, wz) > 0.88f)) {
                    int ty = h + 1;
                    for (int y = 0; y < 5; ++y) col->setBlock(x, ty + y, z, BlockType::OakLog);
                    for (int ox = -2; ox <= 2; ++ox) {
                        for (int oz = -2; oz <= 2; ++oz) {
                            if (std::abs(ox) == 2 && std::abs(oz) == 2) continue;
                            if (ox != 0 || oz != 0) {
                                col->setBlock(x + ox, ty + 2, z + oz, BlockType::OakLeaves);
                                col->setBlock(x + ox, ty + 3, z + oz, BlockType::OakLeaves);
                            }
                        }
                    }
                    col->setBlock(x, ty + 5, z, BlockType::OakLeaves);
                }
            }
        }

        col->buildAllMeshes();
        chunks[key] = std::move(col);
    }

    void update(glm::vec3 playerPos) {
        int pcx = (int)std::floor(playerPos.x / (float)SECTION_SIZE);
        int pcz = (int)std::floor(playerPos.z / (float)SECTION_SIZE);

        for (int dx = -viewDistance; dx <= viewDistance; ++dx) {
            for (int dz = -viewDistance; dz <= viewDistance; ++dz) {
                generateColumn(pcx + dx, pcz + dz);
            }
        }
    }

    BlockType getBlock(int wx, int wy, int wz) const {
        if (wy < WORLD_MIN_Y || wy >= WORLD_MAX_Y) return BlockType::Air;
        int cx = (int)std::floor(wx / (float)SECTION_SIZE);
        int cz = (int)std::floor(wz / (float)SECTION_SIZE);
        uint64_t key = getChunkKey(cx, cz);
        auto it = chunks.find(key);
        if (it == chunks.end()) return BlockType::Air;

        int lx = (wx % SECTION_SIZE + SECTION_SIZE) % SECTION_SIZE;
        int lz = (wz % SECTION_SIZE + SECTION_SIZE) % SECTION_SIZE;
        return it->second->getBlock(lx, wy, lz);
    }
};
