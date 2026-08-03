#pragma once
#ifndef UNIT_TEST
#include <Adafruit_NeoPixel.h>
#include <vector>
#include "led_mapping.h"

struct ActiveFlash {
    unsigned long startMillis;
    uint32_t promptTokens;
};

class LedRenderer {
public:
    LedRenderer(Adafruit_NeoPixel& strip, int ledCount);

    void setBaseline(int ledCount, RGB color);
    void addFlash(uint32_t promptTokens, unsigned long nowMillis);
    void renderFrame(unsigned long nowMillis);
    void renderOfflinePulse(unsigned long nowMillis);

private:
    Adafruit_NeoPixel& strip_;
    int ledCount_;
    int baselineLedCount_ = 0;
    RGB baselineColor_ = {0, 0, 255};
    std::vector<ActiveFlash> activeFlashes_;

    static constexpr unsigned long kFlashDurationMs = 600;
};
#endif // UNIT_TEST
