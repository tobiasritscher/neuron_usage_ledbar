#include <unity.h>
#include "rate_tracker.h"

void test_rate_zero_with_no_events() {
    RateTracker tracker(0.3, 60);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, tracker.updateRate(1000));
}

void test_rate_reflects_window_sum() {
    RateTracker tracker(1.0, 60); // alpha=1.0 disables smoothing for this test
    tracker.addEvent(950, 100);
    tracker.addEvent(970, 200);

    double rate = tracker.updateRate(1000);

    TEST_ASSERT_EQUAL_DOUBLE(300.0, rate); // 300 tokens inside a 60s window = 300/min
}

void test_rate_drops_events_outside_window() {
    RateTracker tracker(1.0, 60);
    tracker.addEvent(800, 500); // 200s before "now" — outside the 60s window
    tracker.addEvent(990, 100);

    double rate = tracker.updateRate(1000);

    TEST_ASSERT_EQUAL_DOUBLE(100.0, rate);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_rate_zero_with_no_events);
    RUN_TEST(test_rate_reflects_window_sum);
    RUN_TEST(test_rate_drops_events_outside_window);
    return UNITY_END();
}
