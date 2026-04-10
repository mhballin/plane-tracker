# Aviation Dashboard UI Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the light card UI with a dark aviation dashboard (Flightradar24 aesthetic): dark navy, cyan accent, amber warnings, radar panel on the home screen when aircraft present, full-width weather when none, and AeroDataBox route lookup on the detail screen.

**Architecture:** Screen-by-screen rebuild. `GeoUtils.h` provides haversine math (distance, bearing, blip coords). `RouteCache` handles NVS-backed AeroDataBox lookups. Two pre-built home screens (`screen_home` with radar, `screen_home_empty` full-width weather) switch on aircraft count. `WeatherWidgets` struct deduplicates weather label pointers across both home variants.

**Tech Stack:** LVGL 9.2.2, LovyanGFX, ESP32-S3, Arduino/PlatformIO, ArduinoJson 7, ESP32 Preferences (NVS), HTTPClient.

---

### Task 1: Color palette + Aircraft model + IATA table + Config

**Files:**
- Modify: `src/LVGLDisplayManager.cpp:113-123`
- Modify: `src/models/Aircraft.h`
- Modify: `src/services/OpenSkyService.cpp:14-38` and `~:166-186`
- Modify: `src/config/Config.h`

- [ ] **Step 1: Replace color defines in `src/LVGLDisplayManager.cpp`**

Replace lines 113–123 (the old `COLOR_BG_TOP` through `COLOR_TEXT_ON_ACCENT` block):

```cpp
// Aviation dashboard dark theme
#define COLOR_BG             lv_color_hex(0x0e1726)
#define COLOR_TOPBAR         lv_color_hex(0x101f33)
#define COLOR_PANEL          lv_color_hex(0x162033)
#define COLOR_INSET          lv_color_hex(0x0a1428)
#define COLOR_STATUSBAR      lv_color_hex(0x060e1a)
#define COLOR_ACCENT         lv_color_hex(0x00d4ff)
#define COLOR_AMBER          lv_color_hex(0xf59e0b)
#define COLOR_SUCCESS        lv_color_hex(0x22c55e)
#define COLOR_TEXT_PRIMARY   lv_color_hex(0xeaf6ff)
#define COLOR_TEXT_SECONDARY lv_color_hex(0x5a8aaa)
#define COLOR_TEXT_DIM       lv_color_hex(0x2a5070)
#define COLOR_BORDER         lv_color_hex(0x1e3a54)
#define COLOR_BORDER_ACCENT  lv_color_hex(0x004466)
#define COLOR_TEXT_ON_ACCENT lv_color_hex(0x060e1a)
```

- [ ] **Step 2: Add `verticalRate` and `squawk` to `src/models/Aircraft.h`**

```cpp
class Aircraft {
public:
    String icao24;
    String callsign;
    float latitude;
    float longitude;
    float altitude;
    float velocity;
    float heading;
    float verticalRate;   // m/s, + = climbing
    String squawk;        // e.g. "1200"
    String aircraftType;
    String airline;
    String origin;
    String destination;
    bool onGround;
    bool valid;

    Aircraft() :
        latitude(0), longitude(0), altitude(0),
        velocity(0), heading(0), verticalRate(0),
        onGround(false), valid(false) {}
};
```

- [ ] **Step 3: Add IATA codes to `kAirlineTable` in `src/services/OpenSkyService.cpp`**

Replace the existing `kAirlineTable` (lines 14–25):

```cpp
static const struct {
    const char* prefix;
    const char* airline;
    const char* iataCode;  // 2-letter IATA for AeroDataBox lookup
} kAirlineTable[] = {
    { "AAL", "American Airlines",  "AA" },
    { "DAL", "Delta Air Lines",    "DL" },
    { "UAL", "United Airlines",    "UA" },
    { "SWA", "Southwest Airlines", "WN" },
    { "JBU", "JetBlue Airways",    "B6" },
    { "FDX", "FedEx",              "FX" },
    { "UPS", "UPS Airlines",       "5X" },
    { "ASA", "Alaska Airlines",    "AS" },
    { "FFT", "Frontier Airlines",  "F9" },
    { "NKS", "Spirit Airlines",    "NK" },
};
```

- [ ] **Step 4: Parse `verticalRate` and `squawk` in `fetchAircraft()` and add `getIataFlightNumber()`**

In `fetchAircraft()`, after `plane.heading = state[10]...` (around line 180), add:

```cpp
plane.verticalRate = state[11].isNull() ? 0.0f : state[11].as<float>();
plane.squawk       = state[14].isNull() ? "" : state[14].as<String>();
```

Add to `OpenSkyService.h` public section:
```cpp
String getIataFlightNumber(const String& callsign);
```

Add to `OpenSkyService.cpp` at the bottom:
```cpp
String OpenSkyService::getIataFlightNumber(const String& callsign) {
    if (callsign.length() < 4) return "";
    String prefix = callsign.substring(0, 3);
    for (const auto& entry : kAirlineTable) {
        if (prefix == entry.prefix) {
            return String(entry.iataCode) + callsign.substring(3);
        }
    }
    return "";
}
```

Also update `guessAirline()` — the struct field access `entry.airline` still works because the struct now has an extra field at the end; no change needed there.

- [ ] **Step 5: Add `AERODATABOX_API_KEY` and `RADAR_MAX_RANGE_NM` to `src/config/Config.h`**

After the `HOME_LON` constexpr:
```cpp
// AeroDataBox Route Lookup
constexpr char AERODATABOX_API_KEY[] = AERODATABOX_API_KEY_MACRO;

// Radar display settings
constexpr float RADAR_MAX_RANGE_NM = 150.0f;
```

- [ ] **Step 6: Build and verify no errors**

```bash
pio run -e full 2>&1 | tail -20
```

Expected: `[SUCCESS]` — or only pre-existing warnings, no new errors.

- [ ] **Step 7: Commit**

```bash
git add src/models/Aircraft.h src/services/OpenSkyService.cpp src/services/OpenSkyService.h src/LVGLDisplayManager.cpp src/config/Config.h
git commit -m "feat: dark color palette, Aircraft verticalRate/squawk, IATA table, config keys"
```

---

### Task 2: GeoUtils.h + native unit tests

**Files:**
- Create: `src/utils/GeoUtils.h`
- Create: `test/test_geoutils/test_geoutils.cpp`
- Modify: `platformio.ini`

- [ ] **Step 1: Add native test environment to `platformio.ini`**

Append to `platformio.ini`:

```ini
[env:native]
platform = native
test_framework = unity
build_flags =
    -I src
    -D NATIVE_BUILD
```

- [ ] **Step 2: Write the failing test file**

Create `test/test_geoutils/test_geoutils.cpp`:

```cpp
#include <unity.h>
#include "utils/GeoUtils.h"

void setUp() {}
void tearDown() {}

void test_distance_portland_to_boston() {
    // Portland ME → Boston MA ≈ 93 nm
    float d = GeoUtils::distanceNm(43.6591f, -70.2568f, 42.3601f, -71.0589f);
    TEST_ASSERT_FLOAT_WITHIN(3.0f, 93.0f, d);
}

void test_distance_zero() {
    float d = GeoUtils::distanceNm(43.0f, -70.0f, 43.0f, -70.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, d);
}

void test_bearing_east() {
    // (0,0) → (0,1) = 90° east
    float b = GeoUtils::bearingDeg(0.0f, 0.0f, 0.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 90.0f, b);
}

void test_bearing_north() {
    float b = GeoUtils::bearingDeg(0.0f, 0.0f, 1.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 0.0f, b);
}

void test_bearing_south() {
    float b = GeoUtils::bearingDeg(1.0f, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 180.0f, b);
}

void test_cardinal_N() {
    TEST_ASSERT_EQUAL_STRING("N", GeoUtils::cardinalDir(0.0f));
    TEST_ASSERT_EQUAL_STRING("N", GeoUtils::cardinalDir(359.0f));
    TEST_ASSERT_EQUAL_STRING("N", GeoUtils::cardinalDir(22.0f));
}

void test_cardinal_NE() {
    TEST_ASSERT_EQUAL_STRING("NE", GeoUtils::cardinalDir(47.0f));
}

void test_cardinal_E() {
    TEST_ASSERT_EQUAL_STRING("E", GeoUtils::cardinalDir(90.0f));
}

void test_blip_at_center() {
    // 0 distance → blip at center (95, 95)
    auto pos = GeoUtils::blipPosition(0.0f, 0.0f, 150.0f, 95, 6);
    TEST_ASSERT_EQUAL(95, pos.x);
    TEST_ASSERT_EQUAL(95, pos.y);
}

void test_blip_north_max_range() {
    // Due north, exactly max range, no margin → top-center (95, 0)
    auto pos = GeoUtils::blipPosition(150.0f, 0.0f, 150.0f, 95, 0);
    TEST_ASSERT_EQUAL(95, pos.x);
    TEST_ASSERT_INT_WITHIN(1, 0, pos.y);
}

void test_blip_east_half_range() {
    // Due east, half range → right-center (95 + 0.5*95, 95) ≈ (142, 95)
    auto pos = GeoUtils::blipPosition(75.0f, 90.0f, 150.0f, 95, 0);
    TEST_ASSERT_INT_WITHIN(2, 142, pos.x);
    TEST_ASSERT_INT_WITHIN(2, 95,  pos.y);
}

void test_blip_clamped_at_max_range() {
    // Beyond max range → clamped to radius - margin
    auto pos1 = GeoUtils::blipPosition(150.0f, 0.0f, 150.0f, 95, 6);
    auto pos2 = GeoUtils::blipPosition(300.0f, 0.0f, 150.0f, 95, 6);
    TEST_ASSERT_EQUAL(pos1.x, pos2.x);
    TEST_ASSERT_EQUAL(pos1.y, pos2.y);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_distance_portland_to_boston);
    RUN_TEST(test_distance_zero);
    RUN_TEST(test_bearing_east);
    RUN_TEST(test_bearing_north);
    RUN_TEST(test_bearing_south);
    RUN_TEST(test_cardinal_N);
    RUN_TEST(test_cardinal_NE);
    RUN_TEST(test_cardinal_E);
    RUN_TEST(test_blip_at_center);
    RUN_TEST(test_blip_north_max_range);
    RUN_TEST(test_blip_east_half_range);
    RUN_TEST(test_blip_clamped_at_max_range);
    return UNITY_END();
}
```

