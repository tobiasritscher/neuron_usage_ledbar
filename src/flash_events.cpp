#include "flash_events.h"
#include <ArduinoJson.h>
#include <ctime>
#include <cstdio>

static int64_t parseIso8601ToEpoch(const std::string& iso) {
    struct tm timeVal = {};
    int year, month, day, hour, minute, second;
    if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d",
               &year, &month, &day, &hour, &minute, &second) != 6) {
        return 0;
    }
    timeVal.tm_year = year - 1900;
    timeVal.tm_mon = month - 1;
    timeVal.tm_mday = day;
    timeVal.tm_hour = hour;
    timeVal.tm_min = minute;
    timeVal.tm_sec = second;
    timeVal.tm_isdst = 0;
    return static_cast<int64_t>(timegm(&timeVal));
}

std::vector<FlashEvent> parseFlashEvents(const std::string& jsonBody) {
    std::vector<FlashEvent> events;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonBody);
    if (err) {
        return events;
    }

    JsonArray arr = doc["data"].is<JsonArray>() ? doc["data"].as<JsonArray>()
                                                  : doc.as<JsonArray>();

    for (JsonObject entry : arr) {
        FlashEvent event;
        event.requestId = entry["request_id"] | "";
        event.timestampEpoch = parseIso8601ToEpoch(entry["startTime"] | "");
        event.promptTokens = entry["prompt_tokens"] | 0;
        events.push_back(event);
    }

    return events;
}

std::vector<FlashEvent> filterNewerThan(const std::vector<FlashEvent>& events,
                                         const std::string& lastSeenRequestId,
                                         int64_t lastSeenEpoch) {
    std::vector<FlashEvent> fresh;
    for (const auto& event : events) {
        bool isNewer = event.timestampEpoch > lastSeenEpoch;
        bool isSameTimeDifferentRequest =
            event.timestampEpoch == lastSeenEpoch && event.requestId != lastSeenRequestId;
        if (isNewer || isSameTimeDifferentRequest) {
            fresh.push_back(event);
        }
    }
    return fresh;
}
