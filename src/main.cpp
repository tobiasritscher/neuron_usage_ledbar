#ifndef UNIT_TEST

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Adafruit_NeoPixel.h>
#include <ctime>
#include <deque>
#include <set>
#include <string>

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

std::deque<std::string> seenRequestIdOrder;
std::set<std::string> seenRequestIds;
constexpr size_t kMaxSeenRequestIds = 64;
unsigned long lastPollMillis = 0;
unsigned long lastRenderMillis = 0;
int consecutiveFailures = 0;
bool isOffline = false;

void markSeen(const std::string& requestId) {
    if (seenRequestIds.insert(requestId).second) {
        seenRequestIdOrder.push_back(requestId);
        if (seenRequestIdOrder.size() > kMaxSeenRequestIds) {
            seenRequestIds.erase(seenRequestIdOrder.front());
            seenRequestIdOrder.pop_front();
        }
    }
}

std::string todayDateString() {
    time_t now = time(nullptr);
    struct tm timeInfo;
    gmtime_r(&now, &timeInfo);
    char buffer[11];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeInfo);
    return std::string(buffer);
}

static uint32_t wheel(uint8_t pos) {
    pos = 255 - pos;
    if (pos < 85) {
        return strip.Color(255 - pos * 3, 0, pos * 3);
    }
    if (pos < 170) {
        pos -= 85;
        return strip.Color(0, pos * 3, 255 - pos * 3);
    }
    pos -= 170;
    return strip.Color(pos * 3, 255 - pos * 3, 0);
}

// Runs once at boot, before WiFi/network setup, so it always plays
// regardless of connectivity — confirms the strip and wiring work
// independent of whether real data ever arrives.
void runStartupLedSelfTest() {
    for (int frame = 0; frame < 60; frame++) {
        for (int i = 0; i < LED_COUNT; i++) {
            strip.setPixelColor(i, wheel(((i * 256 / LED_COUNT) + frame * 8) & 255));
        }
        strip.show();
        delay(25);
    }
    strip.clear();
    strip.show();
    delay(150);

    for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255));
        strip.show();
        delay(35);
    }
    delay(200);
    for (int i = LED_COUNT - 1; i >= 0; i--) {
        strip.setPixelColor(i, 0);
        strip.show();
        delay(35);
    }
}

void pollLiteLlm(bool isBaselineSnapshot) {
    std::string date = todayDateString();
    char pathBuffer[256];
    snprintf(pathBuffer, sizeof(pathBuffer), LITELLM_LOGS_PATH_TEMPLATE,
              date.c_str(), date.c_str());

    bool fetchSuccess = false;
    auto events = fetchRecentFlashEvents(secureClient, LITELLM_HOST,
                                          String(pathBuffer), LITELLM_API_KEY, fetchSuccess);
    if (!fetchSuccess) {
        consecutiveFailures++;
        Serial.printf("[warn] poll failed (consecutiveFailures=%d)\n", consecutiveFailures);
        return;
    }
    consecutiveFailures = 0;

    auto freshEvents = filterNewerThan(events, seenRequestIds);
    Serial.printf("[debug] poll: events=%d fresh=%d baseline=%d\n",
                  (int)events.size(), (int)freshEvents.size(), (int)isBaselineSnapshot);

    for (const auto& event : events) {
        markSeen(event.requestId);
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
    runStartupLedSelfTest();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
    }
    Serial.printf("[info] WiFi connected, IP=%s\n", WiFi.localIP().toString().c_str());
    secureClient.setCACert(LITELLM_ROOT_CA);

    Serial.println("[debug] syncing NTP...");
    configTime(0, 0, "pool.ntp.org");
    time_t now = time(nullptr);
    while (now < 100000) {
        delay(250);
        now = time(nullptr);
    }
    Serial.println("[debug] NTP synced, doing baseline poll...");

    constexpr int kMaxBaselineAttempts = 5;
    for (int attempt = 0; attempt < kMaxBaselineAttempts; attempt++) {
        pollLiteLlm(true);
        if (consecutiveFailures == 0) {
            break;
        }
        delay(1000);
    }
    Serial.println("[debug] baseline poll done, entering loop()");
    lastPollMillis = millis();
}

void loop() {
    unsigned long nowMillis = millis();

    if (WiFi.status() != WL_CONNECTED) {
        renderer.renderOfflinePulse(nowMillis);
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
            int ledCount = rateToLedCount(smoothedRate, mappingConfig);
            Serial.printf("[debug] rate=%.0f ledCount=%d\n", smoothedRate, ledCount);
            renderer.setBaseline(ledCount, rateToColor(smoothedRate, mappingConfig));
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
