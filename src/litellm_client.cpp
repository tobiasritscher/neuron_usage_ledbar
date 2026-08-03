#ifndef UNIT_TEST
#include "litellm_client.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

std::vector<FlashEvent> fetchRecentFlashEvents(WiFiClientSecure& client,
                                                const String& host,
                                                const String& path,
                                                const String& apiKey,
                                                bool& outSuccess) {
    outSuccess = false;
    HTTPClient http;
    String url = "https://" + host + path;
    if (!http.begin(client, url)) {
        return {};
    }

    http.addHeader("Authorization", "Bearer " + apiKey);
    http.setTimeout(5000);

    int statusCode = http.GET();
    std::vector<FlashEvent> events;
    if (statusCode == 200) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getStream());
        if (!err) {
            events = flashEventsFromJsonDoc(doc);
            outSuccess = true;
        }
    }
    http.end();
    return events;
}
#endif // UNIT_TEST
