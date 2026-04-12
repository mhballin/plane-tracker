# LVGL FreeRTOS Task — Design Spec
_2026-04-12_

## Problem

`App::tick()` calls `lv_timer_handler()` once per loop iteration. `OpenSkyService` and
`WeatherService` use blocking `HTTPClient` calls with up to **12-second timeouts** and
**500 ms retry delays** (`delay(RETRY_DELAY_MS * attempt)`). During those windows
`lv_timer_handler()` — and therefore the GT911 touch poll — never runs.

Symptoms: "View Aircraft" button rarely responds (must tap many times), and touch is
occasionally laggy. Both are explained by touch being dead for multi-second stretches
whenever a network request is in flight.

## Goal

Move LVGL's timer handler onto a dedicated FreeRTOS task so that touch and rendering
remain fully responsive regardless of what the main loop is doing.

## Scope

Changes are confined to `src/lv_conf.h` and `src/LVGLDisplayManager.{h,cpp}`.
No changes to the service layer, App scheduling, or main loop structure.

---

## Changes

### 1. `src/lv_conf.h`

Add in the **HAL SETTINGS** section:

```c
#define LV_USE_OS LV_OS_FREERTOS
```

This enables LVGL's built-in FreeRTOS mutex, which backs `lv_lock()` / `lv_unlock()`.

Also tighten the indev poll rate:

```c
#define LV_INDEV_DEF_READ_PERIOD 10   /* was 30 */
```

### 2. `src/LVGLDisplayManager.h`

Add two private members to `LVGLDisplayManager`:

```cpp
esp_timer_handle_t lvgl_tick_timer_ = nullptr;
TaskHandle_t       lvgl_task_handle_ = nullptr;
```

Add one private static method declaration:

```cpp
static void lvgl_task(void* arg);
```

Add `#include <esp_timer.h>` at the top of `LVGLDisplayManager.h`. FreeRTOS headers
(`freertos/FreeRTOS.h`, `freertos/task.h`) are already available via `Arduino.h` on
the ESP32 Arduino framework.

### 3. `src/LVGLDisplayManager.cpp`

#### `initialize()` — after `lv_init()`

Replace the existing `lv_tick_inc` approach with a hardware timer:

```cpp
// 1ms hardware tick — accurate regardless of main-loop blocking
esp_timer_create_args_t tick_args = {};
tick_args.callback = [](void*) { lv_tick_inc(1); };
tick_args.name = "lvgl_tick";
esp_timer_create(&tick_args, &lvgl_tick_timer_);
esp_timer_start_periodic(lvgl_tick_timer_, 1000 /* µs */);

// LVGL handler task — polls touch and drives rendering every 5 ms
xTaskCreatePinnedToCore(
    lvgl_task,
    "lvgl",
    8192,        // stack bytes
    nullptr,
    2,           // priority (above idle, below WiFi)
    &lvgl_task_handle_,
    1            // Core 1 — same as Arduino loop
);
```

#### New static method `lvgl_task()`

```cpp
void LVGLDisplayManager::lvgl_task(void* /*arg*/) {
    while (true) {
        lv_lock();
        lv_timer_handler();
        lv_unlock();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

#### `tick()` — becomes a no-op

Remove the `lv_tick_inc` and `lv_timer_handler` calls. Keep the method signature so
`App::tick()` doesn't need to change.

```cpp
void LVGLDisplayManager::tick(uint32_t /*period_ms*/) {
    // LVGL is now driven by the dedicated lvgl_task; nothing to do here.
}
```

#### All public methods that write LVGL state

Wrap the body of each of these with `lv_lock()` / `lv_unlock()`:

| Method | Reason |
|--------|--------|
| `update()` | Updates all widget labels |
| `setScreen()` | Loads a new screen object |
| `setStatusMessage()` | Writes to label widgets |
| `setBrightness()` | Calls LGFX (safe outside lock, but consistent) |
| `setLastUpdateTimestamp()` | Writes internal state read during update |

Pattern:

```cpp
void LVGLDisplayManager::setStatusMessage(const String& msg) {
    lv_lock();
    // ... existing body ...
    lv_unlock();
}
```

#### Destructor — clean up

```cpp
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

---

## Thread Safety

- `lv_lock()` / `lv_unlock()` use the recursive mutex LVGL creates when
  `LV_USE_OS = LV_OS_FREERTOS`. Any code that calls into LVGL from outside the
  `lvgl_task` must hold this lock.
- LVGL callbacks (`flush_cb`, `touchpad_read`, button event handlers) are called
  *from within* `lv_timer_handler()` which already holds the lock — no extra
  locking needed inside those callbacks.
- `App::tick()` calls `display_->update()`, `setScreen()`, etc. — these are
  covered by the per-method lock wrappers above.

## What Does NOT Change

- GT911 touch config (I2C address, pins, x/y range)
- Display rotation (`PANEL_ROTATION = 2`)
- Screen builder functions (`build_home_screen`, etc.)
- Service layer (OpenSkyService, WeatherService)
- App task scheduling and main loop structure
