#pragma once
#include <cmath>

inline float hash2D(int x, int z) {
    int n = x + z * 57; n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

inline float smoothNoise(float x, float z) {
    int ix = (int)std::floor(x), iz = (int)std::floor(z);
    float fx = x - ix, fz = z - iz;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fz = fz * fz * (3.0f - 2.0f * fz);
    float s00 = hash2D(ix, iz), s10 = hash2D(ix + 1, iz);
    float s01 = hash2D(ix, iz + 1), s11 = hash2D(ix + 1, iz + 1);
    return (s00 * (1.0f - fx) + s10 * fx) * (1.0f - fz) + (s01 * (1.0f - fx) + s11 * fx) * fz;
}

inline int getTerrainHeight(int x, int z) {
    float n = smoothNoise(x * 0.12f, z * 0.12f) * 3.5f + smoothNoise(x * 0.25f, z * 0.25f) * 1.5f;
    int h = 4 + (int)n;
    return (h < 2) ? 2 : ((h > 9) ? 9 : h);
}
