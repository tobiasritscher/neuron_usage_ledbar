#ifndef UNIT_TEST
#include "litellm_client.h"
#include <HTTPClient.h>
#include <string>

namespace {
// Arduino String caps at 65'535 bytes on boards without PSRAM (CAPACITY_MAX
// in WString.h) — getString() on a larger body silently returns "" or a
// truncated prefix, regardless of free heap. std::string has no such cap, so
// the body is buffered into one via HTTPClient::writeToStream instead.
class StdStringSink : public Stream {
public:
    explicit StdStringSink(std::string& out) : out_(out) {}
    size_t write(uint8_t c) override {
        out_.push_back(static_cast<char>(c));
        return 1;
    }
    size_t write(const uint8_t* buffer, size_t size) override {
        out_.append(reinterpret_cast<const char*>(buffer), size);
        return size;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }

private:
    std::string& out_;
};
} // namespace

std::vector<FlashEvent> fetchRecentFlashEvents(WiFiClientSecure& client,
                                                const String& host,
                                                const String& path,
                                                const String& apiKey,
                                                bool& outSuccess) {
    outSuccess = false;
    client.setTimeout(15000);
    // NetworkClientSecure defaults handshake_timeout to 120000ms internally,
    // independent of setTimeout()'s 15s — without this, a stalled TCP
    // connect/TLS handshake (e.g. packets silently dropped rather than
    // refused) can block for up to 2 minutes regardless of the read timeout
    // set below.
    client.setHandshakeTimeout(15000);
    HTTPClient http;
    String url = "https://" + host + path;
    if (!http.begin(client, url)) {
        return {};
    }

    http.addHeader("Authorization", "Bearer " + apiKey);
    http.setTimeout(15000);

    int statusCode = http.GET();
    if (statusCode == HTTPC_ERROR_CONNECTION_REFUSED) {
        // Covers any TCP/TLS connect failure (e.g. start_ssl_client), which
        // fails fast and is usually transient — one immediate retry.
        Serial.println("[warn] connect failed, retrying once");
        client.stop();
        statusCode = http.GET();
    }
    std::vector<FlashEvent> events;
    if (statusCode == 200) {
        std::string body;
        if (http.getSize() > 0) {
            body.reserve(http.getSize());
        }
        StdStringSink sink(body);
        int written = http.writeToStream(&sink);
        if (written <= 0) {
            Serial.printf("[warn] body read failed: %s (size=%d, maxBlock=%u)\n",
                          http.errorToString(written).c_str(), http.getSize(),
                          ESP.getMaxAllocHeap());
        } else {
            ParseResult result = parseFlashEvents(body);
            if (result.ok) {
                events = result.events;
                outSuccess = true;
            } else {
                Serial.printf("[warn] JSON parse failed (bodyLen=%u)\n", body.size());
            }
        }
    } else {
        Serial.printf("[warn] HTTP status=%d\n", statusCode);
    }
    http.end();
    client.stop();
    return events;
}
#endif // UNIT_TEST
