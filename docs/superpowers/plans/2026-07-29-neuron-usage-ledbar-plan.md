# Neuron Usage LED Bar Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Standalone ESP32-C6 firmware that drives an 18-LED bar to visualize real-time prompt-token usage across the whole `neuron.noser.com` LiteLLM instance, with a music-visualizer-style flash per request on top of a steady baseline level.

**Architecture:** PlatformIO project with pure-logic modules (JSON parsing, sliding-window rate, LED/color mapping) unit-tested natively, plus hardware-dependent modules (HTTPS client, NeoPixel renderer) verified manually, wired together in `main.cpp`'s `setup()`/`loop()`.

**Tech Stack:** PlatformIO, Arduino framework (`espressif32` platform, `esp32-c6-devkitc-1` board), ArduinoJson v7, Adafruit NeoPixel, Unity (native test framework).

## Global Constraints

- LED bar: 18 addressable LEDs, WS2812B/SK6812-compatible protocol.
- Controller: ESP32-C6 Pico Mini, standalone firmware — no companion computer needed at runtime.
- Wiring: `DAT` → GPIO2 through a 330Ω resistor; LED `VCC` from an external 5V supply, never the ESP32's onboard 5V pin; common ground across PSU, strip, and ESP32; 1000µF capacitor across LED `VCC`/`GND`.
- Metric: only `prompt_tokens` count toward the tokens/minute rate — `completion_tokens` are excluded.
- Data source: LiteLLM proxy at `https://neuron.noser.com/v1`, admin `Authorization: Bearer <key>`, paginated logs endpoint (first page, newest-first, scoped to today).
- Rate calculation: 60-second sliding window of prompt-token sums, then EMA-smoothed.
- LED mapping: 0-18 LEDs lit bottom-up, blue (low) → red (high) linear gradient, thresholds configurable.
- Flash overlay: fast attack / slower exponential decay per new request, magnitude scaled by that request's `prompt_tokens`, rendered in a loop decoupled from the ~2-3s poll interval (~30-50fps).
- Error handling: WiFi auto-reconnects in the background; a slow offline breathing pulse replaces the baseline display while disconnected or after repeated HTTP failures; JSON/HTTP failures must never crash the firmware.
- Build tool: PlatformIO, not the Arduino IDE. Pure logic lives in host-testable modules with no `Arduino.h`/hardware dependency.

---

## File Structure

```
neuron_usage_ledbar/
  platformio.ini
  .gitignore
  src/
    config.h              # tunable constants (pins, thresholds, endpoint template)
    secrets.h.example      # template for WiFi/API credentials (secrets.h is gitignored)
    flash_events.h/.cpp    # JSON -> FlashEvent parsing, "what's new since last poll"
    rate_tracker.h/.cpp    # 60s sliding window + EMA smoothing
    led_mapping.h/.cpp     # rate -> LED count + blue/red gradient color
    litellm_client.h/.cpp  # HTTPS GET against the LiteLLM logs endpoint
    led_renderer.h/.cpp    # NeoPixel baseline + flash-overlay rendering
    main.cpp               # setup()/loop() wiring everything together
  test/
    test_flash_events/test_main.cpp
    test_rate_tracker/test_main.cpp
    test_led_mapping/test_main.cpp
```

---

### Task 1: Confirm the real LiteLLM logs endpoint and response schema

**Files:**
- Create: `docs/superpowers/plans/2026-07-29-litellm-endpoint-notes.md`

**Interfaces:**
- Produces: a confirmed endpoint path/query-param template and confirmed JSON field names, which Task 3 (`flash_events.cpp`) and Task 6 (`litellm_client.cpp`) consume.

This is a discovery task, not a coding task — the real schema can't be verified without your live API key.

- [ ] **Step 1: Generate the admin API key**

In the LiteLLM UI (the same one showing the "Request Logs / Live Tail" screen), generate an API key with admin/read access to spend logs.

- [ ] **Step 2: Run a real request against the assumed endpoint**

```bash
export LITELLM_KEY="paste-your-key-here"
TODAY=$(date +%F)
curl -s "https://neuron.noser.com/v1/spend/logs?start_date=${TODAY}&end_date=${TODAY}&summarize=false" \
  -H "Authorization: Bearer ${LITELLM_KEY}" | head -c 2000
```

