# Codebase Cleanup & Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove all dead code, fix log spam and magic numbers, and break large files into single-responsibility units across three sequential passes.

**Architecture:** Three-pass layered approach — delete, then hygiene, then refactor. Each pass ends with a clean compile. No new features introduced.

**Tech Stack:** ESP32-S3, PlatformIO, Arduino framework, LVGL 9.x, LovyanGFX 1.1.16, ArduinoJson 7.x

> **Note on TDD:** This is an embedded project with no test harness. "Verification" means `pio run -e full` compiles cleanly. Functional verification means flashing and observing serial output. Each task ends with a compile check.

---

## File Map

### Deleted
- `src/DisplayManager.cpp` — legacy display driver, dead code
- `src/DisplayManager.h` — legacy display driver header, dead code
- `src/display_smoke_test.cpp` — diagnostic harness, no longer needed

### Modified
- `platformio.ini` — remove `[env:smoke-test]` block
- `src/main.cpp` — remove `#ifndef SMOKE_TEST` guards
- `src/config/Config.h` — add `TICK_DELAY_MS`, `WIFI_RECONNECT_INTERVAL`, `Config::Display` namespace; remove `DEBUG_SERIAL` bool
- `src/core/App.h` — remove WiFi/serial members, add `WiFiManager` + `SerialCommandHandler` members
- `src/core/App.cpp` — remove WiFi/serial logic, shrink to orchestrator
- `src/LVGLDisplayManager.h` — add private builder method declarations
- `src/LVGLDisplayManager.cpp` — decompose `build_home_screen()` into private builders; replace magic numbers
- `src/services/OpenSkyService.cpp` — replace if/else chains with lookup tables; verify bounds checks

### Created
- `src/core/WiFiManager.h`
- `src/core/WiFiManager.cpp`
- `src/core/SerialCommandHandler.h`
- `src/core/SerialCommandHandler.cpp`

---

## PASS 1 — Delete Pass

---

### Task 1: Delete dead source files

**Files:**
- Delete: `src/DisplayManager.cpp`
- Delete: `src/DisplayManager.h`
- Delete: `src/display_smoke_test.cpp`

- [ ] **Step 1: Delete the files**

```bash
rm src/DisplayManager.cpp src/DisplayManager.h src/display_smoke_test.cpp
```

- [ ] **Step 2: Verify no live file references them**

```bash
grep -r "DisplayManager\b" src/ --include="*.cpp" --include="*.h"
grep -r "SMOKE_TEST\|smoke_test" src/ --include="*.cpp" --include="*.h"
```

