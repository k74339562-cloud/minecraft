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
    Bedrock,     // حجر الأساس عند -64
    Sand,        // رمال الشواطئ
    Water,       // ماء البحار والمحيطات
    OakLog,      // خشب الشجرة
    OakLeaves,   // أوراق الشجر
    Count
};

inline bool isBlockOpaque(BlockType type) {
    return type == BlockType::Grass || 
           type == BlockType::Dirt  || 
           type == BlockType::Stone || 
           type == BlockType::Bedrock || 
           type == BlockType::Sand  || 
           type == BlockType::OakLog;
}

inline bool isBlockWater(BlockType type) {
    return type == BlockType::Water;
}

inline bool isBlockAir(BlockType type) {
    return type == BlockType::Air;
}
