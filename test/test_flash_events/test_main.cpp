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

    auto result = parseFlashEvents(json);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_EQUAL(2, result.events.size());
    TEST_ASSERT_EQUAL_STRING("req-2", result.events[0].requestId.c_str());
    TEST_ASSERT_EQUAL_UINT32(125, result.events[0].promptTokens);
    TEST_ASSERT_TRUE(result.events[0].timestampEpoch > result.events[1].timestampEpoch);
}

void test_parse_malformed_json_reports_failure() {
    std::string json = "not json";

    auto result = parseFlashEvents(json);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL(0, result.events.size());
}

void test_filter_newer_than_excludes_seen() {
    std::vector<FlashEvent> events = {
        {"req-2", 1000, 50},
        {"req-1", 900, 30},
    };
    std::set<std::string> seen = {"req-1"};

    auto fresh = filterNewerThan(events, seen);

    TEST_ASSERT_EQUAL(1, fresh.size());
    TEST_ASSERT_EQUAL_STRING("req-2", fresh[0].requestId.c_str());
}

void test_filter_newer_than_empty_when_nothing_new() {
    std::vector<FlashEvent> events = {
        {"req-1", 900, 30},
    };
    std::set<std::string> seen = {"req-1"};

    auto fresh = filterNewerThan(events, seen);

    TEST_ASSERT_EQUAL(0, fresh.size());
}

void test_filter_newer_than_handles_same_second_events() {
    // Three events sharing the same second-precision timestamp (the bug
    // scenario from the final review): two are already seen, one is new.
    // A high-water-mark (requestId, epoch) dedup would re-emit the seen
    // same-second siblings forever; request-id-set dedup must not.
    std::vector<FlashEvent> events = {
        {"req-a", 1000, 10},
        {"req-b", 1000, 20},
        {"req-c", 1000, 30},
    };
    std::set<std::string> seen = {"req-a", "req-b"};

    auto fresh = filterNewerThan(events, seen);

    TEST_ASSERT_EQUAL(1, fresh.size());
    TEST_ASSERT_EQUAL_STRING("req-c", fresh[0].requestId.c_str());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_two_events);
    RUN_TEST(test_parse_malformed_json_reports_failure);
    RUN_TEST(test_filter_newer_than_excludes_seen);
    RUN_TEST(test_filter_newer_than_empty_when_nothing_new);
    RUN_TEST(test_filter_newer_than_handles_same_second_events);
    return UNITY_END();
}
