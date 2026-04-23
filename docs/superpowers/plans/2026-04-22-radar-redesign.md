# Radar Redesign — Local Airspace Display Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the embedded 190px radar widget with a two-screen system: a full-screen radar screen (auto-activates when aircraft within 25nm) with a Casco Bay coastline overlay and aircraft list, and a redesigned home screen with weather + quiet airspace status panel.

**Architecture:** Add `GeoUtils::latLonToRadarPx()` for coastline projection, generate `src/data/CoastlinePortland.h` with the static lat/lon array, then build the two new screens in `LVGLDisplayManager` using LVGL line objects for coastline rendering. `App.cpp` switches to `SCREEN_RADAR` on any aircraft detection, back to `SCREEN_HOME` when clear.

**Tech Stack:** LVGL 9.2.2, LovyanGFX 1.1.16, ESP32-S3, PlatformIO, Unity (native tests)

---

## File Map

| File | Action | What changes |
|------|--------|-------------|
| `src/utils/GeoUtils.h` | Modify | Add `latLonToRadarPx()` |
| `src/data/CoastlinePortland.h` | **Create** | Static lat/lon array for Portland ME coastline |
| `src/config/Config.h` | Modify | `RADAR_MAX_RANGE_NM→25`, add `PWM_LAT/LON`, `RADAR_CIRCLE_RADIUS` |
| `src/LVGLDisplayManager.h` | Modify | New enum, new structs (`RadarBlip`, `AircraftListRow`), new member declarations |
| `src/LVGLDisplayManager.cpp` | Modify | New `build_home_screen()`, `buildAirspacePanel()`, `build_radar_screen()`, `update_radar_screen()` |
| `src/core/App.h` | Modify | Remove `currentAircraftIndex_`, `lastPlaneSwitchMs_` |
| `src/core/App.cpp` | Modify | Screen switching, route lookup for all aircraft, remove cycling |
| `test/test_geoutils/test_geoutils.cpp` | Modify | Add `latLonToRadarPx` tests, update range constant |

---

## Task 1: Add `GeoUtils::latLonToRadarPx` + native tests

**Files:**
- Modify: `src/utils/GeoUtils.h`
- Modify: `test/test_geoutils/test_geoutils.cpp`

- [ ] **Step 1: Add `latLonToRadarPx` to GeoUtils.h after the `blipPosition` function**

```cpp
/// Project a geographic lat/lon point onto the radar circle.
/// Equivalent to distanceNm + bearingDeg + blipPosition with zero margin.
/// Use for rendering coastline points and fixed markers.
inline BlipPos latLonToRadarPx(float ptLat, float ptLon,
                                float centerLat, float centerLon,
                                float maxRangeNm, int16_t circleRadius) {
    float dist = distanceNm(centerLat, centerLon, ptLat, ptLon);
    float bear = bearingDeg(centerLat, centerLon, ptLat, ptLon);
    return blipPosition(dist, bear, maxRangeNm, circleRadius, 0);
}
```

- [ ] **Step 2: Write failing tests** — append to `test/test_geoutils/test_geoutils.cpp` before `int main()`:

```cpp
void test_latlon_to_radar_center() {
    // A point at exactly home position maps to circle center (190, 190)
    auto pos = GeoUtils::latLonToRadarPx(43.661f, -70.255f,
                                          43.661f, -70.255f,
                                          25.0f, 190);
    TEST_ASSERT_INT_WITHIN(2, 190, pos.x);
    TEST_ASSERT_INT_WITHIN(2, 190, pos.y);
}

void test_latlon_to_radar_north() {
    // A point due north at half range (12.5nm) lands at (190, 95)
    // bearing=0, dist=12.5, scale=0.5, r=0.5*190=95, x=190+0=190, y=190-95=95
    float northLat = 43.661f + (12.5f / 60.0f);  // approx 12.5nm north
    auto pos = GeoUtils::latLonToRadarPx(northLat, -70.255f,
                                          43.661f, -70.255f,
                                          25.0f, 190);
    TEST_ASSERT_INT_WITHIN(4, 190, pos.x);
    TEST_ASSERT_INT_WITHIN(4, 95,  pos.y);
}
```

- [ ] **Step 3: Register the new tests in `main()`** — add before `return UNITY_END()`:

```cpp
RUN_TEST(test_latlon_to_radar_center);
RUN_TEST(test_latlon_to_radar_north);
```

- [ ] **Step 4: Run tests, confirm they fail**

```bash
cd /Users/michaelballin/Documents/PlatformIO/Projects/Plane-Tracker
pio test -e native
```
Expected: compile error "no member named latLonToRadarPx"

- [ ] **Step 5: Run tests again after adding the function**

```bash
pio test -e native
```
Expected: all tests pass, including the 2 new ones

- [ ] **Step 6: Commit**

```bash
git add src/utils/GeoUtils.h test/test_geoutils/test_geoutils.cpp
git commit -m "feat: add GeoUtils::latLonToRadarPx for coastline projection"
```

---

## Task 2: Generate `src/data/CoastlinePortland.h`

**Files:**
- **Create:** `src/data/CoastlinePortland.h`
- **Create:** `tools/generate_coastline.py` (reference script for future refinement)

- [ ] **Step 1: Create `src/data/` directory and the header**

```bash
mkdir -p /Users/michaelballin/Documents/PlatformIO/Projects/Plane-Tracker/src/data
mkdir -p /Users/michaelballin/Documents/PlatformIO/Projects/Plane-Tracker/tools
```

- [ ] **Step 2: Write `src/data/CoastlinePortland.h`**

```cpp
// src/data/CoastlinePortland.h
// Simplified Maine coastline for Portland area radar display (~25nm radius).
// Traces the land/water boundary SW→NE. Land is to the west/northwest.
// Approximate — suitable for visual context, not navigation.
// To regenerate with higher fidelity, run tools/generate_coastline.py.
#pragma once

struct GeoPoint { float lat; float lon; };

static const GeoPoint COASTLINE_PORTLAND[] = {
    // Southern boundary — Saco / Scarborough area
    {43.498f, -70.450f},
    {43.510f, -70.388f},
    {43.525f, -70.365f},
    // Prouts Neck
    {43.537f, -70.338f},
    {43.551f, -70.323f},
    {43.556f, -70.302f},
    // Crescent Beach / Ocean Ave
    {43.559f, -70.265f},
    {43.561f, -70.234f},
    {43.565f, -70.200f},
    // Cape Elizabeth / Two Lights
    {43.570f, -70.193f},
    {43.581f, -70.193f},
    {43.607f, -70.200f},
    // Portland Head Light
    {43.623f, -70.207f},
    // Inner Casco Bay / Portland harbor
    {43.636f, -70.198f},
    {43.647f, -70.215f},
    {43.657f, -70.223f},
    // Eastern Promenade / Back Cove
    {43.662f, -70.232f},
    {43.668f, -70.237f},
    {43.673f, -70.239f},
    // East Deering / Martin's Point
    {43.681f, -70.234f},
    {43.694f, -70.228f},
    {43.707f, -70.220f},
    // Falmouth Foreside
    {43.720f, -70.218f},
    {43.729f, -70.214f},
    // Falmouth → Yarmouth
    {43.745f, -70.194f},
    {43.756f, -70.180f},
    {43.769f, -70.173f},
    // Yarmouth / Royal River
    {43.785f, -70.178f},
    {43.799f, -70.185f},
    // North toward Freeport (northern boundary)
    {43.814f, -70.192f},
    {43.840f, -70.175f},
    {43.870f, -70.155f},
};

static const int COASTLINE_PORTLAND_LEN =
    static_cast<int>(sizeof(COASTLINE_PORTLAND) / sizeof(COASTLINE_PORTLAND[0]));

// PWM airport position (Portland International Jetport)
constexpr float COASTLINE_PWM_LAT = 43.6462f;
constexpr float COASTLINE_PWM_LON = -70.3093f;
```

