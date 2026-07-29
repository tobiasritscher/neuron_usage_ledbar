#include "rate_tracker.h"

RateTracker::RateTracker(double emaAlpha, int64_t windowSeconds)
    : windowSeconds_(windowSeconds), emaAlpha_(emaAlpha) {}

void RateTracker::addEvent(int64_t timestampEpoch, uint32_t tokens) {
    samples_.push_back({timestampEpoch, tokens});
}

double RateTracker::updateRate(int64_t nowEpoch) {
    while (!samples_.empty() &&
           samples_.front().timestampEpoch < nowEpoch - windowSeconds_) {
        samples_.pop_front();
    }

    uint64_t windowTotal = 0;
    for (const auto& sample : samples_) {
        windowTotal += sample.tokens;
    }

    double rawRatePerMinute =
        static_cast<double>(windowTotal) * (60.0 / static_cast<double>(windowSeconds_));

    smoothedRate_ = smoothedRate_ + emaAlpha_ * (rawRatePerMinute - smoothedRate_);
    return smoothedRate_;
}
