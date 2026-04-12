# LVGL FreeRTOS Task Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move LVGL's timer handler onto a dedicated FreeRTOS task so touch remains responsive while the main loop blocks on HTTP network calls.

**Architecture:** An `esp_timer` fires every 1 ms to call `lv_tick_inc(1)`, keeping LVGL's internal clock accurate. A pinned FreeRTOS task calls `lv_timer_handler()` every 5 ms inside LVGL's built-in recursive mutex (`lv_lock`/`lv_unlock`). Every public `LVGLDisplayManager` method that writes LVGL state acquires the same lock before touching any LVGL objects.

**Tech Stack:** ESP32-S3, Arduino/ESP-IDF, LVGL 9.2.2, LovyanGFX 1.1.16, FreeRTOS (bundled with ESP32 Arduino core), GT911 capacitive touch over I2C

---

## Files Modified

| File | Change |
|------|--------|
| `src/lv_conf.h` | Add `LV_USE_OS`, tighten indev poll period |
| `src/LVGLDisplayManager.h` | New include, two new private members, one new private static method |
| `src/LVGLDisplayManager.cpp` | `initialize()`, `tick()`, `~LVGLDisplayManager()`, `update()`, `setScreen()`, `setStatusMessage()` |

---

## Task 1: Enable FreeRTOS OS support in LVGL config

**Files:**
- Modify: `src/lv_conf.h`

- [ ] **Step 1: Add `LV_USE_OS` and tighten indev period**

Open `src/lv_conf.h`. Find the **HAL SETTINGS** block (around line 46). Add `LV_USE_OS` immediately after the opening comment and change `LV_INDEV_DEF_READ_PERIOD`:

```c
/*====================
   HAL SETTINGS
 *====================*/

/* OS / threading support — enables lv_lock() / lv_unlock() backed by FreeRTOS recursive mutex */
#define LV_USE_OS LV_OS_FREERTOS

/* Default display refresh period in ms */
#define LV_DEF_REFR_PERIOD 30

/* Input device read period in milliseconds */
#define LV_INDEV_DEF_READ_PERIOD 10   /* was 30 — GT911 polled every 10 ms now */
```

- [ ] **Step 2: Verify the build still compiles**

```bash
cd /Users/michaelballin/Documents/PlatformIO/Projects/Plane-Tracker
pio run -e full 2>&1 | tail -20
```

Expected: `SUCCESS` with no errors. If LVGL complains about a missing FreeRTOS header, confirm `Arduino.h` is included in `LVGLDisplayManager.h` (it is — already present).

- [ ] **Step 3: Commit**

```bash
git add src/lv_conf.h
git commit -m "config: enable LV_OS_FREERTOS, tighten indev poll to 10ms"
```

---

## Task 2: Add members and method declaration to the header

**Files:**
- Modify: `src/LVGLDisplayManager.h`

- [ ] **Step 1: Add `#include <esp_timer.h>`**

At the top of `src/LVGLDisplayManager.h`, after the existing includes (after line 8 `#include "config/Config.h"`), add:

```cpp
#include <esp_timer.h>
```

- [ ] **Step 2: Add private members and static method**

In the `private:` section of `LVGLDisplayManager`, after the `// --- State ---` block (after `uint8_t currentBrightness;` around line 105), add:

```cpp
    // --- LVGL task / tick timer ---
    esp_timer_handle_t lvgl_tick_timer_ = nullptr;
    TaskHandle_t       lvgl_task_handle_ = nullptr;
    static void        lvgl_task(void* arg);
```

- [ ] **Step 3: Compile**

```bash
pio run -e full 2>&1 | tail -20
```

Expected: `SUCCESS`. The new members are just declarations — nothing will link differently yet.

- [ ] **Step 4: Commit**

```bash
git add src/LVGLDisplayManager.h
git commit -m "feat: declare lvgl_task and timer handles in LVGLDisplayManager"
```

---

## Task 3: Start the timer and task in `initialize()`

**Files:**
- Modify: `src/LVGLDisplayManager.cpp` — `initialize()` at line 187

- [ ] **Step 1: Add timer + task startup after `lv_init()`**

In `initialize()`, find this line (line 203):
```cpp
    // Initialize LVGL
    lv_init();
```

Replace it with:

```cpp
    // Initialize LVGL
    lv_init();

    // 1-ms hardware tick — accurate regardless of main-loop blocking
    {
        esp_timer_create_args_t args = {};
        args.callback = [](void*) { lv_tick_inc(1); };
        args.name = "lvgl_tick";
        args.dispatch_method = ESP_TIMER_TASK;
        esp_timer_create(&args, &lvgl_tick_timer_);
        esp_timer_start_periodic(lvgl_tick_timer_, 1000 /* µs = 1 ms */);
    }

    // LVGL handler task — polls touch and drives rendering every 5 ms
    xTaskCreatePinnedToCore(
        lvgl_task,          // task function
        "lvgl",             // name (visible in serial debug)
        8192,               // stack in bytes
        nullptr,            // arg (unused — task uses s_instance)
        2,                  // priority: above idle (0), below WiFi stack (~20)
        &lvgl_task_handle_, // handle for cleanup
        1                   // Core 1 — same core as Arduino loop()
    );
    Serial.println("[LVGL] FreeRTOS task + tick timer started");
```