- [ ] **Step 3: Write `tools/generate_coastline.py`** (reference only — not run during build)

```python
#!/usr/bin/env python3
"""
Generate src/data/CoastlinePortland.h from Natural Earth 1:10m coastline data.
Requires: pip install shapely requests numpy

Usage: python tools/generate_coastline.py
Output: src/data/CoastlinePortland.h (overwrites existing file)
"""
import json, urllib.request, os

HOME_LAT, HOME_LON = 43.661, -70.255
RANGE_DEG = 0.42  # ~25nm in degrees

# Natural Earth 1:10m land polygons (GeoJSON, clipped to NE US)
# Download from: https://www.naturalearthdata.com/downloads/10m-physical-vectors/
# Or use the OpenStreetMap coastline extract from osmcoastline.
# This script shows the approach; run manually to regenerate.

print("Download ne_10m_land.geojson from Natural Earth, then clip to:")
print(f"  bbox: {HOME_LON-RANGE_DEG:.3f},{HOME_LAT-RANGE_DEG:.3f},"
      f"{HOME_LON+RANGE_DEG:.3f},{HOME_LAT+RANGE_DEG:.3f}")
print("Use Douglas-Peucker simplification (tolerance ~0.001 deg)")
print("Extract exterior ring points and write to src/data/CoastlinePortland.h")
```

- [ ] **Step 4: Commit**

```bash
git add src/data/CoastlinePortland.h tools/generate_coastline.py
git commit -m "feat: add Portland ME coastline data for radar display"
```

---

## Task 3: Update `Config.h`

**Files:**
- Modify: `src/config/Config.h`

- [ ] **Step 1: Update `RADAR_MAX_RANGE_NM` and add new constants** in the `Radar Display` section:

Replace:
```cpp
constexpr float RADAR_MAX_RANGE_NM = 150.0f;
```
With:
```cpp
constexpr float RADAR_MAX_RANGE_NM = 25.0f;
constexpr int16_t RADAR_CIRCLE_RADIUS = 190;   // px, circle diameter = 380
```

- [ ] **Step 2: Add PWM coordinates** in the `Home Location` section, after `HOME_LON`:

```cpp
// Portland International Jetport (PWM) — shown as reference on radar
constexpr float PWM_LAT = 43.6462f;
constexpr float PWM_LON = -70.3093f;
```

- [ ] **Step 3: Build to verify no compile errors**

```bash
pio run -e full --target compiledb 2>&1 | tail -5
```
Expected: exits with 0 errors (or same warnings as before — no new ones)

- [ ] **Step 4: Commit**

```bash
git add src/config/Config.h
git commit -m "config: reduce radar range to 25nm, add RADAR_CIRCLE_RADIUS and PWM coords"
```

---

## Task 4: Restructure `LVGLDisplayManager.h`

**Files:**
- Modify: `src/LVGLDisplayManager.h`

- [ ] **Step 1: Replace the `ScreenState` enum** — remove `SCREEN_AIRCRAFT_DETAIL` and `SCREEN_NO_AIRCRAFT`, keep only two states:

```cpp
enum ScreenState {
    SCREEN_HOME,   // weather + quiet airspace panel
    SCREEN_RADAR,  // full radar + aircraft list
};
```

- [ ] **Step 2: Add new structs** inside the `private:` section, before the existing member variables:

```cpp
// Radar blip on the radar screen (one per MAX_AIRCRAFT slot)
struct RadarBlip {
    lv_obj_t*    dot    = nullptr;
    lv_obj_t*    vector = nullptr;  // lv_line, heading direction
    lv_obj_t*    label  = nullptr;  // callsign text
    lv_point_t   vec_pts[2] = {};   // kept alive for lv_line
};

// One row in the aircraft list panel
struct AircraftListRow {
    lv_obj_t* container      = nullptr;  // tappable row
    lv_obj_t* accent_bar     = nullptr;
    lv_obj_t* label_callsign = nullptr;
    lv_obj_t* label_type_route = nullptr;
    lv_obj_t* label_summary  = nullptr;  // visible when collapsed
    lv_obj_t* expanded_panel = nullptr;  // hidden when collapsed
    lv_obj_t* label_alt      = nullptr;
    lv_obj_t* label_speed    = nullptr;
    lv_obj_t* label_hdg      = nullptr;
    lv_obj_t* label_dist     = nullptr;
    lv_obj_t* label_status   = nullptr;
};
```

- [ ] **Step 3: Replace the screen and widget member variables** — remove the old `screen_home`, `screen_home_empty`, `screen_aircraft`, all old radar panel widgets, all old aircraft detail widgets, and both `WeatherWidgets homeWidgets/emptyWidgets`. Replace with:

```cpp
// --- Screens ---
lv_obj_t* screen_home;
lv_obj_t* screen_radar;

// --- Home screen widgets ---
WeatherWidgets homeWidgets;

// Airspace status panel (on home screen)
lv_obj_t* airspace_circle_    = nullptr;
lv_obj_t* airspace_coastline_ = nullptr;
lv_point_t airspace_pts_[256] = {};      // projected pts for dim coastline
lv_obj_t* label_airspace_status_ = nullptr;
lv_obj_t* label_airspace_sub_    = nullptr;
lv_obj_t* label_airspace_range_  = nullptr;

// --- Radar screen widgets ---
lv_obj_t* radar_circle_     = nullptr;
lv_obj_t* radar_coastline_  = nullptr;   // lv_line, full coastline
lv_point_t radar_pts_[256]  = {};        // projected pts for coastline
lv_obj_t* label_radar_count_ = nullptr;  // "3 AIRCRAFT NEARBY" badge
lv_obj_t* label_radar_time_  = nullptr;
lv_obj_t* label_radar_date_  = nullptr;

RadarBlip   radar_blips_[Config::MAX_AIRCRAFT];
lv_obj_t*   list_container_    = nullptr;
lv_obj_t*   label_list_header_ = nullptr;
AircraftListRow list_rows_[Config::MAX_AIRCRAFT];
int         list_selected_idx_  = -1;
```

- [ ] **Step 4: Replace the old private function declarations** with the new ones:

Remove:
```cpp
void build_home_screen();
void build_home_empty_screen();
void build_aircraft_screen();
void build_no_aircraft_screen();
void buildRadarPanel(lv_obj_t* parent);
void update_aircraft_screen(const Aircraft& aircraft);
```

Add:
```cpp
void build_home_screen();
void buildAirspacePanel(lv_obj_t* parent);
void build_radar_screen();
void update_home_screen(const WeatherData& weather, int aircraftCount);
void update_radar_screen(const Aircraft* aircraft, int aircraftCount);
void onListRowClicked(int idx);
static void event_list_row_clicked(lv_event_t* e);
static void event_topbar_back(lv_event_t* e);
```

- [ ] **Step 5: Remove old state fields** — `homeHasAircraft` is no longer needed. Remove it from the `// --- State ---` section.

- [ ] **Step 6: Build to verify** — this will have many compile errors from the .cpp file (expected), but the header itself should parse:

```bash
pio run -e full 2>&1 | grep "error:" | head -20
```
Expected: errors about undefined functions in the .cpp — none in the .h itself

- [ ] **Step 7: Commit header changes**

```bash
git add src/LVGLDisplayManager.h
git commit -m "refactor: restructure LVGLDisplayManager header for two-screen radar redesign"
```

---

## Task 5: Redesign home screen in LVGLDisplayManager.cpp

**Files:**
- Modify: `src/LVGLDisplayManager.cpp`

This task replaces the two old home screen builders with one clean builder, and adds the `buildAirspacePanel()` function.

- [ ] **Step 1: Remove old `build_home_screen()` entirely** (lines ~607–638 — the version that called `buildRadarPanel()`). Also remove `buildRadarPanel()` (lines ~640–732) and `build_home_empty_screen()` (lines ~783–950) and `build_no_aircraft_screen()` (lines ~953–956).

