#pragma once
#include <memory>
#include <vector>
#include "WorldConstants.hpp"
#include "ChunkSection.hpp"

class ChunkColumn {
public:
    int chunkX = 0, chunkZ = 0;
    std::unique_ptr<ChunkSection> sections[NUM_SECTIONS];

    ChunkColumn(int cx, int cz) : chunkX(cx), chunkZ(cz) {}

    // تحويل الارتفاع العالمي Y (-64 إلى 319) إلى رقم المقطع (0 إلى 23)
    int getSectionIndex(int globalY) const {
        if (globalY < WORLD_MIN_Y || globalY >= WORLD_MAX_Y) return -1;
        return (globalY - WORLD_MIN_Y) / SECTION_SIZE;
    }

    void setBlock(int x, int globalY, int z, BlockType type) {
        int idx = getSectionIndex(globalY);
        if (idx < 0 || idx >= NUM_SECTIONS) return;

        // حجز الذاكرة للمقطع فقط إذا كان يحوي بلوكات حقيقية! (توفير 80% من الرام)
        if (!sections[idx]) {
            int sectionChunkY = (idx * SECTION_SIZE) + WORLD_MIN_Y;
            sections[idx] = std::make_unique<ChunkSection>(chunkX, sectionChunkY, chunkZ);
        }

        int localY = (globalY - WORLD_MIN_Y) % SECTION_SIZE;
        sections[idx]->setBlock(x, localY, z, type);
    }

    BlockType getBlock(int x, int globalY, int z) const {
        int idx = getSectionIndex(globalY);
        if (idx < 0 || idx >= NUM_SECTIONS || !sections[idx]) return BlockType::Air;
        int localY = (globalY - WORLD_MIN_Y) % SECTION_SIZE;
        return sections[idx]->getBlock(x, localY, z);
    }

    void buildAllMeshes() {
        for (int i = 0; i < NUM_SECTIONS; ++i) {
            if (sections[i] && !sections[i]->isEmpty()) {
                sections[i]->buildMesh();
            }
        }
    }
};
