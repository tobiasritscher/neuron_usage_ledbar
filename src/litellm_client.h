#pragma once
#ifndef UNIT_TEST
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <vector>
#include "flash_events.h"

// Fetches and parses the newest-first request-log page from the LiteLLM
// proxy. Buffers the response body into a std::string before parsing
// (simpler and more reliable in practice than streaming the JSON parser
// directly off the TLS stream, which showed intermittent "IncompleteInput"
// failures on real hardware). Not an Arduino String: that caps at 64KB
// without PSRAM, and real 8-entry pages run 60-80KB. Sets outSuccess to
// true only if the HTTP request succeeded (status 200) AND the JSON parsed
// without error — check it to distinguish "successfully fetched zero events"
// from "the fetch itself failed" (in which case the returned vector is
// empty but outSuccess is false).
std::vector<FlashEvent> fetchRecentFlashEvents(WiFiClientSecure& client,
                                                const String& host,
                                                const String& path,
                                                const String& apiKey,
                                                bool& outSuccess);

#endif // UNIT_TEST