- [ ] **Step 2: Write the new `build_home_screen()`** — two columns: left weather (flex-grow), right airspace (310px):

```cpp
void LVGLDisplayManager::build_home_screen() {
    screen_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_home, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen_home, LV_OPA_COVER, 0);

    buildTopBar(screen_home, homeWidgets);
    buildStatusBar(screen_home, homeWidgets);

    lv_obj_t* body = lv_obj_create(screen_home);
    lv_obj_set_size(body, hal::Elecrow5Inch::PANEL_WIDTH, 396);
    lv_obj_set_pos(body, 0, 58);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Left: weather + 5-day forecast (flex-fill)
    lv_obj_t* wx_col = lv_obj_create(body);
    lv_obj_set_flex_grow(wx_col, 1);
    lv_obj_set_height(wx_col, 396);
    buildWeatherPanel(wx_col, homeWidgets);

    // Right: airspace status (310px)
    lv_obj_t* ap_col = lv_obj_create(body);
    lv_obj_set_size(ap_col, 310, 396);
    buildAirspacePanel(ap_col);
}
```

- [ ] **Step 3: Write `buildAirspacePanel()`** — dim radar circle + coastline + status labels. Add after `build_home_screen()`:

```cpp
void LVGLDisplayManager::buildAirspacePanel(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(parent, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(parent, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(parent, 1, 0);
    lv_obj_set_style_radius(parent, 0, 0);
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Header
    lv_obj_t* hdr = lv_label_create(parent);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(hdr, "LOCAL AIRSPACE");
    lv_obj_set_pos(hdr, 10, 10);

    label_airspace_range_ = lv_label_create(parent);
    lv_obj_set_style_text_font(label_airspace_range_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_airspace_range_, COLOR_TEXT_DIM, 0);
    lv_label_set_text(label_airspace_range_, "25 nm radius");
    lv_obj_set_pos(label_airspace_range_, 10, 28);

    // Dim radar circle (240px diameter, centered horizontally)
    airspace_circle_ = lv_obj_create(parent);
    lv_obj_set_size(airspace_circle_, 240, 240);
    lv_obj_set_pos(airspace_circle_, 25, 48);
    lv_obj_set_style_radius(airspace_circle_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(airspace_circle_, COLOR_INSET, 0);
    lv_obj_set_style_bg_opa(airspace_circle_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(airspace_circle_, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(airspace_circle_, 1, 0);
    lv_obj_set_style_pad_all(airspace_circle_, 0, 0);
    lv_obj_set_style_clip_corner(airspace_circle_, true, 0);
    lv_obj_clear_flag(airspace_circle_, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

    // Home dot
    lv_obj_t* home = lv_obj_create(airspace_circle_);
    lv_obj_set_size(home, 6, 6);
    lv_obj_center(home);
    lv_obj_set_style_radius(home, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(home, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_border_width(home, 0, 0);
    lv_obj_clear_flag(home, LV_OBJ_FLAG_CLICKABLE);

    // Project coastline once to pixel coords (120px radius = half of 240px circle)
    for (int i = 0; i < COASTLINE_PORTLAND_LEN; i++) {
        auto p = GeoUtils::latLonToRadarPx(
            COASTLINE_PORTLAND[i].lat, COASTLINE_PORTLAND[i].lon,
            Config::HOME_LAT, Config::HOME_LON,
            Config::RADAR_MAX_RANGE_NM, 120);
        airspace_pts_[i] = {(int32_t)p.x, (int32_t)p.y};
    }

    airspace_coastline_ = lv_line_create(airspace_circle_);
    lv_line_set_points(airspace_coastline_, airspace_pts_, COASTLINE_PORTLAND_LEN);
    lv_obj_set_style_line_color(airspace_coastline_, lv_color_hex(0x1e3a54), 0);
    lv_obj_set_style_line_width(airspace_coastline_, 2, 0);
    lv_obj_set_style_line_opa(airspace_coastline_, LV_OPA_COVER, 0);

    // Status labels below circle
    label_airspace_status_ = lv_label_create(parent);
    lv_obj_set_style_text_font(label_airspace_status_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_airspace_status_, COLOR_SUCCESS, 0);
    lv_label_set_text(label_airspace_status_, "\xe2\x97\x8f AIRSPACE CLEAR");
    lv_obj_set_pos(label_airspace_status_, 10, 298);

    label_airspace_sub_ = lv_label_create(parent);
    lv_obj_set_style_text_font(label_airspace_sub_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_airspace_sub_, COLOR_TEXT_DIM, 0);
    lv_label_set_text(label_airspace_sub_, "No aircraft within 25nm");
    lv_obj_set_pos(label_airspace_sub_, 10, 318);
}
```

- [ ] **Step 4: Update `update_home_screen()`** — replace current implementation entirely:

```cpp
void LVGLDisplayManager::update_home_screen(const WeatherData& weather,
                                             int aircraftCount) {
    update_clock(homeWidgets);
    updateWeatherWidgets(homeWidgets, weather, aircraftCount);

    if (!label_airspace_status_) return;

    if (aircraftCount > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "\xe2\x97\x8f %d AIRCRAFT NEARBY", aircraftCount);
        lv_obj_set_style_text_color(label_airspace_status_, COLOR_AMBER, 0);
        lv_label_set_text(label_airspace_status_, buf);
        lv_label_set_text(label_airspace_sub_, "Switching to radar...");
    } else {
        lv_obj_set_style_text_color(label_airspace_status_, COLOR_SUCCESS, 0);
        lv_label_set_text(label_airspace_status_, "\xe2\x97\x8f AIRSPACE CLEAR");
        lv_label_set_text(label_airspace_sub_, "No aircraft within 25nm");
    }
}
```

- [ ] **Step 5: Update `update()` method** — find the current `update()` function and update the `SCREEN_HOME` branch to match the new `update_home_screen()` signature (no aircraft pointer, just count):

Find the section like:
```cpp
if (screen == LVGLDisplayManager::SCREEN_HOME) {
    update_home_screen(weather, aircraft, aircraftCount);
```
Change to:
```cpp
if (screen == LVGLDisplayManager::SCREEN_HOME) {
    update_home_screen(weather, aircraftCount);
```

Also remove the `SCREEN_AIRCRAFT_DETAIL` branch (replace with `SCREEN_RADAR` — handled in Task 9).

- [ ] **Step 6: Fix the constructor** — remove `homeHasAircraft` initialization and the old `radar_blips[]` initialization loop. Update to initialize the new members:

```cpp
, list_selected_idx_(-1)
```

- [ ] **Step 7: Update `initialize()` call sites** — find where `build_home_screen()` and `build_home_empty_screen()` are called in `initialize()`. Replace both calls with a single `build_home_screen()` call. Remove the `lv_screen_load(screen_home_empty)` logic.

- [ ] **Step 8: Build and flash to hardware; verify home screen looks correct**

```bash
pio run -e full --target upload
```
Check: weather panel visible, airspace panel on the right with dim circle and "AIRSPACE CLEAR".

- [ ] **Step 9: Commit**

```bash
git add src/LVGLDisplayManager.cpp src/LVGLDisplayManager.h
git commit -m "feat: redesign home screen with weather panel + airspace status panel"
```

---

## Task 6: Build radar screen shell (circle, coastline, PWM, home dot)

**Files:**
- Modify: `src/LVGLDisplayManager.cpp`

- [ ] **Step 1: Add the `build_radar_screen()` function** after `buildAirspacePanel()`. This builds the full-screen radar layout:

