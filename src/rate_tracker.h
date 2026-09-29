#pragma once
#include <cstdint>
#include <deque>

class RateTracker {
public:
    explicit RateTracker(double emaAlpha = 0.3, int64_t windowSeconds = 60);

    void addEvent(int64_t timestampEpoch, double amount);
    double updateRate(int64_t nowEpoch);
    double smoothedRate() const { return smoothedRate_; }

private:
    struct Sample {
        int64_t timestampEpoch;
        double amount;
    };

    std::deque<Sample> samples_;
    int64_t windowSeconds_;
    double emaAlpha_;
    double smoothedRate_ = 0.0;
};
