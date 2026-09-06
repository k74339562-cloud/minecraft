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
    Glass,       // زجاج (نصف شفاف)
    OakLeaves,   // ورق شجر (مفرغ)
    Count
};

enum class BlockRenderType : uint8_t {
    Air,
    Opaque,      // صلب تماماً
    Cutout,      // ورق شجر ونباتات
    Translucent  // زجاج وماء
};

struct BlockProperties {
    BlockRenderType renderType;
    int topTexture;
    int bottomTexture;
    int sideTexture;
};

// مصفوفة ثابتة فائقة السرعة للخصائص
inline constexpr BlockProperties BLOCK_DATA[] = {
    // Air
    { BlockRenderType::Air, 0, 0, 0 },
    // Grass
    { BlockRenderType::Opaque, 0, 2, 1 },
    // Dirt
    { BlockRenderType::Opaque, 2, 2, 2 },
    // Stone
    { BlockRenderType::Opaque, 3, 3, 3 },
    // Glass
    { BlockRenderType::Translucent, 4, 4, 4 },
    // OakLeaves
    { BlockRenderType::Cutout, 5, 5, 5 }
};

// دوال مساعدة سريعة جداً (Inline)
inline bool isBlockOpaque(BlockType type) {
    return BLOCK_DATA[static_cast<size_t>(type)].renderType == BlockRenderType::Opaque;
}

inline bool isBlockAir(BlockType type) {
    return type == BlockType::Air;
}