- [ ] **Step 3: Compare the real response against the assumed schema**

Check for each of these in the response, and note any mismatch:
- Is the array of entries wrapped in a `"data"` key, or is the response itself a top-level JSON array?
- Does each entry have `request_id`, `prompt_tokens`, `completion_tokens`?
- Is the timestamp field named `startTime`, and is it an ISO 8601 string like `"2026-07-29T16:30:11.123456"` (no timezone suffix, treat as UTC)?
- Is the newest entry first (descending by time), matching the Live Tail view's ordering?

- [ ] **Step 4: Write down the findings**

```markdown
# LiteLLM Logs Endpoint — Confirmed Schema (2026-07-29)

Endpoint: <exact path and query params that worked>
Wrapper key: <"data" | none — top-level array>
Timestamp field: <field name> — format: <observed format>
Confirmed fields per entry: request_id, prompt_tokens, completion_tokens, <field name for time>
Ordering: <newest-first confirmed? Y/N>
```

Fill in `docs/superpowers/plans/2026-07-29-litellm-endpoint-notes.md` with the actual findings.

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/plans/2026-07-29-litellm-endpoint-notes.md
git commit -m "docs: confirm LiteLLM logs endpoint schema"
```

**If the real schema differs from the assumed one** (top-level array instead of `data`-wrapped, different field names), apply the same difference when writing Task 3's `parseFlashEvents` and Task 8's `LITELLM_LOGS_PATH_TEMPLATE` — the code in those tasks is written against the assumed schema below and must be adjusted to match your Step 4 findings before those tasks are considered done.

---

### Task 2: PlatformIO project scaffold

**Files:**
- Create: `platformio.ini`
- Create: `.gitignore`
- Create: `src/config.h`
- Create: `src/secrets.h.example`
- Create: `src/main.cpp` (placeholder blink, replaced fully in Task 8)

**Interfaces:**
- Produces: `esp32-c6` and `native` PlatformIO environments that later tasks' files slot into.

- [ ] **Step 1: Write `platformio.ini`**

```ini
[env:esp32-c6]
platform = espressif32
board = esp32-c6-devkitc-1
framework = arduino
monitor_speed = 115200
lib_deps =
    adafruit/Adafruit NeoPixel@^1.12.0
    bblanchon/ArduinoJson@^7.0.4

[env:native]
platform = native
lib_deps =
    bblanchon/ArduinoJson@^7.0.4
    throwtheswitch/Unity@^2.5.2
build_flags = -std=gnu++17
test_framework = unity
```

- [ ] **Step 2: Write `.gitignore`**

```
.pio/
src/secrets.h
```

- [ ] **Step 3: Write `src/secrets.h.example`**

```cpp
#pragma once

constexpr const char* WIFI_SSID = "your-wifi-ssid";
constexpr const char* WIFI_PASSWORD = "your-wifi-password";
constexpr const char* LITELLM_API_KEY = "sk-...";
```

- [ ] **Step 4: Write `src/config.h`**

```cpp
#pragma once

constexpr int LED_PIN = 2;
constexpr int LED_COUNT = 18;

constexpr const char* LITELLM_HOST = "neuron.noser.com";
// Confirmed against the real API in Task 1 (see
// docs/superpowers/plans/2026-07-29-litellm-endpoint-notes.md): the admin
// spend routes live at the bare host, no /v1 prefix, and start_date/end_date
// need full "YYYY-MM-DD HH:MM:SS" datetimes, not bare dates. "+" is decoded
// as a space by the server, so the two %s slots still take a plain
// YYYY-MM-DD date each — no other code needs to change.
constexpr const char* LITELLM_LOGS_PATH_TEMPLATE =
    "/spend/logs/ui?start_date=%s+00:00:00&end_date=%s+23:59:59&page=1&page_size=20";

constexpr unsigned long POLL_INTERVAL_MS = 2500;
constexpr unsigned long RENDER_INTERVAL_MS = 25; // ~40fps

// Initial defaults — tuned against real traffic in Task 9.
constexpr double RATE_MIN = 0.0;
constexpr double RATE_MAX = 2000.0;

constexpr double EMA_ALPHA = 0.3;
constexpr long RATE_WINDOW_SECONDS = 60;
```

- [ ] **Step 5: Write a placeholder `src/main.cpp`**

```cpp
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
}

