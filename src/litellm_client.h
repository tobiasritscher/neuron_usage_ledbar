#ifndef UNIT_TEST
#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <vector>
#include "flash_events.h"

// Fetches and parses the newest-first request-log page from the LiteLLM
// proxy, parsing directly from the HTTP response stream (avoids buffering
// the full response body as a contiguous String, which fails on ESP32 for
// large multi-hundred-KB JSON payloads). Sets outSuccess to true only if
// the HTTP request succeeded (status 200) AND the JSON parsed without
// error — check it to distinguish "successfully fetched zero events" from
// "the fetch itself failed" (in which case the returned vector is empty
// but outSuccess is false).
std::vector<FlashEvent> fetchRecentFlashEvents(WiFiClientSecure& client,
                                                const String& host,
                                                const String& path,
                                                const String& apiKey,
                                                bool& outSuccess);

#endif // UNIT_TEST
