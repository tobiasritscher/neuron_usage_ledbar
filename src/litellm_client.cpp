#ifndef UNIT_TEST
#include "litellm_client.h"
#include <HTTPClient.h>

std::vector<FlashEvent> fetchRecentFlashEvents(WiFiClientSecure& client,
                                                const String& host,
                                                const String& path,
                                                const String& apiKey,
                                                bool& outSuccess) {
    outSuccess = false;
    client.setTimeout(15000);
    HTTPClient http;
    String url = "https://" + host + path;
    if (!http.begin(client, url)) {
        return {};
    }

    http.addHeader("Authorization", "Bearer " + apiKey);
    http.setTimeout(15000);

    int statusCode = http.GET();
    std::vector<FlashEvent> events;
    if (statusCode == 200) {
        String body = http.getString();
        ParseResult result = parseFlashEvents(std::string(body.c_str()));
        if (result.ok) {
            events = result.events;
            outSuccess = true;
        }
    }
    http.end();
    client.stop();
    return events;
}
#endif // UNIT_TEST