```cpp
void LVGLDisplayManager::build_radar_screen() {
    screen_radar = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_radar, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen_radar, LV_OPA_COVER, 0);

    // === TOP BAR (58px) ===
    lv_obj_t* topbar = lv_obj_create(screen_radar);
    lv_obj_set_size(topbar, hal::Elecrow5Inch::PANEL_WIDTH, 58);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar, COLOR_TOPBAR, 0);
    lv_obj_set_style_border_side(topbar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(topbar, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(topbar, 1, 0);
    lv_obj_set_style_radius(topbar, 0, 0);
    lv_obj_set_style_pad_all(topbar, 0, 0);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);
    // Tap topbar → go back home
    lv_obj_add_event_cb(topbar, event_topbar_back, LV_EVENT_CLICKED, this);

    label_radar_time_ = lv_label_create(topbar);
    lv_obj_set_style_text_font(label_radar_time_, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(label_radar_time_, COLOR_ACCENT, 0);
    lv_label_set_text(label_radar_time_, "--:--");
    lv_obj_align(label_radar_time_, LV_ALIGN_LEFT_MID, 16, -6);

    label_radar_date_ = lv_label_create(topbar);
    lv_obj_set_style_text_font(label_radar_date_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_radar_date_, COLOR_TEXT_DIM, 0);
    lv_label_set_text(label_radar_date_, "--- -- ----");
    lv_obj_align(label_radar_date_, LV_ALIGN_LEFT_MID, 16, 10);

    // Aircraft count badge (center)
    label_radar_count_ = lv_label_create(topbar);
    lv_obj_set_style_text_font(label_radar_count_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_radar_count_, COLOR_AMBER, 0);
    lv_label_set_text(label_radar_count_, "\xe2\x97\x8f 0 AIRCRAFT NEARBY");
    lv_obj_align(label_radar_count_, LV_ALIGN_CENTER, 0, 0);

    // "tap to go home" hint right side
    lv_obj_t* hint = lv_label_create(topbar);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_DIM, 0);
    lv_label_set_text(hint, "tap bar \xe2\x86\x90 home");
    lv_obj_align(hint, LV_ALIGN_RIGHT_MID, -16, 0);

    // === BODY ===
    lv_obj_t* body = lv_obj_create(screen_radar);
    lv_obj_set_size(body, hal::Elecrow5Inch::PANEL_WIDTH, 396);
    lv_obj_set_pos(body, 0, 58);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Left panel: radar (490px)
    lv_obj_t* radar_col = lv_obj_create(body);
    lv_obj_set_size(radar_col, 490, 396);
    lv_obj_set_style_bg_opa(radar_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(radar_col, 0, 0);
    lv_obj_set_style_pad_all(radar_col, 0, 0);
    lv_obj_clear_flag(radar_col, LV_OBJ_FLAG_SCROLLABLE);

    // Radar circle: 380px diameter, centered in 490px column
    // Center at (245, 256) in body coords = (55, 8) in radar_col coords
    radar_circle_ = lv_obj_create(radar_col);
    lv_obj_set_size(radar_circle_, 380, 380);
    lv_obj_set_pos(radar_circle_, 55, 8);
    lv_obj_set_style_radius(radar_circle_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(radar_circle_, COLOR_INSET, 0);
    lv_obj_set_style_bg_opa(radar_circle_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(radar_circle_, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(radar_circle_, 1, 0);
    lv_obj_set_style_pad_all(radar_circle_, 0, 0);
    lv_obj_set_style_clip_corner(radar_circle_, true, 0);
    lv_obj_clear_flag(radar_circle_, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

    // Inner range ring (12.5nm = half radius = 190px)
    lv_obj_t* ring = lv_obj_create(radar_circle_);
    lv_obj_set_size(ring, 190, 190);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(0x1a3048), 0);
    lv_obj_set_style_border_width(ring, 1, 0);
    lv_obj_clear_flag(ring, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

    // Cardinal labels (N/S/E/W) — placed at circle edge
    const struct { const char* txt; lv_align_t align; int ox; int oy; } cards[4] = {
        {"N", LV_ALIGN_TOP_MID,    0,  4},
        {"S", LV_ALIGN_BOTTOM_MID, 0, -4},
        {"E", LV_ALIGN_RIGHT_MID, -6,  0},
        {"W", LV_ALIGN_LEFT_MID,   6,  0},
    };
    for (auto& c : cards) {
        lv_obj_t* lbl = lv_label_create(radar_circle_);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_label_set_text(lbl, c.txt);
        lv_obj_align(lbl, c.align, c.ox, c.oy);
    }

    // Project coastline to pixel coords once (circleRadius=190)
    for (int i = 0; i < COASTLINE_PORTLAND_LEN; i++) {
        auto p = GeoUtils::latLonToRadarPx(
            COASTLINE_PORTLAND[i].lat, COASTLINE_PORTLAND[i].lon,
            Config::HOME_LAT, Config::HOME_LON,
            Config::RADAR_MAX_RANGE_NM, Config::RADAR_CIRCLE_RADIUS);
        radar_pts_[i] = {(int32_t)p.x, (int32_t)p.y};
    }

    radar_coastline_ = lv_line_create(radar_circle_);
    lv_line_set_points(radar_coastline_, radar_pts_, COASTLINE_PORTLAND_LEN);
    lv_obj_set_style_line_color(radar_coastline_, lv_color_hex(0x2a5f8a), 0);
    lv_obj_set_style_line_width(radar_coastline_, 2, 0);
    lv_obj_set_style_line_opa(radar_coastline_, LV_OPA_COVER, 0);

    // PWM marker: cross at PWM lat/lon
    auto pwm = GeoUtils::latLonToRadarPx(Config::PWM_LAT, Config::PWM_LON,
                                           Config::HOME_LAT, Config::HOME_LON,
                                           Config::RADAR_MAX_RANGE_NM,
                                           Config::RADAR_CIRCLE_RADIUS);
    static lv_point_t pwm_h[2], pwm_v[2];
    pwm_h[0] = {(int32_t)(pwm.x - 7), (int32_t)pwm.y};
    pwm_h[1] = {(int32_t)(pwm.x + 7), (int32_t)pwm.y};
    pwm_v[0] = {(int32_t)pwm.x, (int32_t)(pwm.y - 7)};
    pwm_v[1] = {(int32_t)pwm.x, (int32_t)(pwm.y + 7)};

    lv_obj_t* ph = lv_line_create(radar_circle_);
    lv_line_set_points(ph, pwm_h, 2);
    lv_obj_set_style_line_color(ph, COLOR_AMBER, 0);
    lv_obj_set_style_line_width(ph, 2, 0);

    lv_obj_t* pv = lv_line_create(radar_circle_);
    lv_line_set_points(pv, pwm_v, 2);
    lv_obj_set_style_line_color(pv, COLOR_AMBER, 0);
    lv_obj_set_style_line_width(pv, 2, 0);

    lv_obj_t* pwm_lbl = lv_label_create(radar_circle_);
    lv_obj_set_style_text_font(pwm_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pwm_lbl, COLOR_AMBER, 0);
    lv_label_set_text(pwm_lbl, "PWM");
    lv_obj_set_pos(pwm_lbl, pwm.x + 9, pwm.y - 8);

    // Home dot (center, 10px cyan with outer ring)
    lv_obj_t* home_dot = lv_obj_create(radar_circle_);
    lv_obj_set_size(home_dot, 10, 10);
    lv_obj_center(home_dot);
    lv_obj_set_style_radius(home_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(home_dot, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(home_dot, 0, 0);
    lv_obj_clear_flag(home_dot, LV_OBJ_FLAG_CLICKABLE);

    // Range labels
    lv_obj_t* rl1 = lv_label_create(radar_col);
    lv_obj_set_style_text_font(rl1, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(rl1, COLOR_TEXT_DIM, 0);
    lv_label_set_text(rl1, "12.5nm");
    lv_obj_set_pos(rl1, 263, 60);  // just inside inner ring, near N

    lv_obj_t* rl2 = lv_label_create(radar_col);
    lv_obj_set_style_text_font(rl2, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(rl2, COLOR_TEXT_DIM, 0);
    lv_label_set_text(rl2, "25nm");
    lv_obj_set_pos(rl2, 263, 15);  // just inside outer ring, near N

    // === STATUS BAR (26px) ===
    lv_obj_t* sbar = lv_obj_create(screen_radar);
    lv_obj_set_size(sbar, hal::Elecrow5Inch::PANEL_WIDTH, 26);
    lv_obj_align(sbar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(sbar, COLOR_STATUSBAR, 0);
    lv_obj_set_style_border_side(sbar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(sbar, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(sbar, 1, 0);
    lv_obj_set_style_radius(sbar, 0, 0);
    lv_obj_set_style_pad_all(sbar, 0, 0);
    lv_obj_clear_flag(sbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* sbar_left = lv_label_create(sbar);
    lv_obj_set_style_text_font(sbar_left, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sbar_left, COLOR_TEXT_DIM, 0);
    lv_label_set_text(sbar_left, "OPENSKY OK");
    lv_obj_align(sbar_left, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t* sbar_hint = lv_label_create(sbar);
    lv_obj_set_style_text_font(sbar_hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sbar_hint, COLOR_TEXT_DIM, 0);
    lv_label_set_text(sbar_hint, "tap top bar to return home");
    lv_obj_align(sbar_hint, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* sbar_live = lv_label_create(sbar);
    lv_obj_set_style_text_font(sbar_live, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sbar_live, COLOR_SUCCESS, 0);
    lv_label_set_text(sbar_live, "\xe2\x97\x8f LIVE");
    lv_obj_align(sbar_live, LV_ALIGN_RIGHT_MID, -12, 0);
}
```