void loop() {
    Serial.println("scaffold ok");
    delay(1000);
}
```

- [ ] **Step 6: Verify the native environment builds (no source to test yet, just toolchain check)**

Run: `pio test -e native`
Expected: `No test files found` or similar — this confirms PlatformIO and the native platform are installed and configured correctly, not a real test pass yet.

- [ ] **Step 7: Copy secrets template and fill in real values**

```bash
cp src/secrets.h.example src/secrets.h
```

Edit `src/secrets.h` with your real WiFi SSID/password and the API key from Task 1.

- [ ] **Step 8: Commit**

```bash
git add platformio.ini .gitignore src/config.h src/secrets.h.example src/main.cpp
git commit -m "chore: scaffold PlatformIO project for neuron usage LED bar"
```

(`src/secrets.h` is gitignored — verify with `git status` that it does not appear before committing.)

---

### Task 3: `flash_events` — parse LiteLLM logs into flash events

**Files:**
- Create: `src/flash_events.h`
- Create: `src/flash_events.cpp`
- Test: `test/test_flash_events/test_main.cpp`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces:
  - `struct FlashEvent { std::string requestId; int64_t timestampEpoch; uint32_t promptTokens; }`
  - `std::vector<FlashEvent> parseFlashEvents(const std::string& jsonBody)` — used by Task 8.
  - `std::vector<FlashEvent> filterNewerThan(const std::vector<FlashEvent>& events, const std::string& lastSeenRequestId, int64_t lastSeenEpoch)` — used by Task 8.

- [ ] **Step 1: Write the failing tests**

```cpp
// test/test_flash_events/test_main.cpp
#include <unity.h>
#include "flash_events.h"

void test_parse_two_events() {
    std::string json = R"({
        "data": [
            {"request_id": "req-2", "startTime": "2026-07-29T16:30:11", "prompt_tokens": 125},
            {"request_id": "req-1", "startTime": "2026-07-29T16:30:06", "prompt_tokens": 102}
        ]
    })";

    auto events = parseFlashEvents(json);

    TEST_ASSERT_EQUAL(2, events.size());
    TEST_ASSERT_EQUAL_STRING("req-2", events[0].requestId.c_str());
    TEST_ASSERT_EQUAL_UINT32(125, events[0].promptTokens);
    TEST_ASSERT_TRUE(events[0].timestampEpoch > events[1].timestampEpoch);
}

void test_filter_newer_than_excludes_seen() {
    std::vector<FlashEvent> events = {
        {"req-2", 1000, 50},
        {"req-1", 900, 30},
    };

    auto fresh = filterNewerThan(events, "req-1", 900);

    TEST_ASSERT_EQUAL(1, fresh.size());
    TEST_ASSERT_EQUAL_STRING("req-2", fresh[0].requestId.c_str());
}

void test_filter_newer_than_empty_when_nothing_new() {
    std::vector<FlashEvent> events = {
        {"req-1", 900, 30},
    };

    auto fresh = filterNewerThan(events, "req-1", 900);

    TEST_ASSERT_EQUAL(0, fresh.size());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_two_events);
    RUN_TEST(test_filter_newer_than_excludes_seen);
    RUN_TEST(test_filter_newer_than_empty_when_nothing_new);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_flash_events`
Expected: FAIL — `flash_events.h` does not exist yet.

- [ ] **Step 3: Write `src/flash_events.h`**

```cpp
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
```

- [ ] **Step 4: Write `src/flash_events.cpp`**

```cpp
#include "flash_events.h"
#include <ArduinoJson.h>
#include <ctime>
#include <cstdio>

static int64_t parseIso8601ToEpoch(const std::string& iso) {
    struct tm timeVal = {};
    int year, month, day, hour, minute, second;
    if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d",
               &year, &month, &day, &hour, &minute, &second) != 6) {
        return 0;
    }
    timeVal.tm_year = year - 1900;
    timeVal.tm_mon = month - 1;
    timeVal.tm_mday = day;
    timeVal.tm_hour = hour;
    timeVal.tm_min = minute;
    timeVal.tm_sec = second;
    timeVal.tm_isdst = 0;
    return static_cast<int64_t>(timegm(&timeVal));
}

