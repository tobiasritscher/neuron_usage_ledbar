#include "led_mapping.h"
#include <algorithm>
#include <cmath>

static double normalizedFraction(double smoothedRate, const LedMappingConfig& config) {
    if (config.rateMin <= 0.0 || config.rateMax <= config.rateMin ||
        smoothedRate <= config.rateMin) {
        return 0.0;
    }
    double fraction = std::log(smoothedRate / config.rateMin) /
                      std::log(config.rateMax / config.rateMin);
    return std::min(1.0, fraction);
}

int rateToLedCount(double smoothedRate, const LedMappingConfig& config) {
    double fraction = normalizedFraction(smoothedRate, config);
    return static_cast<int>(fraction * config.ledCount + 0.5);
}

RGB ledGradientColor(int index, int ledCount) {
    double t = ledCount > 1 ? static_cast<double>(index) / (ledCount - 1) : 0.0;
    uint8_t red = static_cast<uint8_t>(std::min(255.0, 510.0 * t));
    uint8_t green = static_cast<uint8_t>(std::min(255.0, 510.0 * (1.0 - t)));
    return RGB{red, green, 0};
}