Expected: zero matches (only `main.cpp` will show `SMOKE_TEST` — that's cleaned in Task 3).

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "chore: delete dead DisplayManager and smoke test files"
```

---

### Task 2: Clean platformio.ini

**Files:**
- Modify: `platformio.ini`

- [ ] **Step 1: Remove the smoke-test environment block**

Open `platformio.ini`. Delete everything from `[env:smoke-test]` to the end of the file (lines 79–90):

```ini
[env:smoke-test]
extends = env:full
build_flags = 
    -I src
    -DCORE_DEBUG_LEVEL=3
    -DCONFIG_ARDUHAL_LOG_COLORS=1
    -DBOARD_HAS_PSRAM
    -DCONFIG_SPIRAM_SUPPORT=1
    -DSMOKE_TEST
    ; DISABLE USB CDC for smoke-test so Serial goes to UART
    -DARDUINO_USB_CDC_ON_BOOT=0
monitor_speed = ${env:full.monitor_speed}
```

The file should end after the `[env:full]` `monitor_filters` block.

- [ ] **Step 2: Compile**

```bash
pio run -e full
```

Expected: compiles cleanly. The `SMOKE_TEST` guard in `main.cpp` will still be there but harmless — cleaned next task.

- [ ] **Step 3: Commit**

```bash
git add platformio.ini
git commit -m "chore: remove smoke-test build environment"
```

---

### Task 3: Clean main.cpp — remove SMOKE_TEST guards

**Files:**
- Modify: `src/main.cpp`

Current `src/main.cpp`:
```cpp
#include <Arduino.h>
#include "core/App.h"

#ifndef SMOKE_TEST

static core::App app;

void setup() {
    if (!app.begin()) {
        Serial.println("[FATAL] App failed to initialize");
        while (true) {
            delay(1000);
        }
    }
}

void loop() {
    app.tick();
}

#endif  // SMOKE_TEST
```

- [ ] **Step 1: Remove the SMOKE_TEST guards**

Replace the entire file with:

```cpp
#include <Arduino.h>
#include "core/App.h"

static core::App app;

void setup() {
    if (!app.begin()) {
        Serial.println("[FATAL] App failed to initialize");
        while (true) {
            delay(1000);
        }
    }
}

void loop() {
    app.tick();
}
```

- [ ] **Step 2: Compile**

```bash
pio run -e full
```

Expected: clean compile.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "chore: remove SMOKE_TEST preprocessor guards from main.cpp"
```

---

## PASS 2 — Hygiene Pass

---

### Task 4: Replace DEBUG_SERIAL bool with compile-time flag

**Files:**
- Modify: `src/config/Config.h`
- Modify: `platformio.ini`

The current `Config::DEBUG_SERIAL = true` is a runtime bool that is always on. Replace with a `#ifdef DEBUG_SERIAL` compile-time gate. In the live codebase (after deleting DisplayManager), there are no gesture prints left — this establishes the pattern for any future debug prints and removes the always-true bool.

- [ ] **Step 1: Remove DEBUG_SERIAL from Config.h**

In `src/config/Config.h`, find and remove these two lines in the Debug Settings section:

```cpp
    constexpr bool DEBUG_SERIAL = true;
    constexpr bool DEBUG_API_RESPONSES = false;
```

The entire `// Debug Settings` comment block can be removed since both constants are gone.

- [ ] **Step 2: Add DEBUG_SERIAL flag comment to platformio.ini**

In `platformio.ini`, add this comment block to the `build_flags` section of `[env:full]`, just before the Wi-Fi credentials block:

```ini
    ; Debug serial output — uncomment to enable verbose logging
    ; -DDEBUG_SERIAL
```

- [ ] **Step 3: Compile**

```bash
pio run -e full
```

Expected: clean compile. If anything referenced `Config::DEBUG_SERIAL`, the compiler will flag it — fix any such references by replacing `if (Config::DEBUG_SERIAL)` with `#ifdef DEBUG_SERIAL`.

- [ ] **Step 4: Commit**

```bash
git add src/config/Config.h platformio.ini
git commit -m "chore: replace DEBUG_SERIAL runtime bool with compile-time flag"
```

---

### Task 5: Fix WiFi connect dot-spam and add missing Config constants

**Files:**
- Modify: `src/config/Config.h`
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Add missing constants to Config.h**

In `src/config/Config.h`, add these after the `WIFI_TIMEOUT_MS` line and before the OpenWeather section:

```cpp
    constexpr uint32_t TICK_DELAY_MS          = 20;     // Main loop delay
    constexpr uint32_t WIFI_RECONNECT_INTERVAL = 30000; // ms between reconnect attempts
```

Then add a new `Display` sub-namespace at the end of the `Config` namespace, before the closing `}`:

```cpp
    // ========================================
    // Display Geometry
    // ========================================
    namespace Display {
        constexpr int WIDTH           = 800;
        constexpr int HEIGHT          = 480;
        constexpr int TAP_REGION_X_MIN = 720;
        constexpr int TAP_REGION_X_MAX = 800;
        constexpr int TAP_REGION_Y_MIN = 0;
        constexpr int TAP_REGION_Y_MAX = 80;
    }
```

- [ ] **Step 2: Fix the dot-spam in App::connectWiFi()**

In `src/core/App.cpp`, find `connectWiFi()`. Replace the dot-printing loop body:

**Before:**
```cpp
bool App::connectWiFi() {
    Serial.printf("[WiFi] Connecting to SSID: \"%s\"\n", Config::WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);

    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if ((millis() - started) > Config::WIFI_TIMEOUT_MS) {
            Serial.println("[WiFi] Connection timeout");
            health_.setStatusMessage("WiFi timeout");
            return false;
        }
        delay(300);
        Serial.print('.');
    }

    Serial.println();
    Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    health_.setStatusMessage("WiFi connected");
    return true;
}
```

**After:**
```cpp
bool App::connectWiFi() {
    Serial.printf("[WiFi] Connecting to SSID: \"%s\"\n", Config::WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);

    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if ((millis() - started) > Config::WIFI_TIMEOUT_MS) {
            Serial.println("[WiFi] Connection timeout");
            health_.setStatusMessage("WiFi timeout");
            return false;
        }
        delay(300);
    }

    Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    health_.setStatusMessage("WiFi connected");
    return true;
}
```

- [ ] **Step 3: Replace hardcoded delay(20) in App::tick()**

In `src/core/App.cpp`, at the bottom of `App::tick()`, replace:

```cpp
    delay(20);
```

With:

```cpp
    delay(Config::TICK_DELAY_MS);
```

- [ ] **Step 4: Replace hardcoded 30000 in the WiFi reconnect block in App::tick()**

In `src/core/App.cpp`, in the WiFi reconnect block, replace:

```cpp
        if ((now - lastWifiReconnectMs_) >= 30000) {
```

With:

```cpp
        if ((now - lastWifiReconnectMs_) >= Config::WIFI_RECONNECT_INTERVAL) {
```

- [ ] **Step 5: Compile**

```bash
pio run -e full
```

Expected: clean compile.

- [ ] **Step 6: Commit**

```bash
git add src/config/Config.h src/core/App.cpp
git commit -m "chore: add missing Config constants, fix WiFi dot-spam"
```

---

### Task 6: Add bounds checking to OpenSkyService JSON parsing

**Files:**
- Modify: `src/services/OpenSkyService.cpp`

The current code checks `state[5]` and `state[6]` for null before lat/lon, but indices 7–11 are accessed without null checks. Malformed API responses can crash the device.

- [ ] **Step 1: Harden the state parsing block**

In `src/services/OpenSkyService.cpp`, find the inner loop that populates `Aircraft`. Replace the block from `Aircraft &plane = aircraftList[aircraftCount];` through `aircraftCount++;` with:

```cpp
Aircraft &plane = aircraftList[aircraftCount];
plane.icao24    = state[0].isNull() ? "" : state[0].as<String>();
plane.callsign  = state[1].isNull() ? "" : state[1].as<String>();
plane.callsign.trim();
plane.longitude = state[5].as<float>();   // null-checked above
plane.latitude  = state[6].as<float>();   // null-checked above
plane.altitude  = state[7].isNull() ? 0.0f : state[7].as<float>();
plane.onGround  = state[8].isNull() ? false : state[8].as<bool>();
if (plane.onGround) continue;
plane.velocity  = state[9].isNull()  ? 0.0f : state[9].as<float>();
plane.heading   = state[10].isNull() ? 0.0f : state[10].as<float>();
plane.valid     = true;
plane.aircraftType = guessAircraftType(plane.callsign);
plane.airline      = guessAirline(plane.callsign);
if (plane.airline == "Private") continue;
plane.origin      = "";
plane.destination = "";
aircraftCount++;
```

- [ ] **Step 2: Compile**

```bash
pio run -e full
```

Expected: clean compile.

- [ ] **Step 3: Commit**

```bash
git add src/services/OpenSkyService.cpp
git commit -m "fix: add null guards to all OpenSky JSON field accesses"
```

---

## PASS 3 — Refactor Pass

---

### Task 7: Extract WiFiManager

**Files:**
- Create: `src/core/WiFiManager.h`
- Create: `src/core/WiFiManager.cpp`
- Modify: `src/core/App.h`
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Create WiFiManager.h**

```cpp
// src/core/WiFiManager.h
#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace core {

class WiFiManager {
public:
    WiFiManager();

    // Attempt initial connection + NTP sync. Returns true on success.
    bool connect();

    // Call from App::tick(). Handles reconnect every WIFI_RECONNECT_INTERVAL ms.
    // Calls onReconnect callback if reconnection succeeds.
    void tick(uint32_t nowMs);

    bool isConnected() const;
    String localIP() const;

    // One-shot flag: true for one tick() call after a successful reconnect.
    // Consumers should check this to re-init auth tokens etc.
    bool justReconnected();

private:
    uint32_t lastReconnectAttemptMs_;
    bool reconnectedFlag_;
};

}  // namespace core
```

- [ ] **Step 2: Create WiFiManager.cpp**

```cpp
// src/core/WiFiManager.cpp
#include "core/WiFiManager.h"
#include "config/Config.h"
#include <time.h>

namespace core {

WiFiManager::WiFiManager()
    : lastReconnectAttemptMs_(0)
    , reconnectedFlag_(false) {}

bool WiFiManager::connect() {
    Serial.printf("[WiFi] Connecting to SSID: \"%s\"\n", Config::WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);

    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if ((millis() - started) > Config::WIFI_TIMEOUT_MS) {
            Serial.println("[WiFi] Connection timeout");
            return false;
        }
        delay(300);
    }

    Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    configTime(Config::GMT_OFFSET_SEC, Config::DAYLIGHT_OFFSET_SEC, Config::NTP_SERVER);
    return true;
}

void WiFiManager::tick(uint32_t nowMs) {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    if ((nowMs - lastReconnectAttemptMs_) < Config::WIFI_RECONNECT_INTERVAL) {
        return;
    }

    lastReconnectAttemptMs_ = nowMs;
    Serial.println("[WiFi] Disconnected — attempting reconnect");

    if (connect()) {
        reconnectedFlag_ = true;
    }
}

bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::localIP() const {
    return WiFi.localIP().toString();
}

bool WiFiManager::justReconnected() {
    if (reconnectedFlag_) {
        reconnectedFlag_ = false;
        return true;
    }
    return false;
}

}  // namespace core
```

- [ ] **Step 3: Update App.h — swap members**

In `src/core/App.h`:

Add the include at the top:
```cpp
#include "core/WiFiManager.h"
```

Remove `<WiFi.h>` include (App no longer needs it directly).

In the private section, remove:
```cpp
    uint32_t lastWifiReconnectMs_;
```

Add:
```cpp
    WiFiManager wifiManager_;
```

- [ ] **Step 4: Update App.cpp — remove WiFi logic**

Remove `#include <WiFi.h>` and `#include <time.h>` from the top of `App.cpp` (both now live in WiFiManager).

Remove `lastWifiReconnectMs_(0)` from the constructor initializer list.

In `App::begin()`, replace:
```cpp
    display_->setStatusMessage("Connecting WiFi...");
    if (!connectWiFi()) {
        display_->setStatusMessage("WiFi failed - retrying...");
        Serial.println("[WiFi] Initial connection failed; will retry in tick()");
        // Don't halt — services will be skipped until WiFi is up
    } else {
        configTime(Config::GMT_OFFSET_SEC, Config::DAYLIGHT_OFFSET_SEC, Config::NTP_SERVER);
    }
```

With:
```cpp
    display_->setStatusMessage("Connecting WiFi...");
    if (!wifiManager_.connect()) {
        display_->setStatusMessage("WiFi failed - retrying...");
        Serial.println("[WiFi] Initial connection failed; will retry in tick()");
    }
```

In `App::tick()`, replace the entire WiFi reconnect block:
```cpp
    // Reconnect WiFi if dropped; retry at most every 30 seconds
    if (WiFi.status() != WL_CONNECTED) {
        if ((now - lastWifiReconnectMs_) >= Config::WIFI_RECONNECT_INTERVAL) {
            lastWifiReconnectMs_ = now;
            Serial.println("[WiFi] Disconnected — attempting reconnect");
            if (display_) display_->setStatusMessage("WiFi reconnecting...");
            if (connectWiFi()) {
                configTime(Config::GMT_OFFSET_SEC, Config::DAYLIGHT_OFFSET_SEC, Config::NTP_SERVER);
                if (openSkyService_) openSkyService_->initialize();
                if (display_) display_->setStatusMessage("WiFi reconnected");
            }
        }
    }
```

With:
```cpp
    wifiManager_.tick(now);
    if (wifiManager_.justReconnected()) {
        if (display_) display_->setStatusMessage("WiFi reconnected");
        if (openSkyService_) openSkyService_->initialize();
    }
```

Delete the entire `App::connectWiFi()` method definition from `App.cpp`.

Remove `bool connectWiFi();` from the private section of `App.h`.

- [ ] **Step 5: Compile**

```bash
pio run -e full
```

Expected: clean compile. If you see `WL_CONNECTED` or `WiFi.` references in `App.cpp`, those are missed replacements — fix them.

- [ ] **Step 6: Commit**

```bash
git add src/core/WiFiManager.h src/core/WiFiManager.cpp src/core/App.h src/core/App.cpp
git commit -m "refactor: extract WiFiManager, remove WiFi logic from App"
```

---

### Task 8: Extract SerialCommandHandler

**Files:**
- Create: `src/core/SerialCommandHandler.h`
- Create: `src/core/SerialCommandHandler.cpp`
- Modify: `src/core/App.h`
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Create SerialCommandHandler.h**

```cpp
// src/core/SerialCommandHandler.h
#pragma once

#include <Arduino.h>
#include "LVGLDisplayManager.h"

namespace core {

class SerialCommandHandler {
public:
    explicit SerialCommandHandler(LVGLDisplayManager* display);

    // Call from App::tick(). Drains Serial buffer and dispatches commands.
    // Also handles raw touch dump if that mode is active.
    void tick();

    bool rawTouchMode() const { return rawTouchMode_; }

private:
    LVGLDisplayManager* display_;
    String buffer_;
    bool rawTouchMode_;

    void dispatch(const String& command);
    void runI2CScan();
    void dumpRawTouch();
};

}  // namespace core
```

- [ ] **Step 2: Create SerialCommandHandler.cpp**

```cpp
// src/core/SerialCommandHandler.cpp
#include "core/SerialCommandHandler.h"
#include <Wire.h>

namespace core {

SerialCommandHandler::SerialCommandHandler(LVGLDisplayManager* display)
    : display_(display)
    , buffer_("")
    , rawTouchMode_(false) {}

void SerialCommandHandler::tick() {
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (!buffer_.isEmpty()) {
                buffer_.toUpperCase();
                dispatch(buffer_);
                buffer_ = "";
            }
            continue;
        }
        buffer_ += c;
        if (buffer_.length() > 64) {
            buffer_ = buffer_.substring(buffer_.length() - 64);
        }
    }

    if (rawTouchMode_) {
        dumpRawTouch();
    }
}

void SerialCommandHandler::dispatch(const String& command) {
    if (command == "RAW") {
        rawTouchMode_ = !rawTouchMode_;
        Serial.printf("[CMD] RAW mode %s\n", rawTouchMode_ ? "ON" : "OFF");
    } else if (command == "I2CSCAN") {
        runI2CScan();
    } else {
        Serial.printf("[CMD] Unknown command: %s\n", command.c_str());
    }
}

void SerialCommandHandler::runI2CScan() {
    Serial.println("[CMD] Running I2C scan");
    Wire.end();
    Wire.begin(19, 20);
    Wire.setClock(400000);
    int found = 0;
    for (uint8_t address = 1; address < 127; ++address) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C] 0x%02X\n", address);
            found++;
        }
    }
    Serial.printf("[I2C] Devices found: %d\n", found);
}

void SerialCommandHandler::dumpRawTouch() {
    if (!display_) return;
    auto* lcd = display_->getLCD();
    if (!lcd) return;
    int tx = 0, ty = 0;
    if (lcd->getTouch(&tx, &ty)) {
        Serial.printf("RAW_TOUCH: %d, %d\n", tx, ty);
    }
}

}  // namespace core
```

- [ ] **Step 3: Update App.h**

Add include:
```cpp
#include "core/SerialCommandHandler.h"
```

In the private members section, remove:
```cpp
    String serialBuffer_;
    bool rawTouchMode_;
```

Add:
```cpp
    SerialCommandHandler serial_;
```

Remove declarations:
```cpp
    void processSerialCommands();
    void processRawTouchDump();
```

- [ ] **Step 4: Update App.cpp**

In the constructor initializer list, remove:
```cpp
    , serialBuffer_("")
    , rawTouchMode_(false)
```

Add `serial_` to the initializer list. Because `SerialCommandHandler` takes a pointer, it must be initialized after `display_` is constructed. Add it at the end of the initializer list:
```cpp
    , serial_(nullptr)
```

Then in `App::begin()`, after `display_->initialize()` succeeds, initialize the handler:
```cpp
    serial_ = SerialCommandHandler(display_);
```

Wait — `serial_` is a value member, not a pointer. It's constructed with `nullptr` in the initializer list but we need to assign it after display is ready. Change the member type to `SerialCommandHandler` and construct it directly. To do this properly:

Change `App.h` member declaration to:
```cpp
    SerialCommandHandler serial_;
```

And in `App.cpp` constructor initializer list add:
```cpp
    , serial_(nullptr)
```

Then after `display_->initialize()` in `begin()`, add:
```cpp
    serial_ = SerialCommandHandler(display_);
```

In `App::tick()`, replace:
```cpp
    processSerialCommands();
    processRawTouchDump();
```

With:
```cpp
    serial_.tick();
```

Delete the `App::processSerialCommands()` and `App::processRawTouchDump()` method bodies from `App.cpp`.

Remove `<Wire.h>` include from `App.cpp` (now in SerialCommandHandler.cpp).

- [ ] **Step 5: Compile**

```bash
pio run -e full
```

Expected: clean compile.

- [ ] **Step 6: Commit**

```bash
git add src/core/SerialCommandHandler.h src/core/SerialCommandHandler.cpp src/core/App.h src/core/App.cpp
git commit -m "refactor: extract SerialCommandHandler, remove serial parsing from App"
```

---

### Task 9: Decompose build_home_screen()

**Files:**
- Modify: `src/LVGLDisplayManager.h`
- Modify: `src/LVGLDisplayManager.cpp`

`build_home_screen()` is currently 199 lines (lines 259–457). It builds three distinct sections: a top status bar, a weather card (with embedded forecast rows), and an aircraft summary card. Extract each into a private method.

- [ ] **Step 1: Add private builder declarations to LVGLDisplayManager.h**

In `src/LVGLDisplayManager.h`, in the private section after `void build_no_aircraft_screen();`, add:

```cpp
    // Home screen section builders (called by build_home_screen)
    void buildTopBar(lv_obj_t* screen);
    void buildWeatherCard(lv_obj_t* screen);
    void buildAircraftCard(lv_obj_t* screen);
```

- [ ] **Step 2: Extract buildTopBar()**

In `src/LVGLDisplayManager.cpp`, add this new method before `build_home_screen()`:

```cpp
void LVGLDisplayManager::buildTopBar(lv_obj_t* screen) {
    lv_obj_t* top_bar = lv_obj_create(screen);
    lv_obj_set_size(top_bar, hal::Elecrow5Inch::PANEL_WIDTH, 80);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);

    label_time = lv_label_create(top_bar);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(label_time, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_time, "00:00");
    lv_obj_align(label_time, LV_ALIGN_LEFT_MID, 20, -5);

    label_date = lv_label_create(top_bar);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_date, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_date, "Mon, Jan 1");
    lv_obj_align(label_date, LV_ALIGN_LEFT_MID, 20, 20);
}
```

- [ ] **Step 3: Extract buildWeatherCard()**

Add this method after `buildTopBar()`:

```cpp
void LVGLDisplayManager::buildWeatherCard(lv_obj_t* screen) {
    lv_obj_t* weather_card = lv_obj_create(screen);
    lv_obj_set_size(weather_card, 480, 320);
    lv_obj_align(weather_card, LV_ALIGN_TOP_LEFT, 20, 100);
    lv_obj_set_style_bg_color(weather_card, COLOR_CARD, 0);
    lv_obj_set_style_border_color(weather_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(weather_card, 2, 0);
    lv_obj_set_style_shadow_width(weather_card, 20, 0);
    lv_obj_set_style_shadow_color(weather_card, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(weather_card, LV_OPA_30, 0);
    lv_obj_set_style_radius(weather_card, 16, 0);
    lv_obj_set_style_pad_all(weather_card, 20, 0);

    // Current conditions — left column
    label_temperature = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_temperature, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_temperature, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_temperature, "--°F");
    lv_obj_align(label_temperature, LV_ALIGN_TOP_LEFT, 0, 0);

    label_weather_desc = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_weather_desc, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_weather_desc, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_weather_desc, "Loading...");
    lv_obj_align(label_weather_desc, LV_ALIGN_TOP_LEFT, 0, 60);

    label_feels_like = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_feels_like, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_feels_like, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_feels_like, "Feels: --°F");
    lv_obj_align(label_feels_like, LV_ALIGN_TOP_LEFT, 0, 85);

    label_temp_range = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_temp_range, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_temp_range, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_temp_range, "H: --° L: --°");
    lv_obj_align(label_temp_range, LV_ALIGN_TOP_LEFT, 0, 105);

    label_wind = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_wind, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_wind, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_wind, "Wind: -- mph");
    lv_obj_align(label_wind, LV_ALIGN_TOP_LEFT, 0, 135);

    label_sunrise = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_sunrise, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_sunrise, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_sunrise, LV_SYMBOL_UP " --:--");
    lv_obj_align(label_sunrise, LV_ALIGN_TOP_LEFT, 0, 155);

    label_sunset = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_sunset, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_sunset, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_sunset, LV_SYMBOL_DOWN " --:--");
    lv_obj_align(label_sunset, LV_ALIGN_TOP_LEFT, 0, 175);

    arc_humidity = lv_arc_create(weather_card);
    lv_obj_set_size(arc_humidity, 80, 80);
    lv_arc_set_range(arc_humidity, 0, 100);
    lv_arc_set_value(arc_humidity, 0);
    lv_obj_set_style_arc_color(arc_humidity, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_humidity, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_humidity, 8, LV_PART_MAIN);
    lv_obj_align(arc_humidity, LV_ALIGN_BOTTOM_LEFT, 10, 0);
    lv_obj_remove_flag(arc_humidity, LV_OBJ_FLAG_CLICKABLE);

    label_humidity = lv_label_create(arc_humidity);
    lv_obj_set_style_text_font(label_humidity, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_humidity, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_humidity, "--%");
    lv_obj_center(label_humidity);

    // 5-day forecast — right column
    int forecast_x = 200;
    int row_height  = 45;
    int start_y     = 10;

    lv_obj_t* forecast_title = lv_label_create(weather_card);
    lv_obj_set_style_text_font(forecast_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(forecast_title, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(forecast_title, "5-Day Forecast");
    lv_obj_align(forecast_title, LV_ALIGN_TOP_LEFT, forecast_x, -5);

    for (int i = 0; i < 5; i++) {
        int y_pos = start_y + 25 + (i * row_height);

        forecast_rows[i].container = lv_obj_create(weather_card);
        lv_obj_set_size(forecast_rows[i].container, 240, row_height);
        lv_obj_align(forecast_rows[i].container, LV_ALIGN_TOP_LEFT, forecast_x, y_pos);
        lv_obj_set_style_bg_opa(forecast_rows[i].container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(forecast_rows[i].container, 0, 0);
        lv_obj_set_style_pad_all(forecast_rows[i].container, 0, 0);

        forecast_rows[i].label_day = lv_label_create(forecast_rows[i].container);
        lv_obj_set_style_text_font(forecast_rows[i].label_day, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(forecast_rows[i].label_day, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(forecast_rows[i].label_day, "---");
        lv_obj_align(forecast_rows[i].label_day, LV_ALIGN_LEFT_MID, 0, 0);

        forecast_rows[i].label_condition = lv_label_create(forecast_rows[i].container);
        lv_obj_set_style_text_font(forecast_rows[i].label_condition, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(forecast_rows[i].label_condition, COLOR_TEXT_SECONDARY, 0);
        lv_label_set_text(forecast_rows[i].label_condition, "");
        lv_obj_align(forecast_rows[i].label_condition, LV_ALIGN_CENTER, -10, 0);

        forecast_rows[i].label_temp = lv_label_create(forecast_rows[i].container);
        lv_obj_set_style_text_font(forecast_rows[i].label_temp, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(forecast_rows[i].label_temp, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(forecast_rows[i].label_temp, "--/--");
        lv_obj_align(forecast_rows[i].label_temp, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}
```

- [ ] **Step 4: Extract buildAircraftCard()**

Add this method after `buildWeatherCard()`:

```cpp
void LVGLDisplayManager::buildAircraftCard(lv_obj_t* screen) {
    lv_obj_t* aircraft_card = lv_obj_create(screen);
    lv_obj_set_size(aircraft_card, 260, 320);
    lv_obj_align(aircraft_card, LV_ALIGN_TOP_RIGHT, -20, 100);
    lv_obj_set_style_bg_color(aircraft_card, COLOR_CARD, 0);
    lv_obj_set_style_border_color(aircraft_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(aircraft_card, 2, 0);
    lv_obj_set_style_shadow_width(aircraft_card, 20, 0);
    lv_obj_set_style_shadow_color(aircraft_card, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(aircraft_card, LV_OPA_30, 0);
    lv_obj_set_style_radius(aircraft_card, 16, 0);
    lv_obj_set_style_pad_all(aircraft_card, 20, 0);

    lv_obj_t* plane_icon = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(plane_icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(plane_icon, COLOR_ACCENT, 0);
    lv_label_set_text(plane_icon, LV_SYMBOL_GPS);
    lv_obj_align(plane_icon, LV_ALIGN_TOP_MID, 0, 20);

    label_plane_count = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_plane_count, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(label_plane_count, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_plane_count, "0");
    lv_obj_align(label_plane_count, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* label_planes_text = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_planes_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_planes_text, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_planes_text, "Aircraft Nearby");
    lv_obj_align(label_planes_text, LV_ALIGN_CENTER, 0, 35);

    btn_view_planes = lv_button_create(aircraft_card);
    lv_obj_set_size(btn_view_planes, 200, 50);
    lv_obj_align(btn_view_planes, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_view_planes, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(btn_view_planes, 12, 0);
    lv_obj_add_event_cb(btn_view_planes, event_btn_view_planes, LV_EVENT_CLICKED, this);

    lv_obj_t* btn_label = lv_label_create(btn_view_planes);
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0xffffff), 0);
    lv_label_set_text(btn_label, "View Aircraft");
    lv_obj_center(btn_label);
}
```

- [ ] **Step 5: Replace build_home_screen() body**

Replace the existing `build_home_screen()` body with:

```cpp
void LVGLDisplayManager::build_home_screen() {
    screen_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_home, COLOR_BG_TOP, 0);
    lv_obj_set_style_bg_grad_color(screen_home, COLOR_BG_BOTTOM, 0);
    lv_obj_set_style_bg_grad_dir(screen_home, LV_GRAD_DIR_VER, 0);

    buildTopBar(screen_home);
    buildWeatherCard(screen_home);
    buildAircraftCard(screen_home);
}
```

- [ ] **Step 6: Compile**

```bash
pio run -e full
```

Expected: clean compile. The visual output on device should be identical to before.

- [ ] **Step 7: Commit**

```bash
git add src/LVGLDisplayManager.h src/LVGLDisplayManager.cpp
git commit -m "refactor: decompose build_home_screen into buildTopBar/buildWeatherCard/buildAircraftCard"
```

---

### Task 10: Replace airline/aircraft if-else chains with lookup tables

**Files:**
- Modify: `src/services/OpenSkyService.cpp`

- [ ] **Step 1: Add lookup tables at the top of OpenSkyService.cpp**

After the anonymous namespace block (after the closing `}` on line 11), add:

```cpp
// Lookup tables for callsign-prefix → airline/type guessing
static const struct { const char* prefix; const char* airline; } kAirlineTable[] = {
    { "AAL", "American Airlines" },
    { "DAL", "Delta Air Lines"   },
    { "UAL", "United Airlines"   },
    { "SWA", "Southwest Airlines"},
    { "JBU", "JetBlue Airways"   },
    { "FDX", "FedEx"             },
    { "UPS", "UPS Airlines"      },
    { "ASA", "Alaska Airlines"   },
    { "FFT", "Frontier Airlines" },
    { "NKS", "Spirit Airlines"   },
};

static const struct { const char* prefix; const char* type; } kTypeTable[] = {
    { "AAL", "Commercial Jet" },
    { "DAL", "Commercial Jet" },
    { "UAL", "Commercial Jet" },
    { "SWA", "Commercial Jet" },
    { "JBU", "Commercial Jet" },
    { "FDX", "Cargo Aircraft" },
    { "UPS", "Cargo Aircraft" },
};
```

- [ ] **Step 2: Replace guessAirline() body**

```cpp
String OpenSkyService::guessAirline(const String& callsign) {
    if (callsign.startsWith("N")) return "Private";
    if (callsign.length() < 3) return "Unknown";

    String prefix = callsign.substring(0, 3);
    for (const auto& entry : kAirlineTable) {
        if (prefix == entry.prefix) return entry.airline;
    }
    return "Airline";
}
```

- [ ] **Step 3: Replace guessAircraftType() body**

```cpp
String OpenSkyService::guessAircraftType(const String& callsign) {
    if (callsign.startsWith("N")) return "Private Aircraft";
    if (callsign.length() < 3) return "Aircraft";

    String prefix = callsign.substring(0, 3);
    for (const auto& entry : kTypeTable) {
        if (prefix == entry.prefix) return entry.type;
    }
    return "Aircraft";
}
```

- [ ] **Step 4: Compile**

```bash
pio run -e full
```

Expected: clean compile. Behavior is identical — same lookups, no branching.

- [ ] **Step 5: Commit**

```bash
git add src/services/OpenSkyService.cpp
git commit -m "refactor: replace airline/type if-else chains with static lookup tables"
```

---

### Task 11: Final verification

- [ ] **Step 1: Full clean build**

```bash
pio run -e full 2>&1 | tail -20
```

Expected: `SUCCESS` with no warnings about unused variables or implicit conversions.

- [ ] **Step 2: Verify no dead references remain**

```bash
grep -r "DisplayManager\b\|SMOKE_TEST\|DEBUG_SERIAL\|display_smoke" src/ --include="*.cpp" --include="*.h"
```

Expected: zero matches.

- [ ] **Step 3: Verify App.cpp no longer imports WiFi or Wire directly**

```bash
grep -n "#include.*WiFi\|#include.*Wire" src/core/App.cpp
```

Expected: zero matches.

- [ ] **Step 4: Check file line counts**

```bash
wc -l src/core/App.cpp src/LVGLDisplayManager.cpp src/services/OpenSkyService.cpp
```

Expected targets:
- `App.cpp`: under 200 lines
- `LVGLDisplayManager.cpp`: under 550 lines
- `OpenSkyService.cpp`: under 220 lines

- [ ] **Step 5: Flash and observe serial output**

Flash the device and watch the serial monitor. Expected boot sequence:
```
=== ESP32 Plane Tracker v4 Rewrite ===
[WiFi] Connecting to SSID: "YourSSID"
[WiFi] Connected: 192.168.x.x
[OpenSky] Requesting OAuth2 token...
[OpenSky] ✅ Authenticated successfully
[INIT] v4 rewrite foundation ready
```

No dot-spam. No gesture debug output. Only meaningful status lines.

- [ ] **Step 6: Final commit**

```bash
git add -A
git commit -m "chore: final cleanup verification pass complete"
```
