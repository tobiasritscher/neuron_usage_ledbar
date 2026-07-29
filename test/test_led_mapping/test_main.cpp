#include <unity.h>
#include "led_mapping.h"

void test_led_count_at_min_is_zero() {
    LedMappingConfig config{0.0, 1000.0, 18};
    TEST_ASSERT_EQUAL(0, rateToLedCount(0.0, config));
}

void test_led_count_at_max_is_full() {
    LedMappingConfig config{0.0, 1000.0, 18};
    TEST_ASSERT_EQUAL(18, rateToLedCount(1000.0, config));
}

void test_led_count_clamped_above_max() {
    LedMappingConfig config{0.0, 1000.0, 18};
    TEST_ASSERT_EQUAL(18, rateToLedCount(5000.0, config));
}

void test_color_is_blue_at_min() {
    LedMappingConfig config{0.0, 1000.0, 18};
    RGB color = rateToColor(0.0, config);
    TEST_ASSERT_EQUAL_UINT8(0, color.r);
    TEST_ASSERT_EQUAL_UINT8(255, color.b);
}

void test_color_is_red_at_max() {
    LedMappingConfig config{0.0, 1000.0, 18};
    RGB color = rateToColor(1000.0, config);
    TEST_ASSERT_EQUAL_UINT8(255, color.r);
    TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_led_count_at_min_is_zero);
    RUN_TEST(test_led_count_at_max_is_full);
    RUN_TEST(test_led_count_clamped_above_max);
    RUN_TEST(test_color_is_blue_at_min);
    RUN_TEST(test_color_is_red_at_max);
    return UNITY_END();
}
