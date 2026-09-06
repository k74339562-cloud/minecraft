#pragma once
#include <cstdint>

// فيرتكس مضغوط في 4 بايت (32 بت) فقط!
// [0..4]   : X (0-31)       -> 5 بت
// [5..9]   : Y (0-31)       -> 5 بت
// [10..14] : Z (0-31)       -> 5 بت
// [15..17] : Face Dir (0-5) -> 3 بت (أعلى، أسفل، يمين، يسار، أمام، خلف)
// [18..19] : AO (0-3)       -> 2 بت (درجات الظلال الناعمة)
// [20..29] : Texture Layer  -> 10 بت (تتسع حتى 1024 صورة مختلفة!)
// [30..31] : محجوز للمستقبل -> 2 بت
struct PackedVertex {
    uint32_t data;

    static PackedVertex create(uint32_t x, uint32_t y, uint32_t z, 
                               uint32_t faceDir, uint32_t ao, uint32_t texLayer) {
        PackedVertex v;
        v.data = (x & 0x1F) |
                 ((y & 0x1F) << 5) |
                 ((z & 0x1F) << 10) |
                 ((faceDir & 0x7) << 15) |
                 ((ao & 0x3) << 18) |
                 ((texLayer & 0x3FF) << 20);
        return v;
    }
};
