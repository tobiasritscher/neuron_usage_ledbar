#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct FlashEvent {
    std::string requestId;
    int64_t timestampEpoch = 0;
    uint32_t promptTokens = 0;
};

std::vector<FlashEvent> parseFlashEvents(const std::string& jsonBody);

std::vector<FlashEvent> filterNewerThan(const std::vector<FlashEvent>& events,
                                         const std::string& lastSeenRequestId,
                                         int64_t lastSeenEpoch);
