#include "led_mapping.h"
#include <algorithm>

static double normalizedFraction(double smoothedRate, const LedMappingConfig& config) {
    if (config.rateMax <= config.rateMin) {
        return 0.0;
    }
    double fraction = (smoothedRate - config.rateMin) / (config.rateMax - config.rateMin);
    return std::min(1.0, std::max(0.0, fraction));
}

int rateToLedCount(double smoothedRate, const LedMappingConfig& config) {
    double fraction = normalizedFraction(smoothedRate, config);
    return static_cast<int>(fraction * config.ledCount + 0.5);
}

RGB rateToColor(double smoothedRate, const LedMappingConfig& config) {
    double fraction = normalizedFraction(smoothedRate, config);
    uint8_t red = static_cast<uint8_t>(255 * fraction);
    uint8_t blue = static_cast<uint8_t>(255 * (1.0 - fraction));
    return RGB{red, 0, blue};
}
