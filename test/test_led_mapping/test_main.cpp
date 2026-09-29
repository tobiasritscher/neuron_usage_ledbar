#include <unity.h>
#include "led_mapping.h"

// Log scale: 0.01 .. 10 spans 3 decades over 18 LEDs -> 6 LEDs per decade.
static const LedMappingConfig kConfig{0.01, 10.0, 18};

void test_led_count_below_min_is_zero() {
    TEST_ASSERT_EQUAL(0, rateToLedCount(0.0, kConfig));
    TEST_ASSERT_EQUAL(0, rateToLedCount(0.005, kConfig));
}

void test_led_count_at_max_is_full() {
    TEST_ASSERT_EQUAL(18, rateToLedCount(10.0, kConfig));
}

void test_led_count_clamped_above_max() {
    TEST_ASSERT_EQUAL(18, rateToLedCount(500.0, kConfig));
}

void test_led_count_is_logarithmic() {
    TEST_ASSERT_EQUAL(6, rateToLedCount(0.1, kConfig));
    TEST_ASSERT_EQUAL(12, rateToLedCount(1.0, kConfig));
}

void test_led_count_ignores_non_positive_min() {
    LedMappingConfig broken{0.0, 10.0, 18};
    TEST_ASSERT_EQUAL(0, rateToLedCount(5.0, broken));
}

void test_bottom_led_is_green() {
    RGB color = ledGradientColor(0, 18);
    TEST_ASSERT_EQUAL_UINT8(0, color.r);
    TEST_ASSERT_EQUAL_UINT8(255, color.g);
    TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

void test_top_led_is_red() {
    RGB color = ledGradientColor(17, 18);
    TEST_ASSERT_EQUAL_UINT8(255, color.r);
    TEST_ASSERT_EQUAL_UINT8(0, color.g);
    TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

void test_middle_led_is_yellowish() {
    RGB color = ledGradientColor(9, 18);
    TEST_ASSERT_EQUAL_UINT8(255, color.r);
    TEST_ASSERT_TRUE(color.g > 200);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_led_count_below_min_is_zero);
    RUN_TEST(test_led_count_at_max_is_full);
    RUN_TEST(test_led_count_clamped_above_max);
    RUN_TEST(test_led_count_is_logarithmic);
    RUN_TEST(test_led_count_ignores_non_positive_min);
    RUN_TEST(test_bottom_led_is_green);
    RUN_TEST(test_top_led_is_red);
    RUN_TEST(test_middle_led_is_yellowish);
    return UNITY_END();
}
