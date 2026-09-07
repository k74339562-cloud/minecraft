#pragma once
#include <cstdint>

enum Direction : uint8_t {
    DIR_UP = 0, DIR_DOWN = 1, DIR_NORTH = 2, DIR_SOUTH = 3, DIR_WEST = 4, DIR_EAST = 5
};

enum class BlockType : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    OakLeaves,
    OakLog,      // خشب الشجرة الحقيقي
    Glass,
    Count
};

inline bool isBlockOpaque(BlockType type) {
    return type == BlockType::Grass || 
           type == BlockType::Dirt  || 
           type == BlockType::Stone || 
           type == BlockType::OakLog; // الخشب صلب ومعتم
}

inline bool isBlockAir(BlockType type) {
    return type == BlockType::Air;
}
