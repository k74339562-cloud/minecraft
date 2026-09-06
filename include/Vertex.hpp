#pragma once
#include <cstdint>

// [0..4]   : X (5 bits)
// [5..9]   : Y (5 bits)
// [10..14] : Z (5 bits)
// [15..17] : Face Dir (3 bits)
// [18..19] : AO (2 bits)
// [20..29] : Texture Layer (10 bits)
// [30..31] : UV Corner Index (2 bits: 0=(0,0), 1=(1,0), 2=(1,1), 3=(0,1))
struct PackedVertex {
    uint32_t data;

    static PackedVertex create(uint32_t x, uint32_t y, uint32_t z, 
                               uint32_t faceDir, uint32_t ao, uint32_t texLayer, uint32_t uvIndex) {
        PackedVertex v;
        v.data = (x & 0x1F) |
                 ((y & 0x1F) << 5) |
                 ((z & 0x1F) << 10) |
                 ((faceDir & 0x7) << 15) |
                 ((ao & 0x3) << 18) |
                 ((texLayer & 0x3FF) << 20) |
                 ((uvIndex & 0x3) << 30);
        return v;
    }
};