std::vector<FlashEvent> parseFlashEvents(const std::string& jsonBody) {
    std::vector<FlashEvent> events;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonBody);
    if (err) {
        return events;
    }

    JsonArray arr = doc["data"].is<JsonArray>() ? doc["data"].as<JsonArray>()
                                                  : doc.as<JsonArray>();

    for (JsonObject entry : arr) {
        FlashEvent event;
        event.requestId = entry["request_id"] | "";
        event.timestampEpoch = parseIso8601ToEpoch(entry["startTime"] | "");
        event.promptTokens = entry["prompt_tokens"] | 0;
        events.push_back(event);
    }

    return events;
}

std::vector<FlashEvent> filterNewerThan(const std::vector<FlashEvent>& events,
                                         const std::string& lastSeenRequestId,
                                         int64_t lastSeenEpoch) {
    std::vector<FlashEvent> fresh;
    for (const auto& event : events) {
        bool isNewer = event.timestampEpoch > lastSeenEpoch;
        bool isSameTimeDifferentRequest =
            event.timestampEpoch == lastSeenEpoch && event.requestId != lastSeenRequestId;
        if (isNewer || isSameTimeDifferentRequest) {
            fresh.push_back(event);
        }
    }
    return fresh;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_flash_events`
Expected: PASS (3 tests)

- [ ] **Step 6: Commit**

```bash
git add src/flash_events.h src/flash_events.cpp test/test_flash_events/test_main.cpp
git commit -m "feat: parse LiteLLM logs into flash events"
```

---

### Task 4: `rate_tracker` — sliding-window rate with EMA smoothing

**Files:**
- Create: `src/rate_tracker.h`
- Create: `src/rate_tracker.cpp`
- Test: `test/test_rate_tracker/test_main.cpp`

**Interfaces:**
- Consumes: nothing from earlier tasks (takes raw `(timestampEpoch, tokens)` pairs — Task 8 supplies these from `FlashEvent`s produced by Task 3).
- Produces: `class RateTracker` with `addEvent(int64_t timestampEpoch, uint32_t tokens)`, `double updateRate(int64_t nowEpoch)`, `double smoothedRate() const` — used by Task 8.

- [ ] **Step 1: Write the failing tests**

```cpp
// test/test_rate_tracker/test_main.cpp
#include <unity.h>
#include "rate_tracker.h"

void test_rate_zero_with_no_events() {
    RateTracker tracker(0.3, 60);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, tracker.updateRate(1000));
}

void test_rate_reflects_window_sum() {
    RateTracker tracker(1.0, 60); // alpha=1.0 disables smoothing for this test
    tracker.addEvent(950, 100);
    tracker.addEvent(970, 200);

    double rate = tracker.updateRate(1000);

    TEST_ASSERT_EQUAL_DOUBLE(300.0, rate); // 300 tokens inside a 60s window = 300/min
}

void test_rate_drops_events_outside_window() {
    RateTracker tracker(1.0, 60);
    tracker.addEvent(800, 500); // 200s before "now" — outside the 60s window
    tracker.addEvent(990, 100);

    double rate = tracker.updateRate(1000);

    TEST_ASSERT_EQUAL_DOUBLE(100.0, rate);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_rate_zero_with_no_events);
    RUN_TEST(test_rate_reflects_window_sum);
    RUN_TEST(test_rate_drops_events_outside_window);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_rate_tracker`
Expected: FAIL — `rate_tracker.h` does not exist yet.

- [ ] **Step 3: Write `src/rate_tracker.h`**

```cpp
#pragma once
#include <cstdint>
#include <deque>

class RateTracker {
public:
    explicit RateTracker(double emaAlpha = 0.3, int64_t windowSeconds = 60);

    void addEvent(int64_t timestampEpoch, uint32_t tokens);
    double updateRate(int64_t nowEpoch);
    double smoothedRate() const { return smoothedRate_; }

private:
    struct Sample {
        int64_t timestampEpoch;
        uint32_t tokens;
    };

    std::deque<Sample> samples_;
    int64_t windowSeconds_;
    double emaAlpha_;
    double smoothedRate_ = 0.0;
};
```

- [ ] **Step 4: Write `src/rate_tracker.cpp`**

```cpp
#include "rate_tracker.h"