- [ ] **Step 2: Add the `event_topbar_back` static callback** near the other static event callbacks:

```cpp
void LVGLDisplayManager::event_topbar_back(lv_event_t* e) {
    if (s_instance) s_instance->userDismissed_ = true;
}
```

- [ ] **Step 3: Register `build_radar_screen()` call in `initialize()`** — after the home screen build call:

```cpp
build_radar_screen();
```

- [ ] **Step 4: Update `setScreen()`** — add `SCREEN_RADAR` case:

Find the `setScreen()` function and add:
```cpp
case SCREEN_RADAR:
    lv_screen_load_anim(screen_radar, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    break;
```

- [ ] **Step 5: Build and flash — verify radar screen appears on startup (or add a temporary `setScreen(SCREEN_RADAR)` call in `initialize()` to test)**

```bash
pio run -e full --target upload
```
Check: radar circle visible with coastline, PWM marker, N/S/E/W labels, range rings. The coastline should approximate the Maine coast shape.

- [ ] **Step 6: Commit**

```bash
git add src/LVGLDisplayManager.cpp
git commit -m "feat: build radar screen shell with coastline, PWM, range rings"
```

---

## Task 7: Add aircraft blip widgets to radar screen

**Files:**
- Modify: `src/LVGLDisplayManager.cpp`

- [ ] **Step 1: Add blip pre-allocation to `build_radar_screen()`** — append inside the function, after the range labels, still within the left-column radar section:

```cpp
    // Pre-allocate aircraft blip objects (all hidden)
    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        RadarBlip& b = radar_blips_[i];

        // Dot
        b.dot = lv_obj_create(radar_circle_);
        lv_obj_set_size(b.dot, 12, 12);
        lv_obj_set_style_radius(b.dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(b.dot, COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(b.dot, 0, 0);
        lv_obj_set_pos(b.dot, Config::RADAR_CIRCLE_RADIUS - 6,
                               Config::RADAR_CIRCLE_RADIUS - 6);
        lv_obj_add_flag(b.dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(b.dot, LV_OBJ_FLAG_CLICKABLE);

        // Heading vector (lv_line, 2 points)
        b.vec_pts[0] = {Config::RADAR_CIRCLE_RADIUS, Config::RADAR_CIRCLE_RADIUS};
        b.vec_pts[1] = {Config::RADAR_CIRCLE_RADIUS, Config::RADAR_CIRCLE_RADIUS - 20};
        b.vector = lv_line_create(radar_circle_);
        lv_line_set_points(b.vector, b.vec_pts, 2);
        lv_obj_set_style_line_color(b.vector, COLOR_ACCENT, 0);
        lv_obj_set_style_line_width(b.vector, 2, 0);
        lv_obj_add_flag(b.vector, LV_OBJ_FLAG_HIDDEN);

        // Callsign label
        b.label = lv_label_create(radar_circle_);
        lv_obj_set_style_text_font(b.label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(b.label, COLOR_ACCENT, 0);
        lv_label_set_text(b.label, "");
        lv_obj_set_pos(b.label, Config::RADAR_CIRCLE_RADIUS,
                                Config::RADAR_CIRCLE_RADIUS);
        lv_obj_add_flag(b.label, LV_OBJ_FLAG_HIDDEN);
    }
```

- [ ] **Step 2: Build and flash — blips are hidden at startup, no visual change expected**

```bash
pio run -e full --target upload
```

- [ ] **Step 3: Commit**

```bash
git add src/LVGLDisplayManager.cpp
git commit -m "feat: pre-allocate radar blip widgets with heading vectors and labels"
```

---

## Task 8: Build aircraft list panel

**Files:**
- Modify: `src/LVGLDisplayManager.cpp`

- [ ] **Step 1: Add the right panel (aircraft list) to `build_radar_screen()`** — after the `radar_col` creation but still inside `build_radar_screen()`, append the list panel:

```cpp
    // Right panel: aircraft list (310px)
    lv_obj_t* list_col = lv_obj_create(body);
    lv_obj_set_size(list_col, 310, 396);
    lv_obj_set_style_bg_color(list_col, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(list_col, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(list_col, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(list_col, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(list_col, 1, 0);
    lv_obj_set_style_radius(list_col, 0, 0);
    lv_obj_set_style_pad_all(list_col, 0, 0);
    lv_obj_clear_flag(list_col, LV_OBJ_FLAG_SCROLLABLE);

    // Header label
    label_list_header_ = lv_label_create(list_col);
    lv_obj_set_style_text_font(label_list_header_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_list_header_, COLOR_TEXT_DIM, 0);
    lv_label_set_text(label_list_header_, "AIRCRAFT IN RANGE  \xc2\xb7  0");
    lv_obj_set_pos(label_list_header_, 12, 8);

    // Divider
    lv_obj_t* ldiv = lv_obj_create(list_col);
    lv_obj_set_size(ldiv, 286, 1);
    lv_obj_set_pos(ldiv, 12, 26);
    lv_obj_set_style_bg_color(ldiv, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(ldiv, 0, 0);
    lv_obj_clear_flag(ldiv, LV_OBJ_FLAG_CLICKABLE);

    // Scrollable list container
    list_container_ = lv_obj_create(list_col);
    lv_obj_set_size(list_container_, 310, 362);
    lv_obj_set_pos(list_container_, 0, 30);
    lv_obj_set_style_bg_opa(list_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_container_, 0, 0);
    lv_obj_set_style_pad_all(list_container_, 0, 0);
    lv_obj_set_style_radius(list_container_, 0, 0);
    lv_obj_set_layout(list_container_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_container_, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(list_container_, 0, 0);

    // Pre-allocate rows
    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        AircraftListRow& row = list_rows_[i];

        // Row container
        row.container = lv_obj_create(list_container_);
        lv_obj_set_width(row.container, 310);
        lv_obj_set_height(row.container, 66);
        lv_obj_set_style_bg_color(row.container, COLOR_PANEL, 0);
        lv_obj_set_style_bg_opa(row.container, LV_OPA_COVER, 0);
        lv_obj_set_style_border_side(row.container, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row.container, COLOR_BORDER, 0);
        lv_obj_set_style_border_width(row.container, 1, 0);
        lv_obj_set_style_radius(row.container, 0, 0);
        lv_obj_set_style_pad_all(row.container, 0, 0);
        lv_obj_clear_flag(row.container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row.container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row.container, event_list_row_clicked,
                            LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_flag(row.container, LV_OBJ_FLAG_HIDDEN);

        // Left accent bar (4px)
        row.accent_bar = lv_obj_create(row.container);
        lv_obj_set_size(row.accent_bar, 4, 66);
        lv_obj_set_pos(row.accent_bar, 0, 0);
        lv_obj_set_style_bg_color(row.accent_bar, COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(row.accent_bar, 0, 0);
        lv_obj_set_style_radius(row.accent_bar, 0, 0);
        lv_obj_clear_flag(row.accent_bar, LV_OBJ_FLAG_CLICKABLE);

        // Callsign
        row.label_callsign = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_callsign, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(row.label_callsign, COLOR_ACCENT, 0);
        lv_label_set_text(row.label_callsign, "------");
        lv_obj_set_pos(row.label_callsign, 14, 8);

        // Type · route
        row.label_type_route = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_type_route, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_type_route, COLOR_TEXT_SECONDARY, 0);
        lv_label_set_text(row.label_type_route, "");
        lv_obj_set_pos(row.label_type_route, 14, 30);

        // Summary (collapsed view: alt · speed · heading · dist)
        row.label_summary = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_summary, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_summary, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_summary, "");
        lv_obj_set_pos(row.label_summary, 14, 48);

        // Expanded panel (hidden by default, shown when row is selected)
        row.expanded_panel = lv_obj_create(row.container);
        lv_obj_set_size(row.expanded_panel, 306, 72);
        lv_obj_set_pos(row.expanded_panel, 0, 66);
        lv_obj_set_style_bg_color(row.expanded_panel, lv_color_hex(0x0a1428), 0);
        lv_obj_set_style_bg_opa(row.expanded_panel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row.expanded_panel, 0, 0);
        lv_obj_set_style_radius(row.expanded_panel, 0, 0);
        lv_obj_set_style_pad_hor(row.expanded_panel, 14, 0);
        lv_obj_set_style_pad_ver(row.expanded_panel, 6, 0);
        lv_obj_clear_flag(row.expanded_panel,
                          (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
        lv_obj_add_flag(row.expanded_panel, LV_OBJ_FLAG_HIDDEN);

        // Expanded fields: alt / speed / hdg on row 1
        row.label_alt = lv_label_create(row.expanded_panel);
        lv_obj_set_style_text_font(row.label_alt, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_alt, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(row.label_alt, "--,--- ft");
        lv_obj_set_pos(row.label_alt, 0, 4);

        row.label_speed = lv_label_create(row.expanded_panel);
        lv_obj_set_style_text_font(row.label_speed, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_speed, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(row.label_speed, "--- kt");
        lv_obj_set_pos(row.label_speed, 100, 4);

        row.label_hdg = lv_label_create(row.expanded_panel);
        lv_obj_set_style_text_font(row.label_hdg, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_hdg, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(row.label_hdg, "---\xc2\xb0");
        lv_obj_set_pos(row.label_hdg, 195, 4);

        // dist / status on row 2
        row.label_dist = lv_label_create(row.expanded_panel);
        lv_obj_set_style_text_font(row.label_dist, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_dist, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(row.label_dist, "-.- nm");
        lv_obj_set_pos(row.label_dist, 0, 34);

        row.label_status = lv_label_create(row.expanded_panel);
        lv_obj_set_style_text_font(row.label_status, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_status, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_status, "");
        lv_obj_set_pos(row.label_status, 100, 36);
    }
```

- [ ] **Step 2: Add the `event_list_row_clicked` static callback and `onListRowClicked()` method** near the other event callbacks:

```cpp
void LVGLDisplayManager::event_list_row_clicked(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_instance) s_instance->onListRowClicked(idx);
}

void LVGLDisplayManager::onListRowClicked(int idx) {
    if (idx < 0 || idx >= Config::MAX_AIRCRAFT) return;

    if (list_selected_idx_ == idx) {
        // Tap selected row again → collapse it
        lv_obj_add_flag(list_rows_[idx].expanded_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(list_rows_[idx].container, 66);
        list_selected_idx_ = -1;
        // De-highlight radar blip
        if (radar_blips_[idx].dot)
            lv_obj_set_style_border_width(radar_blips_[idx].dot, 0, 0);
        return;
    }

    // Collapse previously selected
    if (list_selected_idx_ >= 0 && list_selected_idx_ < Config::MAX_AIRCRAFT) {
        int prev = list_selected_idx_;
        lv_obj_add_flag(list_rows_[prev].expanded_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(list_rows_[prev].container, 66);
        if (radar_blips_[prev].dot)
            lv_obj_set_style_border_width(radar_blips_[prev].dot, 0, 0);
    }

    // Expand new row
    lv_obj_remove_flag(list_rows_[idx].expanded_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(list_rows_[idx].container, 138);  // 66 header + 72 expanded
    list_selected_idx_ = idx;

    // Highlight corresponding radar blip with selection ring
    if (radar_blips_[idx].dot) {
        lv_obj_set_style_border_color(radar_blips_[idx].dot, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_border_width(radar_blips_[idx].dot, 2, 0);
        lv_obj_set_style_border_opa(radar_blips_[idx].dot, 160, 0);
    }
}
```

- [ ] **Step 3: Build and flash — the list panel should appear as an empty right column**

```bash
pio run -e full --target upload
```
Check: right panel visible with "AIRCRAFT IN RANGE · 0" header. No rows visible (all hidden).

- [ ] **Step 4: Commit**

```bash
git add src/LVGLDisplayManager.cpp
git commit -m "feat: add aircraft list panel with pre-allocated rows and expand/collapse"
```

---

## Task 9: Implement `update_radar_screen()`

**Files:**
- Modify: `src/LVGLDisplayManager.cpp`

- [ ] **Step 1: Add `update_radar_screen()` after `update_home_screen()`**:

