#include "flash_events.h"
#include <ArduinoJson.h>
#include <cstdio>

static int64_t daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = static_cast<unsigned>(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

static int64_t parseIso8601ToEpoch(const std::string& iso) {
    int year, month, day, hour, minute, second;
    if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d",
               &year, &month, &day, &hour, &minute, &second) != 6) {
        return 0;
    }
    int64_t days = daysFromCivil(year, month, day);
    return days * 86400 + hour * 3600 + minute * 60 + second;
}

ParseResult parseFlashEvents(const std::string& jsonBody) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonBody);
    if (err) {
        return ParseResult{};
    }
    return ParseResult{true, flashEventsFromJsonDoc(doc)};
}

std::vector<FlashEvent> flashEventsFromJsonDoc(JsonDocument& doc) {
    std::vector<FlashEvent> events;

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
                                         const std::set<std::string>& seenRequestIds) {
    std::vector<FlashEvent> fresh;
    for (const auto& event : events) {
        if (seenRequestIds.find(event.requestId) == seenRequestIds.end()) {
            fresh.push_back(event);
        }
    }
    return fresh;
}
