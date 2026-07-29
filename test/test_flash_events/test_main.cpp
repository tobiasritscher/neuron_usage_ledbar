// test/test_flash_events/test_main.cpp
#include <unity.h>
#include "flash_events.h"

void test_parse_two_events() {
    std::string json = R"({
        "data": [
            {"request_id": "req-2", "startTime": "2026-07-29T16:30:11", "prompt_tokens": 125},
            {"request_id": "req-1", "startTime": "2026-07-29T16:30:06", "prompt_tokens": 102}
        ]
    })";

    auto events = parseFlashEvents(json);

    TEST_ASSERT_EQUAL(2, events.size());
    TEST_ASSERT_EQUAL_STRING("req-2", events[0].requestId.c_str());
    TEST_ASSERT_EQUAL_UINT32(125, events[0].promptTokens);
    TEST_ASSERT_TRUE(events[0].timestampEpoch > events[1].timestampEpoch);
}

void test_filter_newer_than_excludes_seen() {
    std::vector<FlashEvent> events = {
        {"req-2", 1000, 50},
        {"req-1", 900, 30},
    };

    auto fresh = filterNewerThan(events, "req-1", 900);

    TEST_ASSERT_EQUAL(1, fresh.size());
    TEST_ASSERT_EQUAL_STRING("req-2", fresh[0].requestId.c_str());
}

void test_filter_newer_than_empty_when_nothing_new() {
    std::vector<FlashEvent> events = {
        {"req-1", 900, 30},
    };

    auto fresh = filterNewerThan(events, "req-1", 900);

    TEST_ASSERT_EQUAL(0, fresh.size());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_two_events);
    RUN_TEST(test_filter_newer_than_excludes_seen);
    RUN_TEST(test_filter_newer_than_empty_when_nothing_new);
    return UNITY_END();
}