```cpp
void LVGLDisplayManager::update_radar_screen(const Aircraft* aircraft,
                                               int aircraftCount) {
    if (!label_radar_count_) return;

    // Update top bar
    {
        struct tm ti;
        if (getLocalTime(&ti)) {
            char tb[12];
            strftime(tb, sizeof(tb), "%H:%M", &ti);
            lv_label_set_text(label_radar_time_, tb);
            char db[20];
            strftime(db, sizeof(db), "%a %d %b %Y", &ti);
            lv_label_set_text(label_radar_date_, db);
        }
        char cb[32];
        snprintf(cb, sizeof(cb), "\xe2\x97\x8f %d AIRCRAFT NEARBY", aircraftCount);
        lv_label_set_text(label_radar_count_, cb);
    }

    // Update list header
    {
        char hb[32];
        snprintf(hb, sizeof(hb), "AIRCRAFT IN RANGE  \xc2\xb7  %d", aircraftCount);
        lv_label_set_text(label_list_header_, hb);
    }

    // Clamp selection to valid range
    if (list_selected_idx_ >= aircraftCount) {
        if (list_selected_idx_ >= 0 && list_selected_idx_ < Config::MAX_AIRCRAFT) {
            lv_obj_add_flag(list_rows_[list_selected_idx_].expanded_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_height(list_rows_[list_selected_idx_].container, 66);
        }
        list_selected_idx_ = (aircraftCount > 0) ? 0 : -1;
        if (list_selected_idx_ == 0) {
            lv_obj_remove_flag(list_rows_[0].expanded_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_height(list_rows_[0].container, 138);
        }
    }

    // Auto-expand row 0 on first entry (no prior selection)
    if (aircraftCount > 0 && list_selected_idx_ < 0) {
        list_selected_idx_ = 0;
        lv_obj_remove_flag(list_rows_[0].expanded_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(list_rows_[0].container, 138);
    }

    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        RadarBlip&       blip = radar_blips_[i];
        AircraftListRow& row  = list_rows_[i];

        if (!blip.dot || !row.container) continue;

        if (aircraft && i < aircraftCount && aircraft[i].valid) {
            const Aircraft& a = aircraft[i];

            // --- Radar blip ---
            float distNm  = GeoUtils::distanceNm(Config::HOME_LAT, Config::HOME_LON,
                                                   a.latitude, a.longitude);
            float bearing = GeoUtils::bearingDeg(Config::HOME_LAT, Config::HOME_LON,
                                                  a.latitude, a.longitude);
            auto pos = GeoUtils::blipPosition(distNm, bearing,
                                               Config::RADAR_MAX_RANGE_NM,
                                               Config::RADAR_CIRCLE_RADIUS, 8);

            lv_obj_set_pos(blip.dot, pos.x - 6, pos.y - 6);

            float altFt = a.altitude * 3.28084f;
            lv_color_t blipColor = (altFt > 0.0f && altFt < 5000.0f)
                                   ? COLOR_AMBER : COLOR_ACCENT;
            lv_obj_set_style_bg_color(blip.dot, blipColor, 0);
            lv_obj_set_style_line_color(blip.vector, blipColor, 0);
            lv_obj_set_style_text_color(blip.label, blipColor, 0);
            lv_obj_remove_flag(blip.dot, LV_OBJ_FLAG_HIDDEN);

            // Heading vector (20px, from blip center)
            float rad = a.heading * GeoUtils::DEG_TO_RAD;
            int cx = pos.x;
            int cy = pos.y;
            blip.vec_pts[0] = {(int32_t)cx, (int32_t)cy};
            blip.vec_pts[1] = {
                (int32_t)(cx + 20.0f * sinf(rad)),
                (int32_t)(cy - 20.0f * cosf(rad))
            };
            lv_line_set_points(blip.vector, blip.vec_pts, 2);
            lv_obj_remove_flag(blip.vector, LV_OBJ_FLAG_HIDDEN);

            // Callsign label (offset right of blip)
            lv_label_set_text(blip.label, a.callsign.c_str());
            lv_obj_set_pos(blip.label, pos.x + 10, pos.y - 8);
            lv_obj_remove_flag(blip.label, LV_OBJ_FLAG_HIDDEN);

            // Selection ring for selected row
            if (i == list_selected_idx_) {
                lv_obj_set_style_border_color(blip.dot, COLOR_TEXT_PRIMARY, 0);
                lv_obj_set_style_border_width(blip.dot, 2, 0);
                lv_obj_set_style_border_opa(blip.dot, 160, 0);
            } else {
                lv_obj_set_style_border_width(blip.dot, 0, 0);
            }

            // --- List row ---
            lv_obj_remove_flag(row.container, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(row.accent_bar, blipColor, 0);
            lv_obj_set_style_text_color(row.label_callsign, blipColor, 0);
            lv_label_set_text(row.label_callsign, a.callsign.c_str());

            // Type · Route
            char tr[64];
            if (!a.aircraftType.isEmpty() && !a.origin.isEmpty() && !a.destination.isEmpty()) {
                snprintf(tr, sizeof(tr), "%s  \xc2\xb7  %s \xe2\x86\x92 %s",
                         a.aircraftType.c_str(), a.origin.c_str(), a.destination.c_str());
            } else if (!a.origin.isEmpty() && !a.destination.isEmpty()) {
                snprintf(tr, sizeof(tr), "%s \xe2\x86\x92 %s",
                         a.origin.c_str(), a.destination.c_str());
            } else {
                snprintf(tr, sizeof(tr), "%s", a.aircraftType.isEmpty()
                                               ? "Unknown" : a.aircraftType.c_str());
            }
            lv_label_set_text(row.label_type_route, tr);

            // Summary line (collapsed)
            char sm[64];
            snprintf(sm, sizeof(sm), "%.0f ft  \xc2\xb7  %.0f kt  \xc2\xb7  %s  \xc2\xb7  %.1f nm",
                     altFt, a.velocity * 1.94384f,   // m/s → kt
                     GeoUtils::cardinalDir(bearing), distNm);
            lv_label_set_text(row.label_summary, sm);

            // Expanded fields
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f ft", altFt);
            lv_label_set_text(row.label_alt, buf);

            snprintf(buf, sizeof(buf), "%.0f kt", a.velocity * 1.94384f);
            lv_label_set_text(row.label_speed, buf);

            snprintf(buf, sizeof(buf), "%.0f\xc2\xb0 %s",
                     a.heading, GeoUtils::cardinalDir(a.heading));
            lv_label_set_text(row.label_hdg, buf);

            snprintf(buf, sizeof(buf), "%.1f nm %s",
                     distNm, GeoUtils::cardinalDir(bearing));
            lv_label_set_text(row.label_dist, buf);

            // Status label
            const char* statusTxt;
            float distToPwmNm = GeoUtils::distanceNm(Config::PWM_LAT, Config::PWM_LON,
                                                       a.latitude, a.longitude);
            float bearingToPwm = GeoUtils::bearingDeg(a.latitude, a.longitude,
                                                       Config::PWM_LAT, Config::PWM_LON);
            float hdgDiff = fabsf(fmodf(a.heading - bearingToPwm + 360.0f, 360.0f));
            if (hdgDiff > 180.0f) hdgDiff = 360.0f - hdgDiff;

            if (altFt >= 5000.0f) {
                statusTxt = "CRUISING";
            } else if (distToPwmNm < 15.0f && hdgDiff < 30.0f) {
                statusTxt = "ON APPROACH";
            } else if (distToPwmNm < 15.0f) {
                statusTxt = "DEPARTING";
            } else {
                statusTxt = "LOW / OVERFLYING";
            }
            lv_obj_set_style_text_color(row.label_status, blipColor, 0);
            lv_label_set_text(row.label_status, statusTxt);

        } else {
            // Hide blip and row
            lv_obj_add_flag(blip.dot,    LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(blip.vector, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(blip.label,  LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(row.container, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
```

- [ ] **Step 2: Update the `update()` dispatch method** — find the `SCREEN_HOME` / `SCREEN_AIRCRAFT_DETAIL` branching and replace with:

```cpp
void LVGLDisplayManager::update(const WeatherData& weather,
                                  const Aircraft* aircraft, int aircraftCount) {
    if (currentScreen == SCREEN_HOME) {
        update_home_screen(weather, aircraftCount);
    } else if (currentScreen == SCREEN_RADAR) {
        update_radar_screen(aircraft, aircraftCount);
    }
}
```

- [ ] **Step 3: Build**

```bash
pio run -e full 2>&1 | grep "error:" | head -10
```
Expected: 0 errors

- [ ] **Step 4: Flash and manually trigger the radar screen** — temporarily add `setScreen(SCREEN_RADAR)` after `build_radar_screen()` in `initialize()` and call `update_radar_screen()` with fake data to verify rendering:

After verifying visually, remove the temporary call.

- [ ] **Step 5: Commit**

```bash
git add src/LVGLDisplayManager.cpp
git commit -m "feat: implement update_radar_screen with blips, vectors, and list rows"
```

---

## Task 10: Update `App.cpp` screen switching

**Files:**
- Modify: `src/core/App.cpp`
- Modify: `src/core/App.h`

- [ ] **Step 1: Remove `currentAircraftIndex_` and `lastPlaneSwitchMs_` from `App.h`** — delete those two members from the private section.

- [ ] **Step 2: Remove their initialization from the `App::App()` constructor** in `App.cpp`.

- [ ] **Step 3: Replace the screen-switching block in `App::tick()`**

Find this block (roughly lines 124–155):
```cpp
if (display_->wasUserDismissed()) {
    aircraftDismissed_ = true;
}
// ... existing aircraft detection / switching logic ...
if (display_->shouldReturnToHome() && currentAircraftCount_ == 0) {
    display_->setScreen(LVGLDisplayManager::SCREEN_HOME);
}
```

