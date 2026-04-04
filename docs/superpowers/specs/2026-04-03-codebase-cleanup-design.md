# Codebase Cleanup & Refactor Design
**Date:** 2026-04-03
**Project:** ESP32 Plane Tracker v4

## Overview

Full cleanup of the Plane Tracker codebase in three layered passes. Each pass compiles and runs before the next begins. Goal: remove all dead code, eliminate log noise, enforce single responsibility across all files.

---

## Pass 1 — Delete Pass

Remove all dead code. Nothing in the repo should exist unless it is referenced by the live app.

### Files to delete
- `src/DisplayManager.cpp` — 837 lines, only used by smoke test
- `src/DisplayManager.h` — 267 lines, only used by smoke test
- `src/display_smoke_test.cpp` — 251 lines, diagnostic harness no longer needed

### platformio.ini changes
- Remove the entire `[env:smoke-test]` block
- Remove any flags that only existed to support the smoke-test env (e.g. `-DSMOKE_TEST`)

### main.cpp changes
- Remove `#ifndef SMOKE_TEST` / `#endif` guards — `setup()` and `loop()` become plain top-level functions

### Success criteria
- Project compiles cleanly with `pio run -e full`
- No references to `DisplayManager`, `SMOKE_TEST`, or `display_smoke_test` remain in any live file

---

## Pass 2 — Hygiene Pass

Fix logging, magic numbers, and fragile code. No structural changes.

### 2.1 Compile-time debug logging

Replace `Config::DEBUG_SERIAL = true` (a runtime bool that is always on) with a compile-time `#ifdef DEBUG_SERIAL` gate.

- Add `-DDEBUG_SERIAL` to `platformio.ini` under a comment marking it as opt-in debug
- Default: flag is **absent** in `[env:full]` (silent in production)
- All gesture and touch debug prints wrap in `#ifdef DEBUG_SERIAL ... #endif`
- Non-debug logs (errors, WiFi status, boot messages) remain unconditional

### 2.2 WiFi connect log cleanup

In `App::connectWiFi()`:
- Remove `Serial.print('.')` dot-spam (can fire 60+ times during 20s timeout)
- Replace with: one `Serial.println("[WiFi] Connecting to SSID: ...")` at start, one result line on success or timeout

### 2.3 Config constants — add missing values

Add to `Config.h` (new `Display` sub-namespace and missing top-level constants):

```cpp
namespace Config {
    // Existing constants remain unchanged

    constexpr uint32_t TICK_DELAY_MS         = 20;
    constexpr uint32_t WIFI_RECONNECT_INTERVAL = 30000;

    namespace Display {
        constexpr int WIDTH  = 800;
        constexpr int HEIGHT = 480;
        // Tap region for aircraft detail button (top-right corner)
        constexpr int TAP_REGION_X_MIN = 720;
        constexpr int TAP_REGION_X_MAX = 800;
        constexpr int TAP_REGION_Y_MIN = 0;
        constexpr int TAP_REGION_Y_MAX = 80;
    }
}
```

Replace all hardcoded occurrences in `LVGLDisplayManager.cpp` and `App.cpp`.

### 2.4 Bounds checking in OpenSkyService

In `OpenSkyService::fetchAircraft()`, add `isNull()` guards before every `.as<>()` cast on JSON array indices. Malformed or partial state vectors from the API must be skipped, not dereferenced.

Pattern:
```cpp
if (state[5].isNull() || state[6].isNull()) continue;
float lat = state[6].as<float>();  // safe after null check
```

Apply to: latitude (index 6), longitude (index 5), altitude (index 7), velocity (index 9), heading (index 10), vertical rate (index 11).

### 2.5 Minor inline fixes
- `delay(20)` in `App::tick()` → `delay(Config::TICK_DELAY_MS)`

### Success criteria
- Serial output during a normal boot+run is clean: only boot, WiFi connect result, and periodic status messages
- No gesture/touch prints appear unless `-DDEBUG_SERIAL` is set
- All magic numbers replaced with named constants
- JSON parsing does not crash on malformed API responses

---

## Pass 3 — Refactor Pass

Break up large files and enforce single responsibility. No new features.

### 3.1 Extract `WiFiManager` (`src/core/WiFiManager.h/.cpp`)

**Responsibility:** Own all WiFi logic. `App` should not import `<WiFi.h>`.

**Interface:**
```cpp
class WiFiManager {
public:
    bool connect();                  // Initial connect + NTP sync on success
    void tick(uint32_t nowMs);       // Reconnect check (every WIFI_RECONNECT_INTERVAL)
    bool isConnected() const;
    String localIP() const;
};
```

**Moves from App.cpp:**
- `connectWiFi()` → `WiFiManager::connect()`
- Reconnect block in `App::tick()` → `WiFiManager::tick()`
- `configTime(...)` call → inside `WiFiManager::connect()` on success

