#pragma once
#include <string>
#include <vector>
#include <set>
#include <cstdint>
#include <ArduinoJson.h>

struct FlashEvent {
    std::string requestId;
    int64_t timestampEpoch = 0;
    uint32_t promptTokens = 0;
    double spend = 0.0; // USD, as billed by LiteLLM
};

// Result of parsing a LiteLLM logs response body. `ok` is true only if the
// JSON deserialized without error — check it to distinguish "parsed
// successfully with zero events" from "parsing failed" (in which case
// `events` is empty but that's not itself evidence of an empty result).
struct ParseResult {
    bool ok = false;
    std::vector<FlashEvent> events;
};

ParseResult parseFlashEvents(const std::string& jsonBody);

// Extracts FlashEvents from an already-deserialized JsonDocument. Shared by
// parseFlashEvents (native tests, deserializes from a string) and the
// hardware path (deserializes directly from an HTTP stream to avoid
// buffering large responses as a contiguous String, which fails on ESP32's
// fragmented heap for multi-hundred-KB payloads).
std::vector<FlashEvent> flashEventsFromJsonDoc(JsonDocument& doc);

// Filters out any event whose requestId is already in seenRequestIds,
// returning only genuinely new events. Dedup is by request_id rather than by
// a single (requestId, epoch) high-water mark because ISO8601 timestamps are
// only second-precision after parsing — multiple distinct requests can share
// the same timestampEpoch, and a high-water-mark approach would re-emit
// those siblings on every poll forever.
std::vector<FlashEvent> filterNewerThan(const std::vector<FlashEvent>& events,
                                         const std::set<std::string>& seenRequestIds);