- [ ] **Step 2: Add the `lvgl_task` static method body**

After the `touchpad_read` function (after line 269), add:

```cpp
// LVGL handler task — runs independently of main loop
void LVGLDisplayManager::lvgl_task(void* /*arg*/) {
    while (true) {
        lv_lock();
        lv_timer_handler();
        lv_unlock();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

- [ ] **Step 3: Compile**

```bash
pio run -e full 2>&1 | tail -30
```

Expected: `SUCCESS`. At this point the timer and task are started but `tick()` still also calls `lv_tick_inc` + `lv_timer_handler` — that's OK for one commit; Task 4 removes those.

- [ ] **Step 4: Commit**

```bash
git add src/LVGLDisplayManager.cpp
git commit -m "feat: start lvgl_task and esp_timer tick in initialize()"
```

---

## Task 4: Make `tick()` a no-op

**Files:**
- Modify: `src/LVGLDisplayManager.cpp` — `tick()` at line 1220

- [ ] **Step 1: Replace the tick body**

Find (lines 1219–1223):
```cpp
// Tick function - call frequently in loop()
void LVGLDisplayManager::tick(uint32_t period_ms) {
    lv_tick_inc(period_ms);
    lv_timer_handler();
}
```

Replace with:
```cpp
// Tick is a no-op — the dedicated lvgl_task owns lv_timer_handler now.
void LVGLDisplayManager::tick(uint32_t /*period_ms*/) {}
```

- [ ] **Step 2: Compile**

```bash
pio run -e full 2>&1 | tail -20
```

Expected: `SUCCESS`.

- [ ] **Step 3: Commit**

```bash
git add src/LVGLDisplayManager.cpp
git commit -m "refactor: tick() is no-op, lvgl_task owns lv_timer_handler"
```

---

## Task 5: Lock LVGL writes in public methods

Three methods call into LVGL from `App::tick()` (outside the LVGL task): `update()`, `setScreen()`, and `setStatusMessage()`. Each needs `lv_lock()` / `lv_unlock()`.

`setBrightness()` only calls LGFX (not LVGL), and `setLastUpdateTimestamp()` only sets a plain C++ member — neither touches LVGL objects, so neither needs the lock.

**Files:**
- Modify: `src/LVGLDisplayManager.cpp` — `update()` line 1211, `setScreen()` line 1226, `setStatusMessage()` line 1298

- [ ] **Step 1: Wrap `update()`**

Find (lines 1211–1217):
```cpp
void LVGLDisplayManager::update(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount) {
    if (currentScreen == SCREEN_HOME) {
        update_home_screen(weather, aircraft, aircraftCount);
    } else if (currentScreen == SCREEN_AIRCRAFT_DETAIL && aircraft && aircraft->valid) {
        update_aircraft_screen(*aircraft);
    }
}
```

Replace with:
```cpp
void LVGLDisplayManager::update(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount) {
    lv_lock();
    if (currentScreen == SCREEN_HOME) {
        update_home_screen(weather, aircraft, aircraftCount);
    } else if (currentScreen == SCREEN_AIRCRAFT_DETAIL && aircraft && aircraft->valid) {
        update_aircraft_screen(*aircraft);
    }
    lv_unlock();
}
```

- [ ] **Step 2: Wrap `setScreen()`**

Find (lines 1226–1243):
```cpp
void LVGLDisplayManager::setScreen(ScreenState screen) {
    ScreenState target = (screen == SCREEN_NO_AIRCRAFT) ? SCREEN_HOME : screen;
    if (currentScreen == target) return;

    currentScreen = target;
    lastScreenChange = millis();

    switch (target) {
        case SCREEN_HOME:
            lv_screen_load(homeHasAircraft ? screen_home : screen_home_empty);
            break;
        case SCREEN_AIRCRAFT_DETAIL:
            lv_screen_load(screen_aircraft);
            break;
        default:
            break;
    }
}
```

Replace with:
```cpp
void LVGLDisplayManager::setScreen(ScreenState screen) {
    ScreenState target = (screen == SCREEN_NO_AIRCRAFT) ? SCREEN_HOME : screen;
    lv_lock();
    if (currentScreen == target) {
        lv_unlock();
        return;
    }

    currentScreen = target;
    lastScreenChange = millis();

    switch (target) {
        case SCREEN_HOME:
            lv_screen_load(homeHasAircraft ? screen_home : screen_home_empty);
            break;
        case SCREEN_AIRCRAFT_DETAIL:
            lv_screen_load(screen_aircraft);
            break;
        default:
            break;
    }
    lv_unlock();
}
```

> Note: `event_btn_view_planes` and `event_btn_back_home` also call `setScreen()`, but they're invoked from inside `lv_timer_handler()` which already holds the lock. `lv_lock()` uses a **recursive** mutex (`xSemaphoreTakeRecursive`), so the re-entrant call is safe — the mutex count increments then decrements without deadlock.

- [ ] **Step 3: Wrap `setStatusMessage()`**

Find (lines 1298–1304):
```cpp
void LVGLDisplayManager::setStatusMessage(const String& msg) {
    statusMessage = msg;
    statusClearTime = millis() + Config::UI_STATUS_MS;
    if (homeWidgets.label_status_left)  lv_label_set_text(homeWidgets.label_status_left,  msg.c_str());
    if (emptyWidgets.label_status_left) lv_label_set_text(emptyWidgets.label_status_left, msg.c_str());
    if (label_status_aircraft)          lv_label_set_text(label_status_aircraft,           msg.c_str());
}
```

Replace with:
```cpp
void LVGLDisplayManager::setStatusMessage(const String& msg) {
    lv_lock();
    statusMessage = msg;
    statusClearTime = millis() + Config::UI_STATUS_MS;
    if (homeWidgets.label_status_left)  lv_label_set_text(homeWidgets.label_status_left,  msg.c_str());
    if (emptyWidgets.label_status_left) lv_label_set_text(emptyWidgets.label_status_left, msg.c_str());
    if (label_status_aircraft)          lv_label_set_text(label_status_aircraft,           msg.c_str());
    lv_unlock();
}
```

- [ ] **Step 4: Compile**

```bash
pio run -e full 2>&1 | tail -20
```

Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add src/LVGLDisplayManager.cpp
git commit -m "feat: lock LVGL writes in update/setScreen/setStatusMessage"
```

