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
// page_size reduced from 20 to 8 on 2026-08-03: a 20-entry page (~150KB JSON)
// took long enough over the office WiFi that even a 15s HTTP timeout
// sometimes tripped (HTTPC_ERROR_READ_TIMEOUT). 8 entries (~60KB) is a
// pragmatic tradeoff — fast/reliable transfer, at the cost of both missing
// individual flashes AND undercounting the displayed rate if more than 8
// requests land between two polls (~2.5s apart), since events that fall off
// the page are never observed and so never counted either way.
constexpr const char* LITELLM_LOGS_PATH_TEMPLATE =
    "/spend/logs/ui?start_date=%s+00:00:00&end_date=%s+23:59:59&page=1&page_size=8";

constexpr unsigned long POLL_INTERVAL_MS = 2500;
constexpr unsigned long RENDER_INTERVAL_MS = 25; // ~40fps
// Show the offline pulse only after this long without a successful poll, so
// brief network/TLS hiccups keep the last known bar instead of flashing white.
constexpr unsigned long OFFLINE_AFTER_MS = 30000;

// Bar height = spend in USD/min (rolling 60s window, EMA-smoothed), on a log
// scale from RATE_MIN to RATE_MAX. Switched from prompt_tokens on 29.09.2026:
// ~97% of prompt tokens are cache reads (agentic clients resend the whole
// context each turn), so token rate saturated the bar all day. Calibrated on
// that day's active minutes: median ~$0.23/min, p90 ~$0.71, p99 ~$3.1,
// max ~$5.3 -> median lands mid-bar, only real peaks reach the red top.
constexpr double RATE_MIN = 0.01;
constexpr double RATE_MAX = 5.0;

constexpr double EMA_ALPHA = 0.3;
constexpr long RATE_WINDOW_SECONDS = 60;