RateTracker::RateTracker(double emaAlpha, int64_t windowSeconds)
    : windowSeconds_(windowSeconds), emaAlpha_(emaAlpha) {}

void RateTracker::addEvent(int64_t timestampEpoch, uint32_t tokens) {
    samples_.push_back({timestampEpoch, tokens});
}

double RateTracker::updateRate(int64_t nowEpoch) {
    while (!samples_.empty() &&
           samples_.front().timestampEpoch < nowEpoch - windowSeconds_) {
        samples_.pop_front();
    }

    uint64_t windowTotal = 0;
    for (const auto& sample : samples_) {
        windowTotal += sample.tokens;
    }

    double rawRatePerMinute =
        static_cast<double>(windowTotal) * (60.0 / static_cast<double>(windowSeconds_));

    smoothedRate_ = smoothedRate_ + emaAlpha_ * (rawRatePerMinute - smoothedRate_);
    return smoothedRate_;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_rate_tracker`
Expected: PASS (3 tests)

- [ ] **Step 6: Commit**

```bash
git add src/rate_tracker.h src/rate_tracker.cpp test/test_rate_tracker/test_main.cpp
git commit -m "feat: add sliding-window rate tracker with EMA smoothing"
```

---

### Task 5: `led_mapping` — rate to LED count and gradient color

**Files:**
- Create: `src/led_mapping.h`
- Create: `src/led_mapping.cpp`
- Test: `test/test_led_mapping/test_main.cpp`

**Interfaces:**
- Consumes: nothing from earlier tasks (takes a plain `double` rate — Task 8 supplies `rateTracker.updateRate(...)`'s return value).
- Produces:
  - `struct RGB { uint8_t r; uint8_t g; uint8_t b; }`
  - `struct LedMappingConfig { double rateMin; double rateMax; int ledCount; }`
  - `int rateToLedCount(double smoothedRate, const LedMappingConfig& config)` — used by Task 8.
  - `RGB rateToColor(double smoothedRate, const LedMappingConfig& config)` — used by Task 8.

- [ ] **Step 1: Write the failing tests**

```cpp
// test/test_led_mapping/test_main.cpp
#include <unity.h>
#include "led_mapping.h"

void test_led_count_at_min_is_zero() {
    LedMappingConfig config{0.0, 1000.0, 18};
    TEST_ASSERT_EQUAL(0, rateToLedCount(0.0, config));
}

void test_led_count_at_max_is_full() {
    LedMappingConfig config{0.0, 1000.0, 18};
    TEST_ASSERT_EQUAL(18, rateToLedCount(1000.0, config));
}

void test_led_count_clamped_above_max() {
    LedMappingConfig config{0.0, 1000.0, 18};
    TEST_ASSERT_EQUAL(18, rateToLedCount(5000.0, config));
}

void test_color_is_blue_at_min() {
    LedMappingConfig config{0.0, 1000.0, 18};
    RGB color = rateToColor(0.0, config);
    TEST_ASSERT_EQUAL_UINT8(0, color.r);
    TEST_ASSERT_EQUAL_UINT8(255, color.b);
}

void test_color_is_red_at_max() {
    LedMappingConfig config{0.0, 1000.0, 18};
    RGB color = rateToColor(1000.0, config);
    TEST_ASSERT_EQUAL_UINT8(255, color.r);
    TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_led_count_at_min_is_zero);
    RUN_TEST(test_led_count_at_max_is_full);
    RUN_TEST(test_led_count_clamped_above_max);
    RUN_TEST(test_color_is_blue_at_min);
    RUN_TEST(test_color_is_red_at_max);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_led_mapping`
Expected: FAIL — `led_mapping.h` does not exist yet.

- [ ] **Step 3: Write `src/led_mapping.h`**

```cpp
#pragma once
#include <cstdint>

struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct LedMappingConfig {
    double rateMin;
    double rateMax;
    int ledCount;
};

int rateToLedCount(double smoothedRate, const LedMappingConfig& config);
RGB rateToColor(double smoothedRate, const LedMappingConfig& config);
```

- [ ] **Step 4: Write `src/led_mapping.cpp`**

```cpp
#include "led_mapping.h"
#include <algorithm>

static double normalizedFraction(double smoothedRate, const LedMappingConfig& config) {
    if (config.rateMax <= config.rateMin) {
        return 0.0;
    }
    double fraction = (smoothedRate - config.rateMin) / (config.rateMax - config.rateMin);
    return std::min(1.0, std::max(0.0, fraction));
}

int rateToLedCount(double smoothedRate, const LedMappingConfig& config) {
    double fraction = normalizedFraction(smoothedRate, config);
    return static_cast<int>(fraction * config.ledCount + 0.5);
}

RGB rateToColor(double smoothedRate, const LedMappingConfig& config) {
    double fraction = normalizedFraction(smoothedRate, config);
    uint8_t red = static_cast<uint8_t>(255 * fraction);
    uint8_t blue = static_cast<uint8_t>(255 * (1.0 - fraction));
    return RGB{red, 0, blue};
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_led_mapping`
Expected: PASS (5 tests)

- [ ] **Step 6: Commit**

```bash
git add src/led_mapping.h src/led_mapping.cpp test/test_led_mapping/test_main.cpp
git commit -m "feat: add rate-to-LED-count and rate-to-color gradient mapping"
```

---

### Task 6: `litellm_client` — HTTPS fetch of the logs endpoint

**Files:**
- Create: `src/litellm_client.h`
- Create: `src/litellm_client.cpp`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: `String fetchRecentLogsJson(WiFiClientSecure& client, const String& host, const String& path, const String& apiKey)` — used by Task 8. Hardware-dependent (WiFi/TLS); not natively testable, verified manually via serial output in this task.

- [ ] **Step 1: Write `src/litellm_client.h`**

```cpp
#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

// Fetches the raw JSON body of the newest-first request-log page from the
// LiteLLM proxy. Returns an empty string on any network/HTTP failure.
String fetchRecentLogsJson(WiFiClientSecure& client,
                            const String& host,
                            const String& path,
                            const String& apiKey);
```

- [ ] **Step 2: Write `src/litellm_client.cpp`**

```cpp
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
```

- [ ] **Step 3: Write a manual smoke-test sketch to verify it against the real API**

Temporarily replace `src/main.cpp` with this (Task 8 replaces it again with the full integration):

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "secrets.h"
#include "litellm_client.h"

WiFiClientSecure secureClient;

void setup() {
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    secureClient.setInsecure();

    char pathBuffer[256];
    snprintf(pathBuffer, sizeof(pathBuffer), LITELLM_LOGS_PATH_TEMPLATE,
             "2026-07-29", "2026-07-29");

    String body = fetchRecentLogsJson(secureClient, LITELLM_HOST, String(pathBuffer),
                                       LITELLM_API_KEY);
    Serial.println("Response length: " + String(body.length()));
    Serial.println(body.substring(0, 500));
}

void loop() {}
```

- [ ] **Step 4: Flash and observe**

Run: `pio run -e esp32-c6 -t upload -t monitor`
Expected: serial output shows `WiFi connected`, a non-zero response length, and the first 500 characters of real JSON matching the schema confirmed in Task 1.

- [ ] **Step 5: Commit**

```bash
git add src/litellm_client.h src/litellm_client.cpp
git commit -m "feat: add HTTPS client for LiteLLM logs endpoint"
```

(Leave `main.cpp` as this smoke-test version for now — Task 8 replaces it with the full integration.)

---

### Task 7: `led_renderer` — baseline bar plus flash overlay

**Files:**
- Create: `src/led_renderer.h`
- Create: `src/led_renderer.cpp`

**Interfaces:**
- Consumes: `RGB` and nothing else structurally from Task 5 (takes plain LED count + `RGB` as baseline inputs — Task 8 supplies these from `rateToLedCount`/`rateToColor`).
- Produces: `class LedRenderer` with `setBaseline(int ledCount, RGB color)`, `addFlash(uint32_t promptTokens, unsigned long nowMillis)`, `renderFrame(unsigned long nowMillis)`, `renderOfflinePulse(unsigned long nowMillis)` — used by Task 8. Hardware-dependent (NeoPixel); verified manually via physical observation in this task.

- [ ] **Step 1: Write `src/led_renderer.h`**

```cpp
#pragma once
#include <Adafruit_NeoPixel.h>
#include <vector>
#include "led_mapping.h"

struct ActiveFlash {
    unsigned long startMillis;
    uint32_t promptTokens;
};

class LedRenderer {
public:
    LedRenderer(Adafruit_NeoPixel& strip, int ledCount);

    void setBaseline(int ledCount, RGB color);
    void addFlash(uint32_t promptTokens, unsigned long nowMillis);
    void renderFrame(unsigned long nowMillis);
    void renderOfflinePulse(unsigned long nowMillis);

private:
    Adafruit_NeoPixel& strip_;
    int ledCount_;
    int baselineLedCount_ = 0;
    RGB baselineColor_ = {0, 0, 255};
    std::vector<ActiveFlash> activeFlashes_;

    static constexpr unsigned long kFlashDurationMs = 600;
};
```

- [ ] **Step 2: Write `src/led_renderer.cpp`**

```cpp
#include "led_renderer.h"
#include <algorithm>
#include <cmath>

LedRenderer::LedRenderer(Adafruit_NeoPixel& strip, int ledCount)
    : strip_(strip), ledCount_(ledCount) {}

void LedRenderer::setBaseline(int ledCount, RGB color) {
    baselineLedCount_ = ledCount;
    baselineColor_ = color;
}

void LedRenderer::addFlash(uint32_t promptTokens, unsigned long nowMillis) {
    activeFlashes_.push_back({nowMillis, promptTokens});
}

void LedRenderer::renderFrame(unsigned long nowMillis) {
    activeFlashes_.erase(
        std::remove_if(activeFlashes_.begin(), activeFlashes_.end(),
                        [&](const ActiveFlash& flash) {
                            return nowMillis - flash.startMillis > kFlashDurationMs;
                        }),
        activeFlashes_.end());

    double flashBoost = 0.0;
    for (const auto& flash : activeFlashes_) {
        double age = static_cast<double>(nowMillis - flash.startMillis);
        double decay = std::exp(-age / (kFlashDurationMs / 4.0));
        double magnitude = std::min(1.0, flash.promptTokens / 500.0);
        flashBoost = std::max(flashBoost, magnitude * decay);
    }

    for (int i = 0; i < ledCount_; i++) {
        if (i < baselineLedCount_) {
            uint8_t r = static_cast<uint8_t>(std::min(255.0, baselineColor_.r + flashBoost * 255.0));
            uint8_t g = static_cast<uint8_t>(std::min(255.0, baselineColor_.g + flashBoost * 255.0));
            uint8_t b = static_cast<uint8_t>(std::min(255.0, baselineColor_.b + flashBoost * 255.0));
            strip_.setPixelColor(i, strip_.Color(r, g, b));
        } else {
            strip_.setPixelColor(i, 0);
        }
    }
    strip_.show();
}

void LedRenderer::renderOfflinePulse(unsigned long nowMillis) {
    double phase = (std::sin(nowMillis / 500.0) + 1.0) / 2.0;
    uint8_t brightness = static_cast<uint8_t>(30 + phase * 40);
    for (int i = 0; i < ledCount_; i++) {
        strip_.setPixelColor(i, strip_.Color(brightness, brightness, brightness));
    }
    strip_.show();
}
```

- [ ] **Step 3: Write a manual smoke-test sketch**

Temporarily replace `src/main.cpp`:

```cpp
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "led_renderer.h"

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
LedRenderer renderer(strip, LED_COUNT);

void setup() {
    strip.begin();
    strip.setBrightness(80);
    renderer.setBaseline(9, RGB{0, 0, 255}); // half the bar, blue
}

void loop() {
    unsigned long nowMillis = millis();
    static unsigned long lastFlash = 0;
    if (nowMillis - lastFlash > 2000) {
        renderer.addFlash(400, nowMillis); // simulate a request every 2s
        lastFlash = nowMillis;
    }
    renderer.renderFrame(nowMillis);
    delay(25);
}
```

- [ ] **Step 4: Flash and visually verify**

Run: `pio run -e esp32-c6 -t upload`
Expected: 9 of 18 LEDs lit blue at rest, with a brief brighter flash sweeping across those 9 LEDs every ~2 seconds that fades out smoothly.

- [ ] **Step 5: Commit**

```bash
git add src/led_renderer.h src/led_renderer.cpp
git commit -m "feat: add NeoPixel baseline + flash-overlay renderer"
```

---

### Task 8: Full integration in `main.cpp`

**Files:**
- Modify: `src/main.cpp` (replace the Task 6/7 smoke-test versions entirely)

**Interfaces:**
- Consumes: `parseFlashEvents`/`filterNewerThan` (Task 3), `RateTracker` (Task 4), `rateToLedCount`/`rateToColor`/`LedMappingConfig` (Task 5), `fetchRecentLogsJson` (Task 6), `LedRenderer` (Task 7), constants from `config.h` (Task 2).
- Produces: the running firmware; no further tasks consume this directly.

- [ ] **Step 1: Write the full `src/main.cpp`**

```cpp
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

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
WiFiClientSecure secureClient;
RateTracker rateTracker(EMA_ALPHA, RATE_WINDOW_SECONDS);
LedRenderer renderer(strip, LED_COUNT);

std::string lastSeenRequestId;
int64_t lastSeenEpoch = 0;
unsigned long lastPollMillis = 0;
unsigned long lastRenderMillis = 0;
int consecutiveFailures = 0;

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
    secureClient.setInsecure();

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

        if (consecutiveFailures >= 3) {
            renderer.renderOfflinePulse(nowMillis);
            return;
        }

        double smoothedRate = rateTracker.updateRate(static_cast<int64_t>(time(nullptr)));
        LedMappingConfig mappingConfig{RATE_MIN, RATE_MAX, LED_COUNT};
        renderer.setBaseline(rateToLedCount(smoothedRate, mappingConfig),
                              rateToColor(smoothedRate, mappingConfig));
    }

    if (nowMillis - lastRenderMillis >= RENDER_INTERVAL_MS) {
        renderer.renderFrame(nowMillis);
        lastRenderMillis = nowMillis;
    }
}
```

- [ ] **Step 2: Build for the ESP32-C6 target**

Run: `pio run -e esp32-c6`
Expected: build succeeds with no errors.

- [ ] **Step 3: Flash and watch serial output**

Run: `pio run -e esp32-c6 -t upload -t monitor`
Expected: connects to WiFi, then the bar settles to a baseline level within one poll interval (~2.5s) with no crashes/reboots over a few minutes of observation.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat: wire full poll/rate/render loop into main.cpp"
```

---

### Task 9: Hardware bring-up and calibration

**Files:**
- Modify: `src/config.h:RATE_MIN,RATE_MAX` (tune from measured real-world traffic)

**Interfaces:**
- Consumes: the fully-integrated firmware from Task 8.
- Produces: calibrated thresholds; this is the final task.

- [ ] **Step 1: Wire the hardware per the spec**

Connect, with the original BLE controller unplugged:
- LED strip `GND` → ESP32 `GND`
- LED strip `DAT` → ESP32 `GPIO2`, through a 330Ω resistor
- LED strip `VCC` → external 5V supply (not the ESP32's 5V pin)
- 1000µF capacitor across LED `VCC`/`GND`
- Common ground across the PSU, LED strip, and ESP32

- [ ] **Step 2: Flash the full firmware**

Run: `pio run -e esp32-c6 -t upload -t monitor`

- [ ] **Step 3: Verify baseline visual behavior**

Confirm: LEDs light bottom-up in blue-to-red gradient as instance load varies, individual requests produce a visible flash, unplugging WiFi triggers the offline breathing pulse and recovers cleanly on reconnect.

- [ ] **Step 4: Check signal integrity without a level-shifter**

Look for flickering, wrong colors, or erratic pixels — signs the ESP32's 3.3V data signal isn't being reliably read by the 5V strip. If the display is clean and stable, no level-shifter is needed. If not, insert a 74AHCT125 or 74HCT245 between GPIO2 and the `DAT` line (as noted in the spec) and re-verify.

- [ ] **Step 5: Calibrate thresholds against real traffic**

Watch the bar over a period of typical usage. If it pins at full/red too easily or barely moves, adjust `RATE_MIN`/`RATE_MAX` in `src/config.h` to match the actual observed tokens/minute range, re-flash, and re-observe until the full 0-18 range is used across normal low/high load.

- [ ] **Step 6: Commit the calibrated thresholds**

```bash
git add src/config.h
git commit -m "chore: calibrate RATE_MIN/RATE_MAX against real instance traffic"
```
