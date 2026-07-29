#ifndef UNIT_TEST

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Adafruit_NeoPixel.h>
#include <ctime>

#include "config.h"
#include "secrets.h"
#include "flash_events.h"
#include "rate_tracker.h"
#include "led_mapping.h"
#include "litellm_client.h"
#include "led_renderer.h"
#include "root_ca.h"

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
WiFiClientSecure secureClient;
RateTracker rateTracker(EMA_ALPHA, RATE_WINDOW_SECONDS);
LedRenderer renderer(strip, LED_COUNT);

std::string lastSeenRequestId;
int64_t lastSeenEpoch = 0;
unsigned long lastPollMillis = 0;
unsigned long lastRenderMillis = 0;
int consecutiveFailures = 0;
bool isOffline = false;

std::string todayDateString() {
    time_t now = time(nullptr);
    struct tm timeInfo;
    gmtime_r(&now, &timeInfo);
    char buffer[11];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeInfo);
    return std::string(buffer);
}

void pollLiteLlm(bool isBaselineSnapshot) {
    std::string date = todayDateString();
    char pathBuffer[256];
    snprintf(pathBuffer, sizeof(pathBuffer), LITELLM_LOGS_PATH_TEMPLATE,
              date.c_str(), date.c_str());

    String body = fetchRecentLogsJson(secureClient, LITELLM_HOST,
                                        String(pathBuffer), LITELLM_API_KEY);
    if (body.length() == 0) {
        consecutiveFailures++;
        return;
    }
    consecutiveFailures = 0;

    auto events = parseFlashEvents(std::string(body.c_str()));
    auto freshEvents = filterNewerThan(events, lastSeenRequestId, lastSeenEpoch);

    if (!events.empty()) {
        lastSeenRequestId = events.front().requestId;
        lastSeenEpoch = events.front().timestampEpoch;
    }

    if (isBaselineSnapshot) {
        return; // don't flash on the startup snapshot
    }

    unsigned long nowMillis = millis();
    for (const auto& event : freshEvents) {
        rateTracker.addEvent(event.timestampEpoch, event.promptTokens);
        renderer.addFlash(event.promptTokens, nowMillis);
    }
}

void setup() {
    Serial.begin(115200);
    strip.begin();
    strip.setBrightness(80);
    strip.show();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
    }
    secureClient.setCACert(LITELLM_ROOT_CA);

    configTime(0, 0, "pool.ntp.org");
    time_t now = time(nullptr);
    while (now < 100000) {
        delay(250);
        now = time(nullptr);
    }

    pollLiteLlm(true);
    lastPollMillis = millis();
}

void loop() {
    unsigned long nowMillis = millis();

    if (WiFi.status() != WL_CONNECTED) {
        renderer.renderOfflinePulse(nowMillis);
        WiFi.reconnect();
        delay(200);
        return;
    }

    if (nowMillis - lastPollMillis >= POLL_INTERVAL_MS) {
        pollLiteLlm(false);
        lastPollMillis = nowMillis;

        isOffline = consecutiveFailures >= 3;

        if (!isOffline) {
            double smoothedRate = rateTracker.updateRate(static_cast<int64_t>(time(nullptr)));
            LedMappingConfig mappingConfig{RATE_MIN, RATE_MAX, LED_COUNT};
            renderer.setBaseline(rateToLedCount(smoothedRate, mappingConfig),
                                  rateToColor(smoothedRate, mappingConfig));
        }
    }

    if (nowMillis - lastRenderMillis >= RENDER_INTERVAL_MS) {
        if (isOffline) {
            renderer.renderOfflinePulse(nowMillis);
        } else {
            renderer.renderFrame(nowMillis);
        }
        lastRenderMillis = nowMillis;
    }
}

#endif // UNIT_TEST