Replace entirely with:
```cpp
if (display_->wasUserDismissed()) {
    aircraftDismissed_ = true;
}

// Reset dismissed flag once the sky clears
if (currentAircraftCount_ == 0) {
    aircraftDismissed_ = false;
}

// Auto-switch to radar when aircraft detected; back to home when clear
if (currentAircraftCount_ > 0 && !aircraftDismissed_
        && display_->getCurrentScreen() != LVGLDisplayManager::SCREEN_RADAR) {
    display_->setScreen(LVGLDisplayManager::SCREEN_RADAR);
    list_selected_reset_ = true;
}

if (currentAircraftCount_ == 0
        && display_->getCurrentScreen() == LVGLDisplayManager::SCREEN_RADAR) {
    display_->setScreen(LVGLDisplayManager::SCREEN_HOME);
}
```

Note: `list_selected_reset_` is a local bool flag — you can just reset it inline via a display call. Since `update_radar_screen()` already handles auto-expanding row 0 when `list_selected_idx_` is -1, we don't need extra state here. Simplify: just remove the `list_selected_reset_` line.

- [ ] **Step 4: Remove the auto-cycling block** from `App::tick()`:

Delete:
```cpp
if (display_ && display_->getCurrentScreen() == LVGLDisplayManager::SCREEN_AIRCRAFT_DETAIL) {
    if (currentAircraftCount_ > 1 && (now - lastPlaneSwitchMs_) >= Config::PLANE_DISPLAY_TIME) {
        currentAircraftIndex_ = (currentAircraftIndex_ + 1) % currentAircraftCount_;
        lastPlaneSwitchMs_ = now;
        routeFetchDone_ = false;
        lastRouteFetchCallsign_ = "";
    }
}
```

- [ ] **Step 5: Update `updateDisplay()` in `App.cpp`** — replace the `SCREEN_AIRCRAFT_DETAIL` branch:

```cpp
void App::updateDisplay() {
    if (!display_) return;

    LVGLDisplayManager::ScreenState screen = display_->getCurrentScreen();

    if (screen == LVGLDisplayManager::SCREEN_HOME) {
        display_->update(currentWeather_, nullptr, currentAircraftCount_);
        return;
    }

    if (screen == LVGLDisplayManager::SCREEN_RADAR) {
        display_->update(currentWeather_, aircraftList_, currentAircraftCount_);
        return;
    }

    display_->update(currentWeather_, nullptr, 0);
}
```

- [ ] **Step 6: Update route lookup in `updateDisplay()`** — route fetches now happen for all valid aircraft, not just one. Replace the old per-index lookup with a loop:

```cpp
    if (screen == LVGLDisplayManager::SCREEN_RADAR) {
        // Trigger route lookup for any aircraft that hasn't been looked up yet
        if (routeCache_ && wifiManager_.isConnected()) {
            for (int i = 0; i < currentAircraftCount_; i++) {
                Aircraft& a = aircraftList_[i];
                if (!a.valid || a.callsign.isEmpty() || a.routeLookupDone) continue;
                String org, dst, orgCity, orgCountry, dstCity, dstCountry;
                if (routeCache_->lookup(a.callsign, org, dst, orgCity, orgCountry,
                                         dstCity, dstCountry)) {
                    a.origin      = org;
                    a.destination = dst;
                    a.originDisplay      = orgCity.isEmpty() ? org : orgCity;
                    a.destinationDisplay = dstCity.isEmpty() ? dst : dstCity;
                }
                a.routeLookupDone = true;
                break;  // one per update cycle — don't block the main loop
            }
        }
        display_->update(currentWeather_, aircraftList_, currentAircraftCount_);
        return;
    }
```

- [ ] **Step 7: Remove now-unused fields** from `App.h` — `currentAircraftIndex_` and `lastPlaneSwitchMs_` were already removed in Step 1. Also remove `routeFetchDone_` and `lastRouteFetchCallsign_` since the new per-aircraft `routeLookupDone` flag in the `Aircraft` struct handles state.

- [ ] **Step 8: Build**

```bash
pio run -e full 2>&1 | grep "error:" | head -10
```
Expected: 0 errors

- [ ] **Step 9: Flash and test the full flow**

```bash
pio run -e full --target upload
pio device monitor --port /dev/cu.usbserial-110 --baud 115200
```

Test sequence:
1. Boot → home screen with weather + dim airspace panel visible
2. When an aircraft enters 25nm → radar screen auto-appears with blip, heading vector, callsign label, list row
3. Tap the top bar → returns to home screen (dismissed flag set, auto-switch suppressed until sky clears)
4. Aircraft leaves range → dismissed flag cleared
5. New aircraft enters → auto-switches back to radar screen

- [ ] **Step 10: Commit**

```bash
git add src/core/App.cpp src/core/App.h
git commit -m "feat: wire App.cpp to new SCREEN_RADAR with per-aircraft route lookup"
```

---

## Task 11: Remove dead code

**Files:**
- Modify: `src/LVGLDisplayManager.cpp`
- Modify: `src/LVGLDisplayManager.h`

- [ ] **Step 1: Remove from `LVGLDisplayManager.cpp`** — delete `build_aircraft_screen()` (the old aircraft detail screen builder, ~lines 958–1200+ in the original), `update_aircraft_screen()`, `event_btn_view_planes()`, `event_btn_back_home()`.

- [ ] **Step 2: Remove from `LVGLDisplayManager.h`** — delete any remaining references to `screen_aircraft`, `screen_home_empty`, old radar/blip widget pointers, old aircraft detail label pointers, `homeHasAircraft`, and the old function declarations for deleted functions.

- [ ] **Step 3: Remove unused `PLANE_DISPLAY_TIME` config constant** from `Config.h` (or leave it — harmless either way; delete if unused).

- [ ] **Step 4: Run native tests one final time**

```bash
pio test -e native
```
Expected: all tests pass

- [ ] **Step 5: Build and flash final build**

```bash
pio run -e full --target upload
```
Expected: clean build, no new warnings

- [ ] **Step 6: Final commit**

```bash
git add src/LVGLDisplayManager.cpp src/LVGLDisplayManager.h src/core/App.cpp src/core/App.h src/config/Config.h
git commit -m "refactor: remove dead aircraft detail screen and old radar widget"
```

---

## Self-Review

**Spec coverage check:**
- ✅ Home screen: weather + quiet airspace circle + "AIRSPACE CLEAR" status → Tasks 5, 10
- ✅ Radar screen: large circle, Casco Bay coastline, PWM marker, range rings, cardinal marks → Task 6
- ✅ Aircraft blips: colored by altitude, heading vectors, callsign labels → Task 7
- ✅ Aircraft list: tappable rows, collapse/expand, all key fields → Task 8
- ✅ 25nm range → Task 3
- ✅ Auto-switch on aircraft detection → Task 10
- ✅ Tap top bar to return home → Tasks 6, 10
- ✅ `latLonToRadarPx` for coastline projection → Task 1
- ✅ STATUS field logic (CRUISING / ON APPROACH / DEPARTING / LOW) → Task 9
- ✅ Route lookup for all aircraft (not just selected) → Task 10

**Type / name consistency check:**
- `RadarBlip` struct defined in Task 4 (h), used in Tasks 7 and 9 ✅
- `AircraftListRow` struct defined in Task 4 (h), used in Tasks 8 and 9 ✅
- `list_selected_idx_` initialized to -1 in Task 4, used in Tasks 8 and 9 ✅
- `radar_blips_[]` / `list_rows_[]` allocated in Tasks 7/8, updated in Task 9 ✅
- `update_home_screen(weather, aircraftCount)` — new 2-arg signature in Task 5, called correctly in Task 9 ✅
- `COASTLINE_PORTLAND_LEN` defined in Task 2 as `static const int`, used in Tasks 5 and 6 ✅
- `Config::RADAR_CIRCLE_RADIUS` defined as `int16_t` in Task 3, used as such in Tasks 6 and 9 ✅
- `GeoUtils::latLonToRadarPx()` defined in Task 1, used in Tasks 5 and 6 ✅
- `a.velocity` is in m/s (from OpenSky); converted to knots as `a.velocity * 1.94384f` in Task 9 ✅ (matches existing `update_aircraft_screen()` pattern)
