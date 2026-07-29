#ifndef UNIT_TEST
#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

// Fetches the raw JSON body of the newest-first request-log page from the
// LiteLLM proxy. Returns an empty string on any network/HTTP failure.
String fetchRecentLogsJson(WiFiClientSecure& client,
                            const String& host,
                            const String& path,
                            const String& apiKey);

#endif // UNIT_TEST
