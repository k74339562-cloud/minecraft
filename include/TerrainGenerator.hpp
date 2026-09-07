#pragma once
#include <cmath>
#include "WorldConstants.hpp"

inline float hash2D(int x, int z, int seed = 1337) {
    int n = x + z * 57 + seed * 131;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

inline float smoothNoise(float x, float z, int seed = 1337) {
    int ix = (int)std::floor(x), iz = (int)std::floor(z);
    float fx = x - ix, fz = z - iz;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fz = fz * fz * (3.0f - 2.0f * fz);
    float s00 = hash2D(ix, iz, seed), s10 = hash2D(ix + 1, iz, seed);
    float s01 = hash2D(ix, iz + 1, seed), s11 = hash2D(ix + 1, iz + 1, seed);
    return (s00 * (1.0f - fx) + s10 * fx) * (1.0f - fz) + (s01 * (1.0f - fx) + s11 * fx) * fz;
}

inline int getTerrainHeight(int x, int z, int seed = 1337) {
    float n1 = smoothNoise(x * 0.05f, z * 0.05f, seed) * 18.0f;
    float n2 = smoothNoise(x * 0.12f, z * 0.12f, seed + 23) * 6.0f;
    int h = 64 + (int)(n1 + n2);
    return h;
}