---

## Task 6: Clean up timer and task in destructor

**Files:**
- Modify: `src/LVGLDisplayManager.cpp` — `~LVGLDisplayManager()` at line 178

- [ ] **Step 1: Replace the destructor**

Find (lines 177–184):
```cpp
// Destructor
LVGLDisplayManager::~LVGLDisplayManager() {
    if (lcd) {
        delete lcd;
        lcd = nullptr;
    }
    s_instance = nullptr;
}
```

Replace with:
```cpp
// Destructor
LVGLDisplayManager::~LVGLDisplayManager() {
    if (lvgl_task_handle_) {
        vTaskDelete(lvgl_task_handle_);
        lvgl_task_handle_ = nullptr;
    }
    if (lvgl_tick_timer_) {
        esp_timer_stop(lvgl_tick_timer_);
        esp_timer_delete(lvgl_tick_timer_);
        lvgl_tick_timer_ = nullptr;
    }
    delete lcd;
    lcd = nullptr;
    s_instance = nullptr;
}
```

- [ ] **Step 2: Compile**

```bash
pio run -e full 2>&1 | tail -20
```

Expected: `SUCCESS`.

- [ ] **Step 3: Commit**

```bash
git add src/LVGLDisplayManager.cpp
git commit -m "feat: clean up lvgl task and tick timer in destructor"
```

---

## Task 7: Flash, verify, and validate touch responsiveness

- [ ] **Step 1: Flash the firmware**

```bash
pio run -e full -t upload 2>&1 | tail -20
```

Expected: `SUCCESS` upload, board resets.

- [ ] **Step 2: Open serial monitor and check boot log**

```bash
pio device monitor
```

Expected output during boot:
```
[LVGL] Initializing LVGL display manager...
[LVGL] FreeRTOS task + tick timer started
[LVGL] Display initialized successfully
```

If you see `[LVGL] FreeRTOS task + tick timer started`, the task is up.

- [ ] **Step 3: Validate touch during a network stall**

Wait for the app to start polling OpenSky (watch for `[OpenSky]` log lines). While the
HTTP fetch is in progress — typically 1–5 seconds — tap the **"View Aircraft"** button
repeatedly. It should respond on the first or second tap, not require 5–10 attempts.

Before this fix: button was unresponsive during HTTP calls.
After this fix: LVGL task polls GT911 every 5 ms independent of HTTP, so touch should
register within ~10 ms of lifting your finger.

- [ ] **Step 4: Final commit**

```bash
git add -p   # review any unstaged changes
git commit -m "feat: LVGL FreeRTOS task — touch responsive during HTTP calls

Moves lv_timer_handler() to a dedicated task pinned to Core 1.
esp_timer fires every 1ms for lv_tick_inc. lv_lock/lv_unlock added
to all public LVGL-writing methods.

Fixes: View Aircraft button unresponsive during OpenSky HTTP fetches."
```
