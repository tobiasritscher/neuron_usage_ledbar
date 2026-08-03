# LiteLLM Logs Endpoint — Confirmed Schema (2026-07-29)

Endpoint: `GET /spend/logs/ui` (host: `neuron.noser.com`, no `/v1` prefix —
that prefix is only for the OpenAI-compatible inference routes, not the
admin/spend routes).

Query params used: `start_date`, `end_date` (both **full datetimes**,
format `YYYY-MM-DD HH:MM:SS`, URL-encoded — a bare date like `2026-07-29`
is rejected with a 400 `"Invalid date format"` error), `page`, `page_size`.

Example request:

```bash
curl -s -G "https://neuron.noser.com/spend/logs/ui" \
  -H "Authorization: Bearer ${LITELLM_KEY}" \
  --data-urlencode "start_date=$(date +%F) 00:00:00" \
  --data-urlencode "end_date=$(date +%F) 23:59:59" \
  --data-urlencode "page=1" \
  --data-urlencode "page_size=5"
```

Wrapper key: `"data"` (top-level object is `{data: [...], total, page,
page_size, total_pages}`, matches the assumed schema in the plan — no
change needed to `parseFlashEvents`'s `doc["data"].is<JsonArray>()` check).

Timestamp field: `startTime` — format `2026-07-29T14:51:50.002+00:00`
(ISO 8601 with milliseconds and a `+00:00` UTC offset suffix). The planned
`sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", ...)` parser only consumes the
`YYYY-MM-DDTHH:MM:SS` prefix and ignores the trailing `.002+00:00` —
confirmed to work unchanged against a real sample.

Confirmed fields per entry: `request_id` (string), `prompt_tokens` (int),
`completion_tokens` (int), `total_tokens` (int), `startTime` (string, see
above). Many additional fields exist (`spend`, `model`, `metadata`, etc.)
that we don't use.

Ordering: not independently re-verified with a live second sample in this
session (only one page was fetched), but this is the same endpoint
powering the UI's "Live Tail" newest-first view, so newest-first ordering
is assumed to hold. If Task 8's integration testing shows entries arriving
oldest-first instead, `filterNewerThan`'s "first element is newest" logic
in `main.cpp`'s `pollLiteLlm` must be swapped to use the *last* element
instead.

## Required change vs. the original plan

`config.h`'s `LITELLM_LOGS_PATH_TEMPLATE` must change from:

```cpp
constexpr const char* LITELLM_LOGS_PATH_TEMPLATE =
    "/spend/logs?start_date=%s&end_date=%s&summarize=false";
```

to:

```cpp
constexpr const char* LITELLM_LOGS_PATH_TEMPLATE =
    "/spend/logs/ui?start_date=%s+00:00:00&end_date=%s+23:59:59&page=1&page_size=20";
```

`+` in a query string is decoded as a literal space by FastAPI/Starlette
(standard form-encoding convention), so no other code needs to change —
`main.cpp`'s `todayDateString()` still produces a bare `YYYY-MM-DD` string
and gets substituted into both `%s` slots exactly as the original plan
had it; only this one constant in `config.h` differs. Applied directly in
Task 2 and Task 8 below instead of the plan's original endpoint guess.

## Postscript (2026-08-03)

`page_size` was later reduced from 20 to 8 (see `config.h`'s own comment
for why — office WiFi transfer time for the larger page tripped the HTTP
timeout). The newest-first ordering assumption above was confirmed working
during live Task 9 hardware testing on 2026-08-03: flashes visibly tracked
real requests in the correct order against actual proxy traffic.