- [ ] **Step 3: Run tests — expect failures (file doesn't exist yet)**

```bash
pio test -e native 2>&1 | tail -15
```

Expected: compile error — `utils/GeoUtils.h: No such file`

- [ ] **Step 4: Create `src/utils/GeoUtils.h`**

```cpp
// src/utils/GeoUtils.h
// Pure-math geo utilities — no Arduino dependency, testable on native.
#pragma once
#include <cmath>
#include <cstdint>

namespace GeoUtils {

constexpr float EARTH_RADIUS_NM = 3440.065f;
constexpr float DEG_TO_RAD = static_cast<float>(M_PI) / 180.0f;

/// Haversine great-circle distance in nautical miles.
inline float distanceNm(float lat1, float lon1, float lat2, float lon2) {
    float dlat = (lat2 - lat1) * DEG_TO_RAD;
    float dlon = (lon2 - lon1) * DEG_TO_RAD;
    float a = sinf(dlat / 2) * sinf(dlat / 2)
            + cosf(lat1 * DEG_TO_RAD) * cosf(lat2 * DEG_TO_RAD)
            * sinf(dlon / 2) * sinf(dlon / 2);
    return 2.0f * EARTH_RADIUS_NM * asinf(sqrtf(a));
}

/// Forward bearing in degrees [0, 360).
inline float bearingDeg(float lat1, float lon1, float lat2, float lon2) {
    float dlon = (lon2 - lon1) * DEG_TO_RAD;
    float y = sinf(dlon) * cosf(lat2 * DEG_TO_RAD);
    float x = cosf(lat1 * DEG_TO_RAD) * sinf(lat2 * DEG_TO_RAD)
            - sinf(lat1 * DEG_TO_RAD) * cosf(lat2 * DEG_TO_RAD) * cosf(dlon);
    float b = atan2f(y, x) * (180.0f / static_cast<float>(M_PI));
    return fmodf(b + 360.0f, 360.0f);
}

/// 8-point cardinal direction string from a bearing in degrees.
inline const char* cardinalDir(float deg) {
    static const char* dirs[8] = { "N","NE","E","SE","S","SW","W","NW" };
    int sector = static_cast<int>((deg + 22.5f) / 45.0f) % 8;
    return dirs[sector];
}

/// Pixel position of an aircraft blip on the radar circle.
/// circleRadius: half of circle width/height (e.g. 95 for a 190px circle).
/// blipMargin: keeps blip inside circle edge (default 6px for an 8px blip).
/// Returns pixel (x, y) relative to top-left of the radar circle container,
/// where (circleRadius, circleRadius) is the center.
struct BlipPos { int16_t x; int16_t y; };

inline BlipPos blipPosition(float distNm, float bearingDegVal,
                             float maxRangeNm, int16_t circleRadius,
                             int16_t blipMargin = 6) {
    float scale = (distNm < maxRangeNm) ? (distNm / maxRangeNm) : 1.0f;
    float r = scale * static_cast<float>(circleRadius - blipMargin);
    float rad = bearingDegVal * DEG_TO_RAD;
    return {
        static_cast<int16_t>(circleRadius + static_cast<int16_t>(r * sinf(rad))),
        static_cast<int16_t>(circleRadius - static_cast<int16_t>(r * cosf(rad)))
    };
}

} // namespace GeoUtils
```

- [ ] **Step 5: Run tests — expect all pass**

```bash
pio test -e native 2>&1 | tail -20
```

Expected:
```
test/test_geoutils/test_geoutils.cpp:XX:test_distance_portland_to_boston  PASSED
...
-----------------------
12 Tests 0 Failures 0 Ignored
OK
```

- [ ] **Step 6: Commit**

```bash
git add src/utils/GeoUtils.h test/test_geoutils/test_geoutils.cpp platformio.ini
git commit -m "feat: add GeoUtils (haversine, bearing, blip) with native unit tests"
```

---

### Task 3: LVGLDisplayManager.h — restructure for new UI

**Files:**
- Modify: `src/LVGLDisplayManager.h`

- [ ] **Step 1: Replace `src/LVGLDisplayManager.h` with new structure**

```cpp
// src/LVGLDisplayManager.h
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "models/Aircraft.h"
#include "models/WeatherData.h"
#include "config/Config.h"

class LGFX_Panel;

class LVGLDisplayManager {
public:
    LVGLDisplayManager();
    ~LVGLDisplayManager();

    bool initialize();
    void update(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount);
    void tick(uint32_t period_ms = 5);
    void setBrightness(uint8_t brightness);
    void setLastUpdateTimestamp(time_t timestamp);
    void setStatusMessage(const String& msg);

    enum ScreenState {
        SCREEN_HOME,
        SCREEN_AIRCRAFT_DETAIL,
        SCREEN_NO_AIRCRAFT  // treated as SCREEN_HOME internally
    };

    void setScreen(ScreenState screen);
    ScreenState getCurrentScreen() const { return currentScreen; }

    lgfx::LGFX_Device* getDisplay();
    lgfx::LGFX_Device* getLCD();
    unsigned long getLastScreenChangeTime() const { return lastScreenChange; }
    bool shouldReturnToHome();
    void processTouch();

private:
    LGFX_Panel* lcd;
    lv_display_t* lv_display;
    lv_indev_t*   lv_indev;

    // --- Screens ---
    lv_obj_t* screen_home;        // home with radar panel (aircraft present)
    lv_obj_t* screen_home_empty;  // home full-width weather (no aircraft)
    lv_obj_t* screen_aircraft;    // aircraft detail

    // --- Shared weather widget set (one per home screen variant) ---
    struct WeatherWidgets {
        lv_obj_t* label_time         = nullptr;
        lv_obj_t* label_date         = nullptr;
        lv_obj_t* label_temperature  = nullptr;
        lv_obj_t* label_weather_desc = nullptr;
        lv_obj_t* label_feels_like   = nullptr;
        lv_obj_t* label_temp_range   = nullptr;
        lv_obj_t* label_wind         = nullptr;
        lv_obj_t* label_humidity_val = nullptr;
        lv_obj_t* label_sunrise      = nullptr;
        lv_obj_t* label_sunset       = nullptr;
        lv_obj_t* label_status_left  = nullptr;  // "OPENSKY OK · Last: HH:MM"
        lv_obj_t* label_status_live  = nullptr;  // "● LIVE" / "● IDLE"
        struct ForecastRow {
            lv_obj_t* container  = nullptr;
            lv_obj_t* label_day  = nullptr;
            lv_obj_t* label_cond = nullptr;
            lv_obj_t* label_hi   = nullptr;
            lv_obj_t* label_lo   = nullptr;
        } forecast[5];
    };
    WeatherWidgets homeWidgets;   // aircraft-present screen
    WeatherWidgets emptyWidgets;  // no-aircraft screen

    // --- Radar panel widgets (screen_home only) ---
    lv_obj_t* radar_container;
    lv_obj_t* radar_blips[Config::MAX_AIRCRAFT];
    lv_obj_t* label_contact_count;
    lv_obj_t* btn_view_planes;

    // --- Aircraft detail screen widgets ---
    lv_obj_t* label_callsign;
    lv_obj_t* label_distance;       // "42.3 nm · 047° NE"
    lv_obj_t* label_airline;
    lv_obj_t* label_aircraft_type;
    lv_obj_t* label_squawk;
    lv_obj_t* label_route_main;     // "BOS → LAX" large
    lv_obj_t* label_route_sub;      // "Boston · Los Angeles"
    lv_obj_t* label_altitude;
    lv_obj_t* label_velocity;
    lv_obj_t* label_heading;
    lv_obj_t* label_vert_speed;     // color-coded fpm
    lv_obj_t* label_status_aircraft;
    lv_obj_t* btn_back_home;

    // --- State ---
    ScreenState currentScreen;
    bool homeHasAircraft;
    unsigned long lastScreenChange;
    unsigned long lastUserInteraction;
    String statusMessage;
    uint32_t statusClearTime;
    time_t lastUpdateTime;
    uint8_t currentBrightness;

    // --- LVGL callbacks ---
    static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);
    static void touchpad_read(lv_indev_t* indev, lv_indev_data_t* data);
    static void event_btn_view_planes(lv_event_t* e);
    static void event_btn_back_home(lv_event_t* e);

    // --- Screen builders ---
    void build_home_screen();
    void build_home_empty_screen();
    void build_aircraft_screen();
    void build_no_aircraft_screen();  // backward-compat: calls build_home_empty_screen()

    // --- Section builders ---
    void buildTopBar(lv_obj_t* screen, WeatherWidgets& w);
    void buildStatusBar(lv_obj_t* screen, WeatherWidgets& w);
    void buildWeatherPanel(lv_obj_t* parent, WeatherWidgets& w);
    void buildRadarPanel(lv_obj_t* parent);

    // --- Update functions ---
    void updateWeatherWidgets(WeatherWidgets& w, const WeatherData& weather, int aircraftCount);
    void update_home_screen(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount);
    void update_aircraft_screen(const Aircraft& aircraft);
    void update_clock(WeatherWidgets& w);

    // --- Utilities ---
    String formatTime(time_t timestamp);
    String formatDate(time_t timestamp);
    const char* getWeatherIcon(const String& condition);
};
```

- [ ] **Step 2: Build to check header compiles (will fail on .cpp mismatches — that's expected)**

```bash
pio run -e full 2>&1 | grep -E "error:|warning:" | head -30
```

Expected: linker/compile errors about missing methods in .cpp — that's fine; we're just validating the header syntax.

- [ ] **Step 3: Commit**

```bash
git add src/LVGLDisplayManager.h
git commit -m "refactor: restructure LVGLDisplayManager.h for dark aviation UI"
```

---

### Task 4: Update constructor + buildTopBar() + buildStatusBar()

**Files:**
- Modify: `src/LVGLDisplayManager.cpp`

- [ ] **Step 1: Update constructor member initializer list**

Replace the constructor body (lines 126–167) with:

```cpp
LVGLDisplayManager::LVGLDisplayManager()
    : lcd(nullptr)
    , lv_display(nullptr)
    , lv_indev(nullptr)
    , screen_home(nullptr)
    , screen_home_empty(nullptr)
    , screen_aircraft(nullptr)
    , radar_container(nullptr)
    , label_contact_count(nullptr)
    , btn_view_planes(nullptr)
    , label_callsign(nullptr)
    , label_distance(nullptr)
    , label_airline(nullptr)
    , label_aircraft_type(nullptr)
    , label_squawk(nullptr)
    , label_route_main(nullptr)
    , label_route_sub(nullptr)
    , label_altitude(nullptr)
    , label_velocity(nullptr)
    , label_heading(nullptr)
    , label_vert_speed(nullptr)
    , label_status_aircraft(nullptr)
    , btn_back_home(nullptr)
    , currentScreen(SCREEN_HOME)
    , homeHasAircraft(false)
    , lastScreenChange(0)
    , lastUserInteraction(0)
    , statusMessage("")
    , statusClearTime(0)
    , lastUpdateTime(0)
    , currentBrightness(255)
{
    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        radar_blips[i] = nullptr;
    }
    s_instance = this;
}
```

- [ ] **Step 2: Update `initialize()` to build both home screens**

Replace the "Build screens" block in `initialize()` (around lines 216–219):

```cpp
    // Build screens
    build_home_screen();
    build_home_empty_screen();
    build_aircraft_screen();

    // Load appropriate home screen
    lv_screen_load(screen_home_empty);
    currentScreen = SCREEN_HOME;
    homeHasAircraft = false;
    lastUserInteraction = millis();
```

- [ ] **Step 3: Rewrite `buildTopBar()`**

Replace the entire `buildTopBar()` function:

```cpp
void LVGLDisplayManager::buildTopBar(lv_obj_t* screen, WeatherWidgets& w) {
    lv_obj_t* bar = lv_obj_create(screen);
    lv_obj_set_size(bar, 800, 58);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, COLOR_TOPBAR, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(bar, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    // Time — cyan, 28px bold, left side
    w.label_time = lv_label_create(bar);
    lv_obj_set_style_text_font(w.label_time, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(w.label_time, COLOR_ACCENT, 0);
    lv_label_set_text(w.label_time, "00:00");
    lv_obj_align(w.label_time, LV_ALIGN_LEFT_MID, 20, -8);

    // Date — secondary, 12px, below time
    w.label_date = lv_label_create(bar);
    lv_obj_set_style_text_font(w.label_date, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(w.label_date, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(w.label_date, "Mon, Jan 1");
    lv_obj_align(w.label_date, LV_ALIGN_LEFT_MID, 20, 14);

    // Location — dim, right side
    lv_obj_t* lbl_loc = lv_label_create(bar);
    lv_obj_set_style_text_font(lbl_loc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_loc, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_loc, Config::WEATHER_CITY);
    lv_obj_align(lbl_loc, LV_ALIGN_RIGHT_MID, -20, 0);
}
```

- [ ] **Step 4: Add `buildStatusBar()`**

Add this new function after `buildTopBar()`:

```cpp
void LVGLDisplayManager::buildStatusBar(lv_obj_t* screen, WeatherWidgets& w) {
    lv_obj_t* bar = lv_obj_create(screen);
    lv_obj_set_size(bar, 800, 26);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, COLOR_STATUSBAR, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(bar, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, 12, 0);
    lv_obj_set_style_pad_ver(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    w.label_status_left = lv_label_create(bar);
    lv_obj_set_style_text_font(w.label_status_left, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(w.label_status_left, COLOR_TEXT_DIM, 0);
    lv_label_set_text(w.label_status_left, "INITIALIZING");
    lv_obj_align(w.label_status_left, LV_ALIGN_LEFT_MID, 0, 0);

    w.label_status_live = lv_label_create(bar);
    lv_obj_set_style_text_font(w.label_status_live, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(w.label_status_live, COLOR_TEXT_DIM, 0);
    lv_label_set_text(w.label_status_live, "● IDLE");
    lv_obj_align(w.label_status_live, LV_ALIGN_RIGHT_MID, 0, 0);
}
```

- [ ] **Step 5: Build check**

```bash
pio run -e full 2>&1 | grep "error:" | head -20
```

Expected: errors about old method signatures being called — we'll fix those in upcoming tasks. For now just confirm no syntax errors in what we just wrote.

- [ ] **Step 6: Commit**

```bash
git add src/LVGLDisplayManager.cpp
git commit -m "feat: dark topbar and status bar builders"
```

---

### Task 5: buildWeatherPanel() — aircraft-present home screen

**Files:**
- Modify: `src/LVGLDisplayManager.cpp`

- [ ] **Step 1: Replace `buildWeatherCard()` with `buildWeatherPanel()`**

Remove the old `buildWeatherCard()` function entirely (lines 284–394) and replace with:

```cpp
void LVGLDisplayManager::buildWeatherPanel(lv_obj_t* parent, WeatherWidgets& w) {
    // parent is the left flex-fill column container
    lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Internal padding container
    lv_obj_t* pad = lv_obj_create(parent);
    lv_obj_set_size(pad, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(pad, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pad, 0, 0);
    lv_obj_set_style_pad_hor(pad, 16, 0);
    lv_obj_set_style_pad_ver(pad, 12, 0);
    lv_obj_clear_flag(pad, LV_OBJ_FLAG_SCROLLABLE);

    // --- Current conditions row ---
    // Temperature (large, left)
    w.label_temperature = lv_label_create(pad);
    lv_obj_set_style_text_font(w.label_temperature, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(w.label_temperature, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(w.label_temperature, "--°");
    lv_obj_set_pos(w.label_temperature, 0, 0);

    // Condition (to right of temperature, 14px secondary)
    w.label_weather_desc = lv_label_create(pad);
    lv_obj_set_style_text_font(w.label_weather_desc, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(w.label_weather_desc, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(w.label_weather_desc, "Loading...");
    lv_obj_set_pos(w.label_weather_desc, 110, 4);

    // Feels-like (11px dim, below condition)
    w.label_feels_like = lv_label_create(pad);
    lv_obj_set_style_text_font(w.label_feels_like, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(w.label_feels_like, COLOR_TEXT_DIM, 0);
    lv_label_set_text(w.label_feels_like, "Feels: --°");
    lv_obj_set_pos(w.label_feels_like, 110, 24);

    // Hi/Lo (11px dim)
    w.label_temp_range = lv_label_create(pad);
    lv_obj_set_style_text_font(w.label_temp_range, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(w.label_temp_range, COLOR_TEXT_DIM, 0);
    lv_label_set_text(w.label_temp_range, "H: --° L: --°");
    lv_obj_set_pos(w.label_temp_range, 110, 42);

    // --- Details strip (border-top + border-bottom) ---
    lv_obj_t* strip = lv_obj_create(pad);
    lv_obj_set_size(strip, LV_PCT(100), 36);
    lv_obj_set_pos(strip, 0, 68);
    lv_obj_set_style_bg_opa(strip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(strip, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(strip, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(strip, 1, 0);
    lv_obj_set_style_radius(strip, 0, 0);
    lv_obj_set_style_pad_all(strip, 0, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

    // Wind label + value
    lv_obj_t* lbl_wind_hdr = lv_label_create(strip);
    lv_obj_set_style_text_font(lbl_wind_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_wind_hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_wind_hdr, "WIND");
    lv_obj_set_pos(lbl_wind_hdr, 0, 4);

    w.label_wind = lv_label_create(strip);
    lv_obj_set_style_text_font(w.label_wind, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(w.label_wind, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(w.label_wind, "-- mph");
    lv_obj_set_pos(w.label_wind, 0, 18);

    // Humidity label + value
    lv_obj_t* lbl_hum_hdr = lv_label_create(strip);
    lv_obj_set_style_text_font(lbl_hum_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_hum_hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_hum_hdr, "HUMIDITY");
    lv_obj_set_pos(lbl_hum_hdr, 90, 4);

    w.label_humidity_val = lv_label_create(strip);
    lv_obj_set_style_text_font(w.label_humidity_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(w.label_humidity_val, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(w.label_humidity_val, "--%");
    lv_obj_set_pos(w.label_humidity_val, 90, 18);

    // Sunrise (amber)
    w.label_sunrise = lv_label_create(strip);
    lv_obj_set_style_text_font(w.label_sunrise, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(w.label_sunrise, COLOR_AMBER, 0);
    lv_label_set_text(w.label_sunrise, LV_SYMBOL_UP " --:--");
    lv_obj_align(w.label_sunrise, LV_ALIGN_RIGHT_MID, -70, 0);

    // Sunset (amber)
    w.label_sunset = lv_label_create(strip);
    lv_obj_set_style_text_font(w.label_sunset, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(w.label_sunset, COLOR_AMBER, 0);
    lv_label_set_text(w.label_sunset, LV_SYMBOL_DOWN " --:--");
    lv_obj_align(w.label_sunset, LV_ALIGN_RIGHT_MID, 0, 0);

    // --- 5-day forecast ---
    lv_obj_t* fc_header = lv_label_create(pad);
    lv_obj_set_style_text_font(fc_header, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(fc_header, COLOR_TEXT_DIM, 0);
    lv_label_set_text(fc_header, "5-DAY FORECAST");
    lv_obj_set_pos(fc_header, 0, 112);

    for (int i = 0; i < 5; i++) {
        int y = 130 + i * 46;
        WeatherWidgets::ForecastRow& row = w.forecast[i];

        row.container = lv_obj_create(pad);
        lv_obj_set_size(row.container, LV_PCT(100), 40);
        lv_obj_set_pos(row.container, 0, y);
        lv_obj_set_style_bg_color(row.container, COLOR_INSET, 0);
        lv_obj_set_style_bg_opa(row.container, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row.container, COLOR_BORDER, 0);
        lv_obj_set_style_border_width(row.container, 1, 0);
        lv_obj_set_style_radius(row.container, 4, 0);
        lv_obj_set_style_pad_hor(row.container, 10, 0);
        lv_obj_set_style_pad_ver(row.container, 0, 0);
        lv_obj_clear_flag(row.container, LV_OBJ_FLAG_SCROLLABLE);

        row.label_day = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_day, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_day, COLOR_TEXT_SECONDARY, 0);
        lv_label_set_text(row.label_day, "---");
        lv_obj_align(row.label_day, LV_ALIGN_LEFT_MID, 0, 0);

        row.label_cond = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_cond, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_cond, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_cond, "");
        lv_obj_align(row.label_cond, LV_ALIGN_CENTER, 0, 0);

        row.label_hi = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_hi, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_hi, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(row.label_hi, "--°");
        lv_obj_align(row.label_hi, LV_ALIGN_RIGHT_MID, -30, 0);

        row.label_lo = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_lo, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_lo, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_lo, "--°");
        lv_obj_align(row.label_lo, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}
```

- [ ] **Step 2: Add `updateWeatherWidgets()` helper**

Add this function after `buildWeatherPanel()`:

```cpp
void LVGLDisplayManager::updateWeatherWidgets(WeatherWidgets& w,
                                               const WeatherData& weather,
                                               int aircraftCount) {
    if (!w.label_temperature) return;

    char buf[64];

    snprintf(buf, sizeof(buf), "%.0f°F", weather.temperature);
    lv_label_set_text(w.label_temperature, buf);

    lv_label_set_text(w.label_weather_desc, weather.description.c_str());

    snprintf(buf, sizeof(buf), "Feels: %.0f°F", weather.feelsLike);
    lv_label_set_text(w.label_feels_like, buf);

    snprintf(buf, sizeof(buf), "H: %.0f°  L: %.0f°", weather.tempMax, weather.tempMin);
    lv_label_set_text(w.label_temp_range, buf);

    snprintf(buf, sizeof(buf), "%.0f mph", weather.windSpeed);
    lv_label_set_text(w.label_wind, buf);

    snprintf(buf, sizeof(buf), "%.0f%%", weather.humidity);
    lv_label_set_text(w.label_humidity_val, buf);

    snprintf(buf, sizeof(buf), LV_SYMBOL_UP " %s", formatTime(weather.sunrise).c_str());
    lv_label_set_text(w.label_sunrise, buf);

    snprintf(buf, sizeof(buf), LV_SYMBOL_DOWN " %s", formatTime(weather.sunset).c_str());
    lv_label_set_text(w.label_sunset, buf);

    for (int i = 0; i < 5; i++) {
        if (!w.forecast[i].container) continue;
        if (i < (int)weather.forecast.size()) {
            const auto& day = weather.forecast[i];
            lv_label_set_text(w.forecast[i].label_day,  day.dayName.c_str());
            lv_label_set_text(w.forecast[i].label_cond, day.condition.c_str());
            snprintf(buf, sizeof(buf), "%.0f°", day.tempMax);
            lv_label_set_text(w.forecast[i].label_hi, buf);
            snprintf(buf, sizeof(buf), "%.0f°", day.tempMin);
            lv_label_set_text(w.forecast[i].label_lo, buf);
        } else {
            lv_label_set_text(w.forecast[i].label_day,  "-");
            lv_label_set_text(w.forecast[i].label_cond, "");
            lv_label_set_text(w.forecast[i].label_hi,   "--°");
            lv_label_set_text(w.forecast[i].label_lo,   "--°");
        }
    }

    // Status bar
    if (w.label_status_left) {
        if (aircraftCount > 0) {
            char ts[16];
            struct tm ti;
            getLocalTime(&ti);
            strftime(ts, sizeof(ts), "%H:%M", &ti);
            snprintf(buf, sizeof(buf), "OPENSKY OK · %s", ts);
            lv_label_set_text(w.label_status_left, buf);
        } else {
            lv_label_set_text(w.label_status_left, "NO AIRCRAFT DETECTED");
        }
    }
    if (w.label_status_live) {
        if (aircraftCount > 0) {
            lv_obj_set_style_text_color(w.label_status_live, COLOR_SUCCESS, 0);
            lv_label_set_text(w.label_status_live, "● LIVE");
        } else {
            lv_obj_set_style_text_color(w.label_status_live, COLOR_TEXT_DIM, 0);
            lv_label_set_text(w.label_status_live, "● IDLE");
        }
    }
}
```

- [ ] **Step 3: Build check**

```bash
pio run -e full 2>&1 | grep "error:" | head -20
```

- [ ] **Step 4: Commit**

```bash
git add src/LVGLDisplayManager.cpp
git commit -m "feat: buildWeatherPanel and updateWeatherWidgets for dark aviation theme"
```

---

### Task 6: buildRadarPanel() + rebuild build_home_screen()

**Files:**
- Modify: `src/LVGLDisplayManager.cpp`

- [ ] **Step 1: Replace `buildAircraftCard()` with `buildRadarPanel()`**

Remove the old `buildAircraftCard()` (lines 396–438) and replace with:

```cpp
void LVGLDisplayManager::buildRadarPanel(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(parent, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(parent, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(parent, 1, 0);
    lv_obj_set_style_radius(parent, 0, 0);
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_gap(parent, 8, 0);

    // Section header
    lv_obj_t* hdr = lv_label_create(parent);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(hdr, "AREA CONTACTS");

    // Radar circle (190x190)
    radar_container = lv_obj_create(parent);
    lv_obj_set_size(radar_container, 190, 190);
    lv_obj_set_style_radius(radar_container, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(radar_container, COLOR_INSET, 0);
    lv_obj_set_style_bg_opa(radar_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(radar_container, COLOR_BORDER_ACCENT, 0);
    lv_obj_set_style_border_width(radar_container, 1, 0);
    lv_obj_set_style_pad_all(radar_container, 0, 0);
    lv_obj_clear_flag(radar_container, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // Range rings at 33% and 66% of radius (95px)
    const int ring_sizes[2] = { 64, 126 };
    for (int i = 0; i < 2; i++) {
        lv_obj_t* ring = lv_obj_create(radar_container);
        lv_obj_set_size(ring, ring_sizes[i], ring_sizes[i]);
        lv_obj_center(ring);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(0x1a3048), 0);
        lv_obj_set_style_border_width(ring, 1, 0);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }

    // Home dot (center, 8px cyan)
    lv_obj_t* home_dot = lv_obj_create(radar_container);
    lv_obj_set_size(home_dot, 8, 8);
    lv_obj_center(home_dot);
    lv_obj_set_style_radius(home_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(home_dot, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(home_dot, 0, 0);
    lv_obj_clear_flag(home_dot, LV_OBJ_FLAG_CLICKABLE);

    // Pre-create aircraft blips (all hidden, default center)
    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        radar_blips[i] = lv_obj_create(radar_container);
        lv_obj_set_size(radar_blips[i], 8, 8);
        lv_obj_set_style_radius(radar_blips[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(radar_blips[i], COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(radar_blips[i], 0, 0);
        lv_obj_set_pos(radar_blips[i], 91, 91);  // center - 4 (blip half-width)
        lv_obj_add_flag(radar_blips[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(radar_blips[i], LV_OBJ_FLAG_CLICKABLE);
    }

    // Contact count + units
    label_contact_count = lv_label_create(parent);
    lv_obj_set_style_text_font(label_contact_count, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(label_contact_count, COLOR_ACCENT, 0);
    lv_label_set_text(label_contact_count, "0");

    lv_obj_t* lbl_units = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_units, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_units, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_units, "CONTACTS");

    // VIEW AIRCRAFT button
    btn_view_planes = lv_button_create(parent);
    lv_obj_set_size(btn_view_planes, 220, 36);
    lv_obj_set_style_bg_color(btn_view_planes, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(btn_view_planes, 4, 0);
    lv_obj_add_event_cb(btn_view_planes, event_btn_view_planes, LV_EVENT_CLICKED, this);

    lv_obj_t* btn_lbl = lv_label_create(btn_view_planes);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(btn_lbl, COLOR_TEXT_ON_ACCENT, 0);
    lv_label_set_text(btn_lbl, LV_SYMBOL_RIGHT "  VIEW AIRCRAFT");
    lv_obj_center(btn_lbl);

    // Tapping anywhere on the panel also navigates to aircraft list
    lv_obj_add_event_cb(parent, event_btn_view_planes, LV_EVENT_CLICKED, this);
}
```

- [ ] **Step 2: Rewrite `build_home_screen()`**

Replace lines 440–455:

```cpp
void LVGLDisplayManager::build_home_screen() {
    screen_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_home, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen_home, LV_OPA_COVER, 0);

    buildTopBar(screen_home, homeWidgets);
    buildStatusBar(screen_home, homeWidgets);

    // Body: 800 x 396px, between top bar (58) and status bar (26)
    lv_obj_t* body = lv_obj_create(screen_home);
    lv_obj_set_size(body, 800, 396);
    lv_obj_set_pos(body, 0, 58);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Weather panel (left, fills remaining space)
    lv_obj_t* weather_col = lv_obj_create(body);
    lv_obj_set_flex_grow(weather_col, 1);
    lv_obj_set_height(weather_col, 396);
    buildWeatherPanel(weather_col, homeWidgets);

    // Radar panel (right, fixed 310px)
    lv_obj_t* radar_col = lv_obj_create(body);
    lv_obj_set_size(radar_col, 310, 396);
    buildRadarPanel(radar_col);
}
```

- [ ] **Step 3: Rewrite `update_home_screen()` with blip positioning and screen switching**

Replace the old `update_home_screen()` (lines 590–662):

```cpp
void LVGLDisplayManager::update_home_screen(const WeatherData& weather,
                                             const Aircraft* aircraft,
                                             int aircraftCount) {
    // --- Switch home screen variant if aircraft count changed ---
    bool nowHas = (aircraftCount > 0);
    if (nowHas != homeHasAircraft) {
        homeHasAircraft = nowHas;
        lv_screen_load(homeHasAircraft ? screen_home : screen_home_empty);
    }

    // --- Update weather labels on whichever screen is active ---
    WeatherWidgets& w = homeHasAircraft ? homeWidgets : emptyWidgets;
    update_clock(w);
    updateWeatherWidgets(w, weather, aircraftCount);

    if (!homeHasAircraft) return;

    // --- Update radar panel ---
    char count_buf[8];
    snprintf(count_buf, sizeof(count_buf), "%d", aircraftCount);
    lv_label_set_text(label_contact_count, count_buf);

    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        if (!radar_blips[i]) continue;

        if (aircraft && i < aircraftCount && aircraft[i].valid) {
            const Aircraft& a = aircraft[i];
            float distNm = GeoUtils::distanceNm(
                Config::HOME_LAT, Config::HOME_LON,
                a.latitude, a.longitude);
            float bearing = GeoUtils::bearingDeg(
                Config::HOME_LAT, Config::HOME_LON,
                a.latitude, a.longitude);

            auto pos = GeoUtils::blipPosition(distNm, bearing,
                                               Config::RADAR_MAX_RANGE_NM, 95, 6);
            lv_obj_set_pos(radar_blips[i], pos.x - 4, pos.y - 4);

            // Amber for low altitude (<5000 ft), cyan otherwise
            float altFt = a.altitude * 3.28084f;
            lv_color_t blipColor = (altFt > 0 && altFt < 5000.0f) ? COLOR_AMBER : COLOR_ACCENT;
            lv_obj_set_style_bg_color(radar_blips[i], blipColor, 0);
            lv_obj_remove_flag(radar_blips[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(radar_blips[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
```

- [ ] **Step 4: Update the main `update()` function**

Replace lines 734–741:

```cpp
void LVGLDisplayManager::update(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount) {
    if (currentScreen == SCREEN_HOME) {
        update_home_screen(weather, aircraft, aircraftCount);
    } else if (currentScreen == SCREEN_AIRCRAFT_DETAIL && aircraft && aircraft->valid) {
        update_aircraft_screen(*aircraft);
    }
}
```

- [ ] **Step 5: Add `GeoUtils.h` include at top of `LVGLDisplayManager.cpp`**

After `#include "LVGLDisplayManager.h"`:

```cpp
#include "utils/GeoUtils.h"
```

- [ ] **Step 6: Update `setScreen()` to handle SCREEN_NO_AIRCRAFT as SCREEN_HOME**

Replace the `setScreen()` function:

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

- [ ] **Step 7: Build check**

```bash
pio run -e full 2>&1 | grep "error:" | head -20
```

- [ ] **Step 8: Commit**

```bash
git add src/LVGLDisplayManager.cpp
git commit -m "feat: buildRadarPanel, rebuild build_home_screen with dynamic switching"
```

---

### Task 7: build_home_empty_screen() — full-width no-aircraft home

**Files:**
- Modify: `src/LVGLDisplayManager.cpp`

- [ ] **Step 1: Add `update_clock()` to take WeatherWidgets parameter**

Replace the old `update_clock()`:

```cpp
void LVGLDisplayManager::update_clock(WeatherWidgets& w) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%I:%M %p", &timeinfo);
    if (w.label_time) lv_label_set_text(w.label_time, time_buf);

    char date_buf[32];
    strftime(date_buf, sizeof(date_buf), "%a, %b %d", &timeinfo);
    if (w.label_date) lv_label_set_text(w.label_date, date_buf);

    if (statusClearTime > 0 && millis() >= statusClearTime) {
        statusClearTime = 0;
        statusMessage = "";
    }
}
```

- [ ] **Step 2: Add `build_home_empty_screen()`**

Add after `build_home_screen()`:

```cpp
void LVGLDisplayManager::build_home_empty_screen() {
    screen_home_empty = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_home_empty, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen_home_empty, LV_OPA_COVER, 0);

    buildTopBar(screen_home_empty, emptyWidgets);
    buildStatusBar(screen_home_empty, emptyWidgets);

    // Body: full 800px width, two-column layout
    lv_obj_t* body = lv_obj_create(screen_home_empty);
    lv_obj_set_size(body, 800, 396);
    lv_obj_set_pos(body, 0, 58);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);

    // Left column: current conditions (340px)
    lv_obj_t* left_col = lv_obj_create(body);
    lv_obj_set_size(left_col, 340, 396);
    lv_obj_set_style_bg_opa(left_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_col, 0, 0);
    lv_obj_set_style_pad_hor(left_col, 20, 0);
    lv_obj_set_style_pad_ver(left_col, 20, 0);
    lv_obj_clear_flag(left_col, LV_OBJ_FLAG_SCROLLABLE);

    // Temperature (80px bold)
    emptyWidgets.label_temperature = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_temperature, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_temperature, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(emptyWidgets.label_temperature, "--°F");
    lv_obj_set_pos(emptyWidgets.label_temperature, 0, 0);

    // Condition (16px secondary)
    emptyWidgets.label_weather_desc = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_weather_desc, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_weather_desc, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(emptyWidgets.label_weather_desc, "Loading...");
    lv_obj_set_pos(emptyWidgets.label_weather_desc, 0, 62);

    // Feels-like (13px dim)
    emptyWidgets.label_feels_like = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_feels_like, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_feels_like, COLOR_TEXT_DIM, 0);
    lv_label_set_text(emptyWidgets.label_feels_like, "Feels: --°F");
    lv_obj_set_pos(emptyWidgets.label_feels_like, 0, 84);

    // Hi/Lo (12px dim)
    emptyWidgets.label_temp_range = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_temp_range, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_temp_range, COLOR_TEXT_DIM, 0);
    lv_label_set_text(emptyWidgets.label_temp_range, "H: --°  L: --°");
    lv_obj_set_pos(emptyWidgets.label_temp_range, 0, 102);

    // Divider
    lv_obj_t* div = lv_obj_create(left_col);
    lv_obj_set_size(div, 300, 1);
    lv_obj_set_pos(div, 0, 124);
    lv_obj_set_style_bg_color(div, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_CLICKABLE);

    // Wind
    lv_obj_t* lbl_wind_hdr = lv_label_create(left_col);
    lv_obj_set_style_text_font(lbl_wind_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_wind_hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_wind_hdr, "WIND");
    lv_obj_set_pos(lbl_wind_hdr, 0, 134);

    emptyWidgets.label_wind = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_wind, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_wind, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(emptyWidgets.label_wind, "-- mph");
    lv_obj_set_pos(emptyWidgets.label_wind, 0, 150);

    // Humidity
    lv_obj_t* lbl_hum_hdr = lv_label_create(left_col);
    lv_obj_set_style_text_font(lbl_hum_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_hum_hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_hum_hdr, "HUMIDITY");
    lv_obj_set_pos(lbl_hum_hdr, 100, 134);

    emptyWidgets.label_humidity_val = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_humidity_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_humidity_val, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(emptyWidgets.label_humidity_val, "--%");
    lv_obj_set_pos(emptyWidgets.label_humidity_val, 100, 150);

    // Sunrise/Sunset (amber, 14px)
    emptyWidgets.label_sunrise = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_sunrise, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_sunrise, COLOR_AMBER, 0);
    lv_label_set_text(emptyWidgets.label_sunrise, LV_SYMBOL_UP " --:--");
    lv_obj_set_pos(emptyWidgets.label_sunrise, 0, 180);

    emptyWidgets.label_sunset = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_sunset, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_sunset, COLOR_AMBER, 0);
    lv_label_set_text(emptyWidgets.label_sunset, LV_SYMBOL_DOWN " --:--");
    lv_obj_set_pos(emptyWidgets.label_sunset, 80, 180);

    // No-contacts watermark
    lv_obj_t* watermark = lv_label_create(screen_home_empty);
    lv_obj_set_style_text_font(watermark, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(watermark, COLOR_BORDER, 0);
    lv_label_set_text(watermark, "NO CONTACTS IN RANGE");
    lv_obj_align(watermark, LV_ALIGN_BOTTOM_MID, 0, -32);

    // Right column: 5-day forecast (flex-fill)
    lv_obj_t* right_col = lv_obj_create(body);
    lv_obj_set_flex_grow(right_col, 1);
    lv_obj_set_height(right_col, 396);
    lv_obj_set_style_bg_opa(right_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(right_col, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(right_col, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(right_col, 1, 0);
    lv_obj_set_style_pad_hor(right_col, 20, 0);
    lv_obj_set_style_pad_ver(right_col, 16, 0);
    lv_obj_clear_flag(right_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* fc_hdr = lv_label_create(right_col);
    lv_obj_set_style_text_font(fc_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(fc_hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(fc_hdr, "5-DAY FORECAST");
    lv_obj_set_pos(fc_hdr, 0, 0);

    lv_obj_t* fc_div = lv_obj_create(right_col);
    lv_obj_set_size(fc_div, LV_PCT(100), 1);
    lv_obj_set_pos(fc_div, 0, 18);
    lv_obj_set_style_bg_color(fc_div, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(fc_div, 0, 0);
    lv_obj_clear_flag(fc_div, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < 5; i++) {
        int y = 26 + i * 60;
        WeatherWidgets::ForecastRow& row = emptyWidgets.forecast[i];

        row.container = lv_obj_create(right_col);
        lv_obj_set_size(row.container, LV_PCT(100), 54);
        lv_obj_set_pos(row.container, 0, y);
        lv_obj_set_style_bg_opa(row.container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_side(row.container, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row.container, COLOR_BORDER, 0);
        lv_obj_set_style_border_width(row.container, 1, 0);
        lv_obj_set_style_radius(row.container, 0, 0);
        lv_obj_set_style_pad_all(row.container, 0, 0);
        lv_obj_clear_flag(row.container, LV_OBJ_FLAG_SCROLLABLE);

        row.label_day = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_day, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_day, COLOR_TEXT_SECONDARY, 0);
        lv_label_set_text(row.label_day, "---");
        lv_obj_align(row.label_day, LV_ALIGN_LEFT_MID, 0, 0);

        row.label_cond = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_cond, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_cond, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_cond, "");
        lv_obj_align(row.label_cond, LV_ALIGN_CENTER, 0, 0);

        row.label_hi = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_hi, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_hi, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(row.label_hi, "--°");
        lv_obj_align(row.label_hi, LV_ALIGN_RIGHT_MID, -30, 0);

        row.label_lo = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_lo, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_lo, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_lo, "--°");
        lv_obj_align(row.label_lo, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

void LVGLDisplayManager::build_no_aircraft_screen() {
    // In the new design, "no aircraft" is the empty home screen — not a separate screen.
    // This function exists only for backward compatibility with callers in App.cpp.
    // build_home_empty_screen() is called from initialize() directly.
}
```

- [ ] **Step 2: Build check**

```bash
pio run -e full 2>&1 | grep "error:" | head -30
```

- [ ] **Step 3: Commit**

```bash
git add src/LVGLDisplayManager.cpp
git commit -m "feat: build_home_empty_screen full-width two-column weather layout"
```

---

### Task 8: build_aircraft_screen() — complete redesign

**Files:**
- Modify: `src/LVGLDisplayManager.cpp`

- [ ] **Step 1: Replace `build_aircraft_screen()`**

Replace the entire function (lines 457–558):

```cpp
void LVGLDisplayManager::build_aircraft_screen() {
    screen_aircraft = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_aircraft, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen_aircraft, LV_OPA_COVER, 0);

    // === TOP BAR (70px — slightly taller than home) ===
    lv_obj_t* topbar = lv_obj_create(screen_aircraft);
    lv_obj_set_size(topbar, 800, 70);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar, COLOR_TOPBAR, 0);
    lv_obj_set_style_border_side(topbar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(topbar, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(topbar, 1, 0);
    lv_obj_set_style_radius(topbar, 0, 0);
    lv_obj_set_style_pad_all(topbar, 0, 0);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);

    // Back button (left)
    btn_back_home = lv_button_create(topbar);
    lv_obj_set_size(btn_back_home, 90, 42);
    lv_obj_align(btn_back_home, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_set_style_bg_opa(btn_back_home, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btn_back_home, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(btn_back_home, 1, 0);
    lv_obj_set_style_border_opa(btn_back_home, 80, 0);
    lv_obj_set_style_radius(btn_back_home, 4, 0);
    lv_obj_add_event_cb(btn_back_home, event_btn_back_home, LV_EVENT_CLICKED, this);

    lv_obj_t* back_lbl = lv_label_create(btn_back_home);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(back_lbl, COLOR_ACCENT, 0);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " BACK");
    lv_obj_center(back_lbl);

    // Callsign (center, 36px bold cyan, letterspaced)
    label_callsign = lv_label_create(topbar);
    lv_obj_set_style_text_font(label_callsign, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(label_callsign, COLOR_ACCENT, 0);
    lv_label_set_text(label_callsign, "---");
    lv_obj_align(label_callsign, LV_ALIGN_CENTER, 0, 0);

    // Distance + bearing (right side, stacked)
    label_distance = lv_label_create(topbar);
    lv_obj_set_style_text_font(label_distance, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(label_distance, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_distance, "-- nm · --°");
    lv_obj_align(label_distance, LV_ALIGN_RIGHT_MID, -18, -8);

    lv_obj_t* lbl_loc = lv_label_create(topbar);
    lv_obj_set_style_text_font(lbl_loc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_loc, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_loc, "FROM " Config::WEATHER_CITY);
    lv_obj_align(lbl_loc, LV_ALIGN_RIGHT_MID, -18, 12);

    // === STATUS BAR (26px, bottom) ===
    lv_obj_t* sbar = lv_obj_create(screen_aircraft);
    lv_obj_set_size(sbar, 800, 26);
    lv_obj_align(sbar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(sbar, COLOR_STATUSBAR, 0);
    lv_obj_set_style_border_side(sbar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(sbar, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(sbar, 1, 0);
    lv_obj_set_style_radius(sbar, 0, 0);
    lv_obj_set_style_pad_hor(sbar, 12, 0);
    lv_obj_clear_flag(sbar, LV_OBJ_FLAG_SCROLLABLE);

    label_status_aircraft = lv_label_create(sbar);
    lv_obj_set_style_text_font(label_status_aircraft, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_status_aircraft, COLOR_TEXT_DIM, 0);
    lv_label_set_text(label_status_aircraft, "");
    lv_obj_align(label_status_aircraft, LV_ALIGN_LEFT_MID, 0, 0);

    // === BODY (top:70, bottom:26, height: 384, padding 14 18) ===
    lv_obj_t* body = lv_obj_create(screen_aircraft);
    lv_obj_set_size(body, 800, 384);
    lv_obj_set_pos(body, 0, 70);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 18, 0);
    lv_obj_set_style_pad_ver(body, 14, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_gap(body, 10, 0);

    // === ROW 1: Identity cards (flex row) ===
    lv_obj_t* row1 = lv_obj_create(body);
    lv_obj_set_size(row1, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_set_style_radius(row1, 0, 0);
    lv_obj_clear_flag(row1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(row1, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_gap(row1, 10, 0);

    // Helper lambda to make an identity card
    auto makeCard = [&](lv_obj_t* parent, const char* labelText, lv_obj_t** valueLabel,
                        bool isSquawk = false) {
        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_size(card, LV_SIZE_CONTENT, 52);
        lv_obj_set_style_bg_color(card, COLOR_PANEL, 0);
        lv_obj_set_style_border_color(card, COLOR_BORDER, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 4, 0);
        lv_obj_set_style_pad_hor(card, 12, 0);
        lv_obj_set_style_pad_ver(card, 6, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(card);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_label_set_text(lbl, labelText);
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

        *valueLabel = lv_label_create(card);
        lv_obj_set_style_text_font(*valueLabel, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(*valueLabel,
            isSquawk ? COLOR_AMBER : COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(*valueLabel, "--");
        lv_obj_align(*valueLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    };

    makeCard(row1, "AIRLINE",   &label_airline);
    makeCard(row1, "AIRCRAFT",  &label_aircraft_type);
    makeCard(row1, "SQUAWK",    &label_squawk, true);

    // === ROW 2: Route block ===
    lv_obj_t* route_block = lv_obj_create(body);
    lv_obj_set_size(route_block, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(route_block, COLOR_INSET, 0);
    lv_obj_set_style_border_color(route_block, COLOR_BORDER_ACCENT, 0);
    lv_obj_set_style_border_width(route_block, 1, 0);
    lv_obj_set_style_radius(route_block, 6, 0);
    lv_obj_set_style_pad_hor(route_block, 18, 0);
    lv_obj_set_style_pad_ver(route_block, 12, 0);
    lv_obj_clear_flag(route_block, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* route_lbl_hdr = lv_label_create(route_block);
    lv_obj_set_style_text_font(route_lbl_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(route_lbl_hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(route_lbl_hdr, "ROUTE");
    lv_obj_align(route_lbl_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    label_route_main = lv_label_create(route_block);
    lv_obj_set_style_text_font(label_route_main, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_route_main, COLOR_ACCENT, 0);
    lv_label_set_text(label_route_main, "LOOKING UP...");
    lv_obj_align(label_route_main, LV_ALIGN_TOP_LEFT, 0, 18);

    label_route_sub = lv_label_create(route_block);
    lv_obj_set_style_text_font(label_route_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_route_sub, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_route_sub, "");
    lv_obj_align(label_route_sub, LV_ALIGN_TOP_LEFT, 0, 52);

    // === ROW 3: Instrument tiles (flex row, flex-grow fills rest) ===
    lv_obj_t* row3 = lv_obj_create(body);
    lv_obj_set_flex_grow(row3, 1);
    lv_obj_set_width(row3, LV_PCT(100));
    lv_obj_set_style_bg_opa(row3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row3, 0, 0);
    lv_obj_set_style_pad_all(row3, 0, 0);
    lv_obj_set_style_radius(row3, 0, 0);
    lv_obj_clear_flag(row3, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(row3, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row3, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Helper to make an instrument tile
    auto makeTile = [&](lv_obj_t* parent, const char* tileLabel, lv_obj_t** valueLabel) {
        lv_obj_t* tile = lv_obj_create(parent);
        lv_obj_set_flex_grow(tile, 1);
        lv_obj_set_height(tile, LV_PCT(100));
        lv_obj_set_style_bg_color(tile, COLOR_PANEL, 0);
        lv_obj_set_style_border_color(tile, COLOR_BORDER, 0);
        lv_obj_set_style_border_width(tile, 1, 0);
        lv_obj_set_style_radius(tile, 5, 0);
        lv_obj_set_style_pad_all(tile, 10, 0);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_layout(tile, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_gap(tile, 6, 0);

        lv_obj_t* lbl = lv_label_create(tile);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_label_set_text(lbl, tileLabel);

        *valueLabel = lv_label_create(tile);
        lv_obj_set_style_text_font(*valueLabel, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(*valueLabel, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(*valueLabel, "--");
    };

    makeTile(row3, "ALTITUDE",   &label_altitude);
    makeTile(row3, "SPEED",      &label_velocity);
    makeTile(row3, "HEADING",    &label_heading);
    makeTile(row3, "VERT SPEED", &label_vert_speed);
}
```

- [ ] **Step 2: Replace `update_aircraft_screen()`**

Replace lines 664–708:

```cpp
void LVGLDisplayManager::update_aircraft_screen(const Aircraft& aircraft) {
    if (!screen_aircraft) return;

    // Callsign
    lv_label_set_text(label_callsign, aircraft.callsign.c_str());

    // Distance + bearing from home
    float distNm = GeoUtils::distanceNm(
        Config::HOME_LAT, Config::HOME_LON,
        aircraft.latitude, aircraft.longitude);
    float bearing = GeoUtils::bearingDeg(
        Config::HOME_LAT, Config::HOME_LON,
        aircraft.latitude, aircraft.longitude);
    const char* card = GeoUtils::cardinalDir(bearing);

    char dist_buf[48];
    snprintf(dist_buf, sizeof(dist_buf), "%.1f nm  ·  %.0f° %s", distNm, bearing, card);
    lv_label_set_text(label_distance, dist_buf);

    // Identity cards
    lv_label_set_text(label_airline,
        aircraft.airline.length() > 0 ? aircraft.airline.c_str() : "Unknown");
    lv_label_set_text(label_aircraft_type,
        aircraft.aircraftType.length() > 0 ? aircraft.aircraftType.c_str() : "Aircraft");
    lv_label_set_text(label_squawk,
        aircraft.squawk.length() > 0 ? aircraft.squawk.c_str() : "----");

    // Route block
    if (aircraft.origin.length() > 0 && aircraft.destination.length() > 0) {
        char route_buf[32];
        snprintf(route_buf, sizeof(route_buf), "%s  →  %s",
                 aircraft.origin.c_str(), aircraft.destination.c_str());
        lv_label_set_text(label_route_main, route_buf);
        lv_obj_set_style_text_color(label_route_main, COLOR_ACCENT, 0);
        lv_label_set_text(label_route_sub, "");   // sub filled by RouteCache in Task 10
    } else if (aircraft.callsign.length() > 0) {
        lv_label_set_text(label_route_main, "LOOKING UP...");
        lv_obj_set_style_text_color(label_route_main, COLOR_TEXT_DIM, 0);
        lv_label_set_text(label_route_sub, "");
    } else {
        lv_label_set_text(label_route_main, "ROUTE UNAVAILABLE");
        lv_obj_set_style_text_color(label_route_main, COLOR_TEXT_DIM, 0);
        lv_label_set_text(label_route_sub, "");
    }

    // Altitude (m → ft)
    char alt_buf[24];
    snprintf(alt_buf, sizeof(alt_buf), "%.0f ft", aircraft.altitude * 3.28084f);
    lv_label_set_text(label_altitude, alt_buf);

    // Speed (m/s → kts)
    char vel_buf[24];
    snprintf(vel_buf, sizeof(vel_buf), "%.0f kts", aircraft.velocity * 1.94384f);
    lv_label_set_text(label_velocity, vel_buf);

    // Heading + cardinal
    char head_buf[24];
    snprintf(head_buf, sizeof(head_buf), "%.0f° %s",
             aircraft.heading, GeoUtils::cardinalDir(aircraft.heading));
    lv_label_set_text(label_heading, head_buf);

    // Vertical speed (m/s → fpm), color-coded
    float fpm = aircraft.verticalRate * 196.85f;
    char vs_buf[24];
    snprintf(vs_buf, sizeof(vs_buf), "%+.0f fpm", fpm);
    lv_label_set_text(label_vert_speed, vs_buf);
    if (fpm > 50.0f) {
        lv_obj_set_style_text_color(label_vert_speed, COLOR_SUCCESS, 0);
    } else if (fpm < -50.0f) {
        lv_obj_set_style_text_color(label_vert_speed, lv_color_hex(0xef4444), 0);
    } else {
        lv_obj_set_style_text_color(label_vert_speed, COLOR_TEXT_PRIMARY, 0);
    }
}
```

- [ ] **Step 3: Update `setStatusMessage()` to use new widget names**

Replace `setStatusMessage()`:

```cpp
void LVGLDisplayManager::setStatusMessage(const String& msg) {
    statusMessage = msg;
    statusClearTime = millis() + Config::UI_STATUS_MS;
    if (homeWidgets.label_status_left)  lv_label_set_text(homeWidgets.label_status_left,  msg.c_str());
    if (emptyWidgets.label_status_left) lv_label_set_text(emptyWidgets.label_status_left, msg.c_str());
    if (label_status_aircraft)          lv_label_set_text(label_status_aircraft,           msg.c_str());
}
```

- [ ] **Step 4: Remove leftover `label_latitude` / `label_longitude` references**

Delete `label_latitude` and `label_longitude` from constructor initializer (they're gone from the header). Also delete any remaining references to `screen_no_aircraft`.

- [ ] **Step 5: Full build**

```bash
pio run -e full 2>&1 | tail -30
```

Expected: `[SUCCESS]`. Fix any remaining undeclared identifier errors.

- [ ] **Step 6: Commit**

```bash
git add src/LVGLDisplayManager.cpp src/LVGLDisplayManager.h
git commit -m "feat: dark aircraft detail screen with route block and instrument tiles"
```

---

### Task 9: RouteCache service (NVS)

**Files:**
- Create: `src/services/RouteCache.h`
- Create: `src/services/RouteCache.cpp`

- [ ] **Step 1: Create `src/services/RouteCache.h`**

```cpp
// src/services/RouteCache.h
// NVS-backed route cache + AeroDataBox HTTP lookup.
// Routes are cached forever (airline routes don't change).
#pragma once
#include <Arduino.h>
#include <Preferences.h>

class RouteCache {
public:
    RouteCache();

    /// Check NVS, then fetch from AeroDataBox if not cached.
    /// Returns true and populates origin/destination on success.
    /// origin/destination are IATA airport codes (e.g. "BOS", "LAX").
    bool lookup(const String& callsign, String& origin, String& destination,
                String& originName, String& destinationName);

    /// Store a resolved route in NVS.
    void store(const String& callsign,
               const String& origin, const String& destination,
               const String& originName, const String& destinationName);

private:
    Preferences prefs_;

    /// Convert ICAO callsign prefix to IATA flight number (e.g. "UAL1234" → "UA1234").
    String toIataFlightNumber(const String& callsign);

    /// HTTP fetch from AeroDataBox. Returns true on success.
    bool fetchFromApi(const String& iataFlightNumber,
                      String& origin, String& destination,
                      String& originName, String& destinationName);
};
```

- [ ] **Step 2: Create `src/services/RouteCache.cpp`**

```cpp
// src/services/RouteCache.cpp
#include "RouteCache.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config/Config.h"

// NVS namespace (max 15 chars)
static constexpr char NVS_NS[] = "route_cache";

// ICAO → IATA mapping (must stay in sync with OpenSkyService kAirlineTable)
static const struct { const char* icao; const char* iata; } kIataMap[] = {
    { "AAL", "AA" }, { "DAL", "DL" }, { "UAL", "UA" }, { "SWA", "WN" },
    { "JBU", "B6" }, { "FDX", "FX" }, { "UPS", "5X" }, { "ASA", "AS" },
    { "FFT", "F9" }, { "NKS", "NK" },
};

RouteCache::RouteCache() {}

String RouteCache::toIataFlightNumber(const String& callsign) {
    if (callsign.length() < 4) return "";
    String prefix = callsign.substring(0, 3);
    for (const auto& e : kIataMap) {
        if (prefix == e.icao) {
            return String(e.iata) + callsign.substring(3);
        }
    }
    return "";
}

bool RouteCache::lookup(const String& callsign,
                         String& origin, String& destination,
                         String& originName, String& destinationName) {
    if (callsign.isEmpty()) return false;

    // Check NVS first
    prefs_.begin(NVS_NS, true);  // read-only
    String cached = prefs_.getString(callsign.c_str(), "");
    prefs_.end();

    if (cached.length() > 0) {
        // Format: "BOS|Boston Logan|LAX|Los Angeles Intl"
        int p1 = cached.indexOf('|');
        int p2 = cached.indexOf('|', p1 + 1);
        int p3 = cached.indexOf('|', p2 + 1);
        if (p1 > 0 && p2 > p1 && p3 > p2) {
            origin          = cached.substring(0, p1);
            originName      = cached.substring(p1 + 1, p2);
            destination     = cached.substring(p2 + 1, p3);
            destinationName = cached.substring(p3 + 1);
            return true;
        }
    }

    // Not in NVS — try API
    String iataFlight = toIataFlightNumber(callsign);
    if (iataFlight.isEmpty()) return false;

    // Skip if key is placeholder
    String key = Config::AERODATABOX_API_KEY;
    if (key == "your-aerodatabox-api-key" || key.isEmpty()) return false;

    if (fetchFromApi(iataFlight, origin, destination, originName, destinationName)) {
        store(callsign, origin, destination, originName, destinationName);
        return true;
    }
    return false;
}

void RouteCache::store(const String& callsign,
                        const String& origin, const String& destination,
                        const String& originName, const String& destinationName) {
    // NVS key max length = 15 chars; callsign ≤ 8 chars — safe
    String value = origin + "|" + originName + "|" + destination + "|" + destinationName;
    prefs_.begin(NVS_NS, false);  // read-write
    prefs_.putString(callsign.c_str(), value);
    prefs_.end();
    Serial.printf("[RouteCache] Stored: %s → %s|%s\n",
                  callsign.c_str(), origin.c_str(), destination.c_str());
}

bool RouteCache::fetchFromApi(const String& iataFlightNumber,
                               String& origin, String& destination,
                               String& originName, String& destinationName) {
    // Today's date in YYYY-MM-DD
    struct tm ti;
    if (!getLocalTime(&ti)) return false;
    char dateBuf[12];
    strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &ti);

    String url = "https://aerodatabox.p.rapidapi.com/flights/number/";
    url += iataFlightNumber;
    url += "/";
    url += dateBuf;

    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000);
    http.addHeader("x-rapidapi-key",  Config::AERODATABOX_API_KEY);
    http.addHeader("x-rapidapi-host", "aerodatabox.p.rapidapi.com");

    Serial.printf("[RouteCache] GET %s\n", url.c_str());
    int code = http.GET();

    if (code != 200) {
        Serial.printf("[RouteCache] HTTP %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[RouteCache] JSON error: %s\n", err.c_str());
        return false;
    }

    // AeroDataBox returns an array; use first element
    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull() || arr.size() == 0) {
        // Some responses are objects wrapped in array — try root object
        if (!doc["departure"].isNull()) {
            origin          = doc["departure"]["airport"]["iata"].as<String>();
            originName      = doc["departure"]["airport"]["name"].as<String>();
            destination     = doc["arrival"]["airport"]["iata"].as<String>();
            destinationName = doc["arrival"]["airport"]["name"].as<String>();
        } else {
            return false;
        }
    } else {
        JsonObject flight = arr[0];
        origin          = flight["departure"]["airport"]["iata"].as<String>();
        originName      = flight["departure"]["airport"]["name"].as<String>();
        destination     = flight["arrival"]["airport"]["iata"].as<String>();
        destinationName = flight["arrival"]["airport"]["name"].as<String>();
    }

    if (origin.isEmpty() || destination.isEmpty()) return false;

    Serial.printf("[RouteCache] Resolved: %s → %s (%s → %s)\n",
                  origin.c_str(), destination.c_str(),
                  originName.c_str(), destinationName.c_str());
    return true;
}
```

- [ ] **Step 3: Build check**

```bash
pio run -e full 2>&1 | grep "error:" | head -20
```

- [ ] **Step 4: Commit**

```bash
git add src/services/RouteCache.h src/services/RouteCache.cpp
git commit -m "feat: RouteCache NVS service with AeroDataBox HTTP lookup"
```

---

### Task 10: App wiring — RouteCache integration

**Files:**
- Modify: `src/core/App.h`
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Add RouteCache to `src/core/App.h`**

Add include and member:

```cpp
#include "services/RouteCache.h"
```

In the `App` class private section, add:
```cpp
RouteCache* routeCache_;
bool        routeFetchDone_;
String      lastRouteFetchCallsign_;
```

- [ ] **Step 2: Initialize RouteCache in `App::begin()`**

In the constructor initializer list, add:
```cpp
, routeCache_(nullptr)
, routeFetchDone_(false)
, lastRouteFetchCallsign_("")
```

In `App::begin()`, after creating other services:
```cpp
routeCache_ = new RouteCache();
```

Add to destructor:
```cpp
delete routeCache_;
```

- [ ] **Step 3: Trigger route lookup in `App::updateDisplay()`**

In the `SCREEN_AIRCRAFT_DETAIL` block of `updateDisplay()`, before calling `display_->update()`:

```cpp
if (screen == LVGLDisplayManager::SCREEN_AIRCRAFT_DETAIL) {
    if (currentAircraftCount_ > 0
        && currentAircraftIndex_ < currentAircraftCount_
        && aircraftList_[currentAircraftIndex_].valid) {

        Aircraft& cur = aircraftList_[currentAircraftIndex_];

        // Trigger route lookup once per callsign
        if (routeCache_ && cur.origin.isEmpty()
            && cur.callsign != lastRouteFetchCallsign_) {
            lastRouteFetchCallsign_ = cur.callsign;
            routeFetchDone_ = false;
        }
        if (routeCache_ && !routeFetchDone_ && !cur.callsign.isEmpty()) {
            routeFetchDone_ = true;
            String org, dst, orgName, dstName;
            if (routeCache_->lookup(cur.callsign, org, dst, orgName, dstName)) {
                cur.origin      = org;
                cur.destination = dst;
            }
        }

        display_->update(currentWeather_, &cur, currentAircraftCount_);
    } else {
        display_->setScreen(LVGLDisplayManager::SCREEN_HOME);
    }
    return;
}
```

- [ ] **Step 4: Reset route fetch state when aircraft index changes**

In `App::tick()`, when the aircraft index advances:
```cpp
if (currentAircraftCount_ > 1 && (now - lastPlaneSwitchMs_) >= Config::PLANE_DISPLAY_TIME) {
    currentAircraftIndex_ = (currentAircraftIndex_ + 1) % currentAircraftCount_;
    lastPlaneSwitchMs_ = now;
    routeFetchDone_ = false;            // re-fetch for new aircraft
    lastRouteFetchCallsign_ = "";
}
```

Also reset when navigating to detail screen — in `shouldReturnToHome()` callback or in the screen-change detection block, add:
```cpp
if (changedAt != 0 && changedAt != lastRedrawnScreenChange_) {
    if (display_->getCurrentScreen() == LVGLDisplayManager::SCREEN_AIRCRAFT_DETAIL) {
        routeFetchDone_ = false;
        lastRouteFetchCallsign_ = "";
    }
    updateDisplay();
    lastRedrawnScreenChange_ = changedAt;
}
```

- [ ] **Step 5: Full build and flash**

```bash
pio run -e full 2>&1 | tail -20
```

Expected: `[SUCCESS]`

```bash
pio run -e full -t upload 2>&1 | tail -20
```

- [ ] **Step 6: Verify on device**

With the device running:
- Screen starts dark navy — confirms color palette loaded
- No aircraft → full-width weather with "NO CONTACTS IN RANGE" watermark
- Aircraft present → radar panel appears right, weather left, blips positioned
- Tap "VIEW AIRCRAFT" → detail screen with dark topbar, callsign, distance/bearing
- Route shows "LOOKING UP..." then resolves to "BOS → LAX" if AeroDataBox key is valid
- Vert speed tile color-codes green/red/white correctly

- [ ] **Step 7: Commit**

```bash
git add src/core/App.h src/core/App.cpp
git commit -m "feat: wire RouteCache into App, route lookup on aircraft detail screen load"
```

---

## Self-Review Checklist

**Spec coverage:**
- §2 Color palette — ✅ Task 1
- §3.1 Shared top bar + status bar — ✅ Tasks 4, 7
- §3.2 Home screen (aircraft present) — ✅ Tasks 5, 6
- §3.3 Home screen (no aircraft) — ✅ Task 7
- §3.4 Aircraft detail — ✅ Task 8
- §4 AeroDataBox route lookup + NVS cache — ✅ Tasks 9, 10
- §5 Haversine distance + bearing — ✅ Task 2
- §6 Blip positioning — ✅ Tasks 2, 6
- §7 Implementation order matches Approach 3 — ✅

**Type consistency:**
- `WeatherWidgets` struct defined in Task 3, used consistently in Tasks 4-7
- `homeWidgets` / `emptyWidgets` declared Task 3, populated Tasks 4-7
- `radar_blips[Config::MAX_AIRCRAFT]` declared Task 3, populated Task 6
- `GeoUtils::blipPosition()` returns `BlipPos{x,y}`, used in Task 6 `update_home_screen()`
- `RouteCache::lookup()` signature matches call in Task 10

**Placeholder scan:** No TBD, TODO, or vague steps present.
