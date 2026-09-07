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
    const int viewDistance = 2; // شبكة 5x5 Chunks حول اللاعب

    void generateColumnData(int cx, int cz) {
        uint64_t key = getChunkKey(cx, cz);
        if (chunks.find(key) != chunks.end()) return;

        auto col = std::make_unique<ChunkColumn>(cx, cz);

        for (int x = 0; x < SECTION_SIZE; ++x) {
            for (int z = 0; z < SECTION_SIZE; ++z) {
                int wx = cx * SECTION_SIZE + x;
                int wz = cz * SECTION_SIZE + z;
                int h = getTerrainHeight(wx, wz);

                // قاع Bedrock
                col->setBlock(x, WORLD_MIN_Y, z, BlockType::Bedrock);
                col->setBlock(x, WORLD_MIN_Y + 1, z, BlockType::Bedrock);

                for (int y = WORLD_MIN_Y + 2; y < h - 3; ++y) {
                    col->setBlock(x, y, z, BlockType::Stone);
                }

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

                // أشجار طبيعية
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
        chunks[key] = std::move(col);
    }

    void update(glm::vec3 playerPos) {
        int pcx = (int)std::floor(playerPos.x / (float)SECTION_SIZE);
        int pcz = (int)std::floor(playerPos.z / (float)SECTION_SIZE);

        // 1. توليد البيانات للبلوكات أولاً
        std::vector<ChunkColumn*> newCols;
        for (int dx = -viewDistance; dx <= viewDistance; ++dx) {
            for (int dz = -viewDistance; dz <= viewDistance; ++dz) {
                int cx = pcx + dx, cz = pcz + dz;
                uint64_t key = getChunkKey(cx, cz);
                if (chunks.find(key) == chunks.end()) {
                    generateColumnData(cx, cz);
                    newCols.push_back(chunks[key].get());
                }
            }
        }

        // 2. بناء المش بربط الحدود مع الجيران لمنع خطوط وفواصل الماء
        for (auto* col : newCols) {
            col->buildAllMeshes(this);
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