`App` holds a `WiFiManager wifiManager_` member. `begin()` calls `wifiManager_.connect()`. `tick()` calls `wifiManager_.tick(now)`. OpenSky re-init after reconnect: `App` checks `wifiManager_.justReconnected()` (a one-shot flag) and calls `openSkyService_->initialize()` if true.

### 3.2 Extract `SerialCommandHandler` (`src/core/SerialCommandHandler.h/.cpp`)

**Responsibility:** Parse and dispatch serial debug commands. `App` should not contain string parsing logic.

**Interface:**
```cpp
class SerialCommandHandler {
public:
    explicit SerialCommandHandler(LVGLDisplayManager* display);
    void tick();                     // Drain Serial, dispatch commands
    bool rawTouchMode() const;
};
```

**Moves from App.cpp:**
- `processSerialCommands()` → `SerialCommandHandler::tick()`
- `processRawTouchDump()` → inside `SerialCommandHandler::tick()` (checks its own `rawTouchMode_`)
- `serialBuffer_` and `rawTouchMode_` members → move to `SerialCommandHandler`

`App::tick()` calls `serial_.tick()`. `App` no longer holds `serialBuffer_` or `rawTouchMode_`.

### 3.3 Break up `LVGLDisplayManager::build_home_screen()`

Currently ~200 lines. Extract private builder methods, each under ~60 lines:

```cpp
private:
    void buildStatusBar(lv_obj_t* parent);
    void buildWeatherCard(lv_obj_t* parent);
    void buildAircraftCard(lv_obj_t* parent);
    void buildForecastRow(lv_obj_t* parent);
```

`build_home_screen()` becomes a short orchestrator:
```cpp
void LVGLDisplayManager::build_home_screen() {
    lv_obj_t* screen = screens_[SCREEN_HOME];
    buildStatusBar(screen);
    buildWeatherCard(screen);
    buildAircraftCard(screen);
    buildForecastRow(screen);
}
```

Apply the same decomposition to any other screen builder functions over ~60 lines.

### 3.4 Airline/aircraft lookup → static data tables

Replace `if/else` chains in `OpenSkyService::guessAirline()` and `guessAircraftType()` with static lookup tables:

```cpp
static const struct { const char* prefix; const char* name; } kAirlines[] = {
    { "AAL", "American Airlines" },
    { "DAL", "Delta Air Lines" },
    { "UAL", "United Airlines" },
    // ...
};
```

Logic becomes a linear search. Same behavior, trivially extensible, no branching mess.

### 3.5 Consolidate HAL config into `ElecrowDisplayProfile.h`

`LVGLDisplayManager.cpp` currently embeds the full pin configuration (RGB bus, panel, touch controller). Move this into `src/hal/ElecrowDisplayProfile.h` as a self-contained LGFX config struct.

`LVGLDisplayManager.cpp` replaces its inline config with:
```cpp
#include "hal/ElecrowDisplayProfile.h"
// Use ElecrowDisplayProfile::LGFXConfig directly
```

This is the single source of truth for all display hardware pin assignments.

### File size targets

| File | Before | After |
|---|---|---|
| `App.cpp` | 365 lines | ~180 lines |
| `LVGLDisplayManager.cpp` | 827 lines | ~500 lines |
| `OpenSkyService.cpp` | 246 lines | ~200 lines |

### Success criteria
- `App.cpp` does not import `<WiFi.h>` or contain string parsing
- `WiFiManager` and `SerialCommandHandler` each have a single clear responsibility
- `build_home_screen()` is under 30 lines
- No airline/aircraft lookup uses if/else chains
- `ElecrowDisplayProfile.h` is the only place GPIO pins are defined
- All files compile cleanly, app runs correctly end-to-end

---

## Out of Scope

- New features of any kind
- Changing the LVGL widget hierarchy or visual design
- Unit tests (separate effort)
- OTA update support
- Any changes to `WeatherService` or `WebDashboard` beyond pass 2 fixes

---

## File Inventory After All Passes

### Deleted
- `src/DisplayManager.cpp`
- `src/DisplayManager.h`
- `src/display_smoke_test.cpp`

### Modified
- `platformio.ini` — remove smoke-test env
- `src/main.cpp` — remove SMOKE_TEST guards
- `src/config/Config.h` — add Display namespace + missing constants
- `src/core/App.h/.cpp` — remove WiFi/serial logic, shrink to orchestrator
- `src/LVGLDisplayManager.cpp` — decompose screen builders, use HAL profile
- `src/services/OpenSkyService.cpp` — bounds checking, data table lookup
- `src/hal/ElecrowDisplayProfile.h` — receive moved HAL config

### Created
- `src/core/WiFiManager.h`
- `src/core/WiFiManager.cpp`
- `src/core/SerialCommandHandler.h`
- `src/core/SerialCommandHandler.cpp`
