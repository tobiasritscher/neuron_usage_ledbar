#ifndef UNIT_TEST
#include "litellm_client.h"
#include <HTTPClient.h>

String fetchRecentLogsJson(WiFiClientSecure& client,
                            const String& host,
                            const String& path,
                            const String& apiKey) {
    HTTPClient http;
    String url = "https://" + host + path;
    if (!http.begin(client, url)) {
        return "";
    }

    http.addHeader("Authorization", "Bearer " + apiKey);
    http.setTimeout(5000);

    int statusCode = http.GET();
    String body;
    if (statusCode == 200) {
        body = http.getString();
    }
    http.end();
    return body;
}
#endif // UNIT_TEST
