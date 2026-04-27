# Plane Tracker — Code Review Fix List

Generated: 2026-04-27. Based on deep diagnostics review of full codebase.

---

## CRITICAL (must fix)

### 1. RouteCache HTTPS calls silently fail
**File:** `src/services/RouteCache.cpp` ~lines 162, 222, 287
**Issue:** All three route backends (hexdb.io, adsbdb, AeroDataBox) call `http.begin(url)` without a `WiFiClientSecure` client. TLS cert verification fails silently — routes always blank.
**Fix:** Add `WiFiClientSecure client; client.setInsecure();` and use `http.begin(client, url)` — same pattern as `OpenSkyService`.
**Status:** ☐ Fixed

### 2. Blocking `delay()` up to 5s in `fetchAircraft()`
**File:** `src/services/OpenSkyService.cpp` ~lines 266–268
**Issue:** `delay(API_RATE_LIMIT - elapsed)` blocks the entire main loop — weather, web server, night-mode all frozen.
**Fix:** Replace with early-return guard: `if (millis() - lastApiCall < API_RATE_LIMIT) return 0;`
**Status:** ☐ Fixed

### 3. Blocking network calls in `App::begin()`
**File:** `src/core/App.cpp` ~lines 98–101
**Issue:** `updateWeather()` + `updateAircraft()` called synchronously at boot — startup can take 30–90s with slow/unreliable OpenSky.
**Fix:** Remove priming calls from `begin()`. Scheduler tasks fire on first `tick()` naturally.
**Status:** ☐ Fixed

---

## IMPORTANT (should fix)

### 4. Weather forecast off-by-one — 5th day always blank
**File:** `src/services/WeatherService.cpp` ~lines 145–206
**Issue:** Last accumulated day never pushed after loop ends.
**Fix:** After the loop, push the final `currentDay` into `weather.forecast` if not yet full.
**Status:** ☐ Fixed

### 5. Weather API key sent over HTTP
**File:** `src/services/WeatherService.cpp` ~lines 22–23
**Issue:** API key in plaintext URL over HTTP — visible on LAN and to ISP.
**Fix:** Change `buildUrl()` and `buildForecastUrl()` to use `https://`. Add `WiFiClientSecure` with `setInsecure()`.
**Status:** ☐ Fixed

### 6. ArduinoJson double-buffer wastes SRAM
**File:** `src/services/OpenSkyService.cpp` ~line 295; `src/services/WeatherService.cpp` ~line 43
**Issue:** `http.getString()` + `deserializeJson(doc, payload)` — both raw payload and parsed doc in SRAM simultaneously. Peak ~3× payload size (~60 KB for OpenSky).
**Fix:** Use `deserializeJson(doc, http.getStream())` to eliminate the intermediate `String`.
**Status:** ☐ Fixed

### 7. 13 unused Montserrat font sizes waste ~120–200 KB flash
**File:** `src/lv_conf.h` ~lines 59–79
**Issue:** Only sizes 12, 14, 16, 22, 28, 48 are used in `LVGLDisplayManager.cpp`. 13 others are enabled but unused. At 90%+ flash, every KB counts.
**Fix:** Disable all Montserrat sizes except: 12, 14, 16, 22, 28, 48.
**Status:** ☐ Fixed

### 8. `build_type = debug` in only build env inflates flash
**File:** `platformio.ini`
**Issue:** Debug build disables `-Os`, retains dead code, inflates flash ~50–100 KB vs release.
**Fix:** Add `[env:release]` with `build_type = release` and `CORE_DEBUG_LEVEL=0`.
**Status:** ☐ Fixed

### 9. `applyNightMode()` probes system time 50×/second
**File:** `src/core/App.cpp` ~lines 265–286
**Issue:** `getLocalTime()` called every 20ms tick. Only needs to run once/minute.
**Fix:** Add `static unsigned long lastNightCheck = 0;` gate — skip if `millis() - lastNightCheck < 60000`.
**Status:** ☐ Fixed

### 10. I2C scan destroys touch controller permanently
**File:** `src/core/SerialCommandHandler.cpp` ~lines 51–53
**Issue:** `Wire.end()` + `Wire.begin()` breaks GT911 touch until reboot. Comment claiming "display reinitializes" is incorrect.
**Fix:** After scan, reinitialize touch by calling `lcd->init()`, or update the comment to say a reboot is required.
**Status:** ☐ Fixed

### 11. Duplicate airline ICAO→IATA table in two files
**File:** `src/services/OpenSkyService.cpp` and `src/services/RouteCache.cpp`
**Issue:** Same ~100-entry mapping duplicated and must be kept in sync manually.
**Fix:** Move to `src/data/AirlineTable.h`, include from both files.
**Status:** ☐ Fixed

---

## MINOR (nice to have)

### 12. `WebDashboard::begin(port)` ignores its parameter
**File:** `src/web/WebDashboard.cpp` ~line 66
**Fix:** Use `port` in `WebServer` constructor or remove the parameter.
**Status:** ☐ Fixed

### 13. Dead lambda `getDayOfWeek` in `parseForecastData`
**File:** `src/services/WeatherService.cpp` ~lines 139–143
**Fix:** Delete the lambda — the function uses inline math instead.
**Status:** ☐ Fixed

### 14. Private-aircraft filter leaves ghost `valid=true` slot
**File:** `src/services/OpenSkyService.cpp` ~lines 322–332
**Fix:** Add `plane.valid = false;` before both `continue` statements.
**Status:** ☐ Fixed

### 15. Coastline arrays over-allocated
**File:** `src/LVGLDisplayManager.h` ~lines 105, 113
**Issue:** `[256]` elements allocated but only `COASTLINE_PORTLAND_LEN` (32) are used.
**Fix:** Change to `[GeoUtils::COASTLINE_PORTLAND_LEN + 1]`.
**Status:** ☐ Fixed

---

## DNS Issue — Root Cause & Fix

**Symptom:** After a failed TLS connection to `auth.opensky-network.org`, all subsequent DNS lookups fail.

**Root cause:** Failed TLS connection may leave a lwIP TCP PCB in `TIME_WAIT`, consuming a pool slot and corrupting DNS resolver state.

**Recommended fix (belt-and-suspenders):**
1. Pre-resolve all API hostnames right after WiFi connects, cache `IPAddress` values.
2. Use `client.connect(cachedIP, 443)` directly for known-problematic hosts — bypasses DNS resolver entirely while keeping TLS SNI correct.
3. Keep the existing Cloudflare DNS re-application every 30s as a guard.
4. Limit the pre-resolve loop to 1–2s timeout per host to avoid blocking connect for 32s+ on failure.

**Status:** ☐ Fixed
