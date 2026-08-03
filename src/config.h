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
// Calibrated 2026-07-30 against real instance traffic: busy-hour Live Tail
// showed ~20 requests/4min averaging ~100k prompt tokens => ~500k tokens/min
// peak. RATE_MAX=2000 (the original guess) pinned the bar at full red all day.
constexpr double RATE_MIN = 0.0;
constexpr double RATE_MAX = 500000.0;

constexpr double EMA_ALPHA = 0.3;
constexpr long RATE_WINDOW_SECONDS = 60;
