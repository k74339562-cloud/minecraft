#pragma once

constexpr int SECTION_SIZE   = 16;
constexpr int SECTION_VOLUME = SECTION_SIZE * SECTION_SIZE * SECTION_SIZE; // 4096
constexpr int WORLD_MIN_Y    = -64;
constexpr int WORLD_MAX_Y    = 320;
constexpr int WORLD_HEIGHT   = WORLD_MAX_Y - WORLD_MIN_Y; // 384 بلوكة
constexpr int NUM_SECTIONS   = WORLD_HEIGHT / SECTION_SIZE; // 24 مقطعاً
constexpr int SEA_LEVEL      = 63;
