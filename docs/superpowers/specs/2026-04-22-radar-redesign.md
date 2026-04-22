# Radar Redesign — Local Airspace Display

**Date:** 2026-04-22  
**Status:** Approved

---

## Overview

Replace the existing 190×190px radar widget (currently embedded in the home screen's right column) with a proper two-screen system. The home screen becomes weather-only. A dedicated full-screen radar screen auto-activates whenever an aircraft enters 25nm of home and shows a geographic radar with a Casco Bay coastline overlay and a tappable aircraft list.

The use case is local plane spotting: glance at the device to immediately identify what aircraft is flying overhead, where it came from, where it's going, and how fast it's moving.

---

## Screen 1 — Home Screen

### Layout (800×480)

- **Top bar** (58px): Clock (UTC), date, station name "PORTLAND INTL JETPORT · PWM", lat/lon. No changes from current.
- **Left panel** (~490px wide, full body height): Current conditions. Expanded from current layout — more room now that radar is gone.
- **Right panel** (~290px wide): "LOCAL AIRSPACE" status panel.
- **Status bar** (26px): OPENSKY status + LIVE indicator. No changes.

### Left Panel — Weather Content

Retains all current weather data. The exact fields (3-day vs 5-day forecast, etc.) are a detail left for implementation — the design accommodates whatever the data source provides. Current layout uses: big temperature, conditions text, wind, pressure, visibility, flight rules (VFR/IFR), 3-day forecast, humidity, dewpoint, ceiling, last-updated time.

### Right Panel — Airspace Status

When no aircraft are within 25nm:
- Header: "LOCAL AIRSPACE" label
- Subtitle: "25 nm radius · Portland ME"
- A dimmed circular radar outline with the Casco Bay coastline rendered at low opacity — same geographic data as the full radar screen, just quiet
- Center home dot (dim)
- "● AIRSPACE CLEAR" in green
- "No aircraft within 25nm"
- Last-checked timestamp
- Countdown to next update (OpenSky polling interval)

### Transition to Radar Screen

The moment OpenSky returns any aircraft within 25nm, the display switches automatically to the radar screen. No user action required. If the user is on the home screen and all aircraft leave 25nm range, it switches back.

---

## Screen 2 — Radar Screen

### Layout (800×480)

- **Top bar** (58px): Same clock/date. Center: amber badge "⬤ N AIRCRAFT NEARBY". Tap anywhere on the top bar to return to home screen manually.
- **Left panel** (490px wide): Full radar.
- **Right panel** (310px wide): Aircraft list.
- **Status bar** (26px): OPENSKY status, hint text "tap top bar to return", LIVE indicator.

### Left Panel — Radar

- Circular radar, ~380px diameter, centered in the 490px panel.
- Background: `COLOR_INSET` dark blue (`#162033`).
- Border: subtle dark stroke.
- **Range rings**: outer ring = 25nm (edge of circle), inner dashed ring = 12.5nm.
- **Cardinal tick marks**: N/S/E/W stubs at the circle edge with labels.
- **Range labels**: "12.5 nm" and "25 nm" near the N tick.
- **Coastline**: Simplified Casco Bay / Portland ME coastline polygon, rendered as a polyline clipped to the radar circle. Stored as a static C array of lat/lon pairs in a new file `src/data/CoastlinePortland.h`. Land mass filled with a very dark green tint (`#131f13`, ~45% opacity). Water (Gulf of Maine, Casco Bay) is the radar background.
- **PWM marker**: Cross (+) at the PWM airport position, amber color, with "PWM" label. Clipped to circle.
- **Home dot**: Cyan filled circle (5px) with a faint outer ring at the center.
- **Aircraft blips**: 6px filled circles. Heading vector: line extending from the blip in the direction of travel (~20px long). Color: amber (`#f59e0b`) if altitude < 5,000 ft, cyan (`#00d4ff`) otherwise.
- **Selected blip**: Dashed selection ring around the currently selected blip (the one whose row is expanded in the list).
- **Blip labels**: Small callsign text and altitude/heading hint rendered next to each blip (where space allows — labels can be suppressed if blips are too close together).

### Right Panel — Aircraft List

- Header: "AIRCRAFT IN RANGE · N" label.
- Scrollable list of aircraft rows, sorted by distance from home (closest first).
- Maximum visible aircraft: up to 15 (OpenSky `MAX_AIRCRAFT` limit), but the panel scrolls if more than ~4-5 fit.

**Default (collapsed) row** (~66px tall — reliably tappable at GT911 36–48px minimum):
- Left accent bar: amber or cyan matching blip color.
- Callsign in large text (16px bold).
- Aircraft type + route on second line: e.g. `Boeing 737  ·  BOS → BTV`
- Summary line: altitude, speed, heading, distance from home.

**Expanded row** (tapped — grows to ~104px):
- Same callsign + type/route header.
- Divider line.
- Grid of fields: ALTITUDE, SPEED, HEADING on one row; DIST FROM HOME, STATUS on next row.
- STATUS field shows contextual text derived from altitude and bearing to PWM:
  - `CRUISING` — altitude ≥ 5,000 ft
  - `ON APPROACH` — altitude < 5,000 ft AND bearing from aircraft toward PWM within ±30° of aircraft heading AND within 15nm of PWM
  - `DEPARTING` — altitude < 5,000 ft AND distance to PWM increasing (bearing ≠ toward PWM) AND within 15nm of PWM
  - `LOW / OVERFLYING` — altitude < 5,000 ft AND not near PWM (>15nm)
- Tapping any other row collapses current and expands that one. Tapping the expanded row again collapses it.

**Auto-selection**: On screen entry, the closest aircraft (row 1) is automatically expanded. If aircraft count changes, the selection is preserved by callsign if still present, otherwise reset to closest.

---

## Geographic Data — Coastline

### Data format

A new header file `src/data/CoastlinePortland.h` contains a single static array:

```cpp
struct GeoPoint { float lat; float lon; };
static const GeoPoint COASTLINE_PORTLAND[] = { ... };
static const int COASTLINE_PORTLAND_LEN = ...;
```

Points cover the coastline within a ~30nm bounding box around Portland ME (43.6441°N, 70.3093°W), simplified to approximately 80–120 points using Douglas-Peucker. This keeps the array under 1 KB. The polygon traces the land/water boundary from south of Cape Elizabeth northward through Portland Head, into Casco Bay, and up toward Falmouth/Cumberland.

### Rendering

At radar draw time, each consecutive pair of points is projected from lat/lon to radar pixel coordinates using the same polar math as blip positioning (equirectangular projection at this scale is accurate enough for 25nm). The resulting polyline is drawn using LVGL line objects or an LVGL canvas depending on what's most efficient. A filled polygon (or series of triangles) behind the line provides the land tint.

### PWM position

`PWM_LAT = 43.6462f`, `PWM_LON = -70.3093f` — added to `Config.h` or `CoastlinePortland.h`.

---

## Data Flow Changes

### Range change

`RADAR_MAX_RANGE_NM` reduced from 150.0 to 25.0 in `Config.h`. The OpenSky query bounding box (`VISIBILITY_RANGE`) may need a corresponding reduction — currently 0.45° (~50km ≈ 27nm), which is a reasonable match for 25nm and can stay as-is.

### Screen switching logic (`App.cpp`)

Current logic: switches to aircraft detail screen on any aircraft detection.  
New logic:
- If any aircraft within 25nm → show radar screen.
- If no aircraft within 25nm → show home screen.
- User can tap the top bar on the radar screen to manually return home; this sets a "dismissed" flag that suppresses auto-switch for one full polling cycle (prevents immediate re-trigger).

### Aircraft model

`Aircraft.h` — no structural changes needed. Existing fields (lat, lon, altitude, speed, heading, callsign, valid) cover all displayed data. Route (origin/destination) comes from the RouteCache service already in place.

---

## LVGL Architecture

### New screens

- `buildHomeScreen()` — extended to include the new airspace status panel (replaces old radar panel build).
- `buildRadarScreen()` — new function, builds the full-screen radar layout.
- `update_home_screen()` — updated to drive the airspace status panel (aircraft count, last-check time, countdown).
- `update_radar_screen()` — new function, updates blip positions, heading vectors, and the aircraft list rows.

### Coastline rendering approach

Two options — to be decided during implementation based on memory and LVGL object count constraints:

**Option A (LVGL line objects):** Create `COASTLINE_PORTLAND_LEN - 1` static `lv_line` objects at build time, each connecting consecutive coastline points projected to pixel coordinates. Projection happens once at build time since home position is fixed. Zero runtime cost. Memory: ~40 bytes per line object × 100 points ≈ 4 KB object overhead.

**Option B (LVGL canvas):** Draw the coastline onto an `lv_canvas` once at build time using `lv_canvas_draw_line`. Canvas stored in PSRAM. Single draw call, lower object count. Requires PSRAM allocation.

**Recommendation:** Option A first (simpler, no PSRAM dependency), fall back to Option B if object count hits LVGL limits.

### Aircraft list panel

Implemented as a scrollable `lv_list` or manually managed `lv_obj` container with per-aircraft row objects. Rows are pre-allocated up to `MAX_AIRCRAFT` (15) and shown/hidden as needed. Expanded state tracked by index.

---

## Out of Scope

- Touch-selecting individual radar blips (targets too small at 6px; list panel provides the interaction instead).
- Runtime coastline updates or map tile fetching.
- Zoom controls.
- Aircraft history trails.
- Audio alerts.

---

## Open Details (for implementation)

- Exact weather fields shown (3-day vs 5-day etc.) — defer to what the weather service provides; layout accommodates both.
- Coastline point array — to be generated from Natural Earth or NOAA simplified data and committed as part of implementation.
- Whether to use LVGL line objects or canvas for coastline rendering — decide based on memory profiling.
