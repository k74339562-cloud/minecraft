#pragma once

constexpr int SECTION_SIZE = 16;
constexpr int WORLD_MIN_Y  = -64;  // قاع العالم
constexpr int WORLD_MAX_Y  = 320;  // سقف البناء
constexpr int WORLD_HEIGHT = WORLD_MAX_Y - WORLD_MIN_Y; // 384 بلوكة!
constexpr int NUM_SECTIONS = WORLD_HEIGHT / SECTION_SIZE; // 24 مقطعاً رأسياً
constexpr int SEA_LEVEL    = 63;   // سطح البحر الرسمي لماينكرافت
