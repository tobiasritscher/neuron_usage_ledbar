#pragma once
#include <cstdint>

struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// Log-scale mapping: rateMin lights the first LED boundary, rateMax fills
// the bar. rateMin must be > 0.
struct LedMappingConfig {
    double rateMin;
    double rateMax;
    int ledCount;
};

int rateToLedCount(double smoothedRate, const LedMappingConfig& config);

// Fixed per-position color, green (bottom) -> yellow -> red (top), so the
// bar's height is readable from color alone and red means "top LEDs lit"
// rather than the whole strip turning red.
RGB ledGradientColor(int index, int ledCount);
