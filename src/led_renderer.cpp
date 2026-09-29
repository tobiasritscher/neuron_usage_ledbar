#ifndef UNIT_TEST
#include "led_renderer.h"
#include <algorithm>
#include <cmath>

LedRenderer::LedRenderer(Adafruit_NeoPixel& strip, int ledCount)
    : strip_(strip), ledCount_(ledCount) {}

void LedRenderer::setBaseline(int ledCount) {
    baselineLedCount_ = ledCount;
}

void LedRenderer::addFlash(uint32_t promptTokens, unsigned long nowMillis) {
    activeFlashes_.push_back({nowMillis, promptTokens});
}

void LedRenderer::renderFrame(unsigned long nowMillis) {
    activeFlashes_.erase(
        std::remove_if(activeFlashes_.begin(), activeFlashes_.end(),
                        [&](const ActiveFlash& flash) {
                            return nowMillis - flash.startMillis > kFlashDurationMs;
                        }),
        activeFlashes_.end());

    double flashBoost = 0.0;
    for (const auto& flash : activeFlashes_) {
        double age = static_cast<double>(nowMillis - flash.startMillis);
        double decay = std::exp(-age / (kFlashDurationMs / 4.0));
        double magnitude = std::min(1.0, flash.promptTokens / 500.0);
        flashBoost = std::max(flashBoost, magnitude * decay);
    }

    for (int i = 0; i < ledCount_; i++) {
        RGB base = i < baselineLedCount_ ? ledGradientColor(i, ledCount_) : RGB{0, 0, 0};
        // Request flashes only light the bottom LEDs, blending toward orange
        // (not adding white), so a busy bar doesn't read as the whole strip
        // blinking white and stays distinct from the offline pulse.
        double boost = i < kFlashLedCount ? flashBoost : 0.0;
        uint8_t r = static_cast<uint8_t>(base.r + boost * (kFlashColor.r - base.r));
        uint8_t g = static_cast<uint8_t>(base.g + boost * (kFlashColor.g - base.g));
        uint8_t b = static_cast<uint8_t>(base.b + boost * (kFlashColor.b - base.b));
        strip_.setPixelColor(i, strip_.Color(r, g, b));
    }
    strip_.show();
}

void LedRenderer::renderOfflinePulse(unsigned long nowMillis) {
    double phase = (std::sin(nowMillis / 500.0) + 1.0) / 2.0;
    uint8_t brightness = static_cast<uint8_t>(30 + phase * 40);
    for (int i = 0; i < ledCount_; i++) {
        strip_.setPixelColor(i, strip_.Color(brightness, brightness, brightness));
    }
    strip_.show();
}
#endif // UNIT_TEST
