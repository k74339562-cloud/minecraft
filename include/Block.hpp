#pragma once
#include <cstdint>
#include <string>

// الاتجاهات الستة للأوجه
enum Direction : uint8_t {
    DIR_UP = 0,    // +Y (الوجه العلوي)
    DIR_DOWN = 1,  // -Y (الوجه السفلي)
    DIR_NORTH = 2, // -Z
    DIR_SOUTH = 3, // +Z
    DIR_WEST = 4,  // -X
    DIR_EAST = 5   // +X
};

// أنواع البلوكات
enum class BlockType : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Count
};

struct BlockData {
    bool isOpaque;       // هل تحجب الرؤية؟ (إذا نعم: تحذف الأوجه خلفها)
    int topTexture;      // رقم صورة السطح
    int bottomTexture;   // رقم صورة القاع
    int sideTexture;     // رقم صورة الجوانب
};
