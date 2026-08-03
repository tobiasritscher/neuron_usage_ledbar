#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <ArduinoJson.h>

struct FlashEvent {
    std::string requestId;
    int64_t timestampEpoch = 0;
    uint32_t promptTokens = 0;
};

std::vector<FlashEvent> parseFlashEvents(const std::string& jsonBody);

// Extracts FlashEvents from an already-deserialized JsonDocument. Shared by
// parseFlashEvents (native tests, deserializes from a string) and the
// hardware path (deserializes directly from an HTTP stream to avoid
// buffering large responses as a contiguous String, which fails on ESP32's
// fragmented heap for multi-hundred-KB payloads).
std::vector<FlashEvent> flashEventsFromJsonDoc(JsonDocument& doc);

std::vector<FlashEvent> filterNewerThan(const std::vector<FlashEvent>& events,
                                         const std::string& lastSeenRequestId,
                                         int64_t lastSeenEpoch);
