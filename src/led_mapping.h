#pragma once
#include <cstdint>

struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct LedMappingConfig {
    double rateMin;
    double rateMax;
    int ledCount;
};

int rateToLedCount(double smoothedRate, const LedMappingConfig& config);
RGB rateToColor(double smoothedRate, const LedMappingConfig& config);
