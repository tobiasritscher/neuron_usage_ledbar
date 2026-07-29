# Neuron Usage LED Bar — Design

## Purpose

A physical LED bar that visualizes real-time LLM usage on the company's LiteLLM
proxy instance (`https://neuron.noser.com/v1`), across all users. More request
(prompt) tokens sent per minute → more of the bar lights up, shifting from
blue (low load) to red (high load). Individual requests trigger a short flash
on top of the baseline level, giving a music-visualizer feel rather than a
slow-moving gauge.

## Hardware

- LED bar: 18 addressable LEDs (WS2812B/SK6812-compatible, adressable pixel
  protocol, confirmed via the original BLE controller's `VCC/DAT/GND` pinout).
- Controller: ESP32-C6 Pico Mini (WiFi + BLE capable), replacing the original
  BLE controller entirely.
- Wiring (per prior investigation):
  - `GND` → ESP32 `GND`
  - `DAT` → ESP32 `GPIO2`, through a 330Ω resistor
  - `VCC` → external 5V supply (not the ESP32's onboard 5V pin — the strip can
    draw multiple amps)
  - Common ground required between PSU, LED strip, and ESP32.
  - 1000µF capacitor across LED `VCC`/`GND`.
  - Level shifting (74AHCT125/74HCT245) recommended but not confirmed
    required — verify signal integrity during bring-up; the ESP32 outputs
    3.3V logic, the strip likely expects 5V data.

## Architecture

Standalone ESP32-C6 firmware (Arduino framework via PlatformIO). No
always-on companion computer required after flashing. PlatformIO is used
instead of the Arduino IDE for dependency pinning and native (host-side) unit
tests of the pure logic.

## Data source

LiteLLM proxy admin API, authenticated with `Authorization: Bearer <admin
API key>`. Uses the same paginated request-log endpoint that backs the
LiteLLM UI's "Request Logs / Live Tail" view (first page, sorted
newest-first, scoped to today). Exact query parameters (page size, date
filter, sort param names) are to be confirmed by inspecting the real
request/response once the API key exists — this is an implementation-time
verification step, not an open design question.

Each log entry exposes at least: `request_id`, `time`, `prompt_tokens`,
`completion_tokens`. Only `prompt_tokens` count toward the "tokens per
minute" metric — completion tokens are excluded.

## Data flow

1. **Boot**: connect to WiFi. Perform one throwaway poll to snapshot the
   newest `request_id`/`time` at startup as the "already seen" baseline, so
   historic requests don't all flash at once.
2. **Poll loop** (every ~2-3s): HTTPS GET the logs endpoint (first page,
   today). Parse with ArduinoJson.
3. For every entry newer than the last-seen marker (by `time`, tie-broken by
   `request_id`): treat it as one **flash event**, with magnitude derived
   from its `prompt_tokens`.
4. Maintain a 60-second sliding window of `(time, prompt_tokens)` pairs
   from observed flash events. Sum the window → raw tokens/minute rate.
5. EMA-smooth the raw rate to avoid single-request spikes wildly kicking the
   baseline level.
6. Map the smoothed rate to:
   - **LED count** (0-18, lit bottom-up) via configurable `RATE_MIN` /
     `RATE_MAX` thresholds (calibrated empirically after first real-world
     observation — no reliable a priori number for "typical" instance load).
   - **Baseline color**: blue (low) → red (high) gradient across the same
     thresholds.
7. **Render loop** at ~30-50fps, decoupled from the ~2-3s poll rate:
   - Baseline bar: steady N LEDs lit in the current gradient color.
   - Flash overlay: each active flash event contributes a brief brightness
     boost (fast attack, slower exponential decay — VU-meter peak style),
     layered on top of the baseline. Flash size/brightness scales with the
     event's `prompt_tokens`.

## Error handling

- **WiFi drop**: auto-reconnect in the background. While disconnected, the
  bar shows a slow "offline" breathing pulse (dim white) instead of freezing
  on stale data.
- **HTTP failure/timeout**: skip that poll cycle, keep last known baseline
  state. After N consecutive failures, fall back to the offline indicator.
- **JSON parse failure**: skip the frame, do not crash; retry next poll.
- **TLS**: use `WiFiClientSecure`. Certificate validation approach (pinned
  root CA vs. `setInsecure()`) to be decided during implementation as a
  pragmatic tradeoff for a hobby/internal project — flagged here so it isn't
  silently decided in code.

## Testing

- **Unit-testable pure logic** (extracted as free functions, no hardware
  dependency), run in PlatformIO's native environment:
  - JSON response → list of flash events
  - Sliding-window rate calculation
  - EMA smoothing
  - Rate → LED count / color gradient mapping
- **Manual hardware/integration checks**:
  - WiFi connects, first poll produces no flashes (baseline snapshot works)
  - Low load → few LEDs, blue
  - High load → most/all LEDs, red
  - Individual request produces a visible flash scaled to its token count
  - Pulling WiFi triggers the offline breathing pulse, recovers cleanly on
    reconnect

## Open items for implementation time (not blocking design approval)

- Confirm exact LiteLLM logs endpoint path/params and today's-date filter
  behavior once the API key is generated.
- Calibrate `RATE_MIN`/`RATE_MAX` against real instance traffic.
- Decide TLS certificate validation approach.
- Confirm level-shifter is/isn't needed once the strip is wired to the
  ESP32-C6.
