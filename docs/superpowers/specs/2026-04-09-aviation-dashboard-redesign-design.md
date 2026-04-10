# Aviation Dashboard UI Redesign — Design Spec

**Date:** 2026-04-09  
**Status:** Approved — ready for implementation  
**Approach:** Screen-by-screen (Approach 3) — complete each screen fully before moving to next

---

## 1. Goal

Replace the current light-themed, generic card UI with a dark aviation dashboard aesthetic — Flightradar24/cockpit instrument feel. Dark navy background, cyan accent, amber warnings. Must look good on the Elecrow 5" TFT (cheap panel — design accounts for gamma compression and poor contrast in dark ranges).

---

## 2. Color Palette

All `#define` constants in `LVGLDisplayManager.cpp` replace existing `COLOR_*` definitions.

| Constant | Hex | Role |
|---|---|---|
| `COLOR_BG` | `#0e1726` | Screen background (slightly lighter than pure black — avoids gamma crush) |
| `COLOR_TOPBAR` | `#101f33` | Top bar background |
| `COLOR_PANEL` | `#162033` | Card / panel background |
| `COLOR_INSET` | `#0a1428` | Instrument tile inset (sinks below panel) |
| `COLOR_STATUSBAR` | `#060e1a` | Status bar background (darkest element) |
| `COLOR_ACCENT` | `#00d4ff` | Cyan — primary interactive + data color |
| `COLOR_AMBER` | `#f59e0b` | Amber — low-altitude blips, squawk, warnings |
| `COLOR_SUCCESS` | `#22c55e` | Green — climbing V/S, system OK |
| `COLOR_TEXT_PRIMARY` | `#eaf6ff` | Near-white with blue tint (reads better than pure white on cold-shifted TFT) |
| `COLOR_TEXT_SECONDARY` | `#5a8aaa` | Secondary labels, conditions |
| `COLOR_TEXT_DIM` | `#2a5070` | Tertiary labels, units, status bar text |
| `COLOR_BORDER` | `#1e3a54` | Panel borders, dividers |
| `COLOR_BORDER_ACCENT` | `#004466` | Accent-tinted border (route block, radar circle) |
| `COLOR_TEXT_ON_ACCENT` | `#060e1a` | Text on cyan button backgrounds |

**Rationale for 4 distinct darks:** Cheap TFT panels compress dark values. Using `#060e1a` → `#0a1428` → `#0e1726` → `#162033` keeps visible depth between status bar, background, insets, and panels even when the display gamma crushes them.

---

## 3. Screen Layout

### 3.1 Shared Elements

**Top bar** (800×58px, `#101f33`, border-bottom `#1e3a54`):
- Left: time `#00d4ff` 28px bold + date `#5a8aaa` 11px below
- Right: location label + status string (dim, 9px)

**Status bar** (800×26px, `#060e1a`, border-top `#1e3a54`):
- Left: dim system status string (last update, OpenSky state) — 9px, letter-spacing 2px
- Right: `● LIVE` in green when data is fresh, `● IDLE` in dim when no aircraft

---

### 3.2 Home Screen — Aircraft Present

**Layout:** Two columns, top 58px = topbar, bottom 26px = statusbar, body 396px tall.

**Weather panel** (left, flex-fill, border-right `#1e3a54`), padding 12px 16px:
- **Current conditions:**
  - Temperature: 56px bold `#eaf6ff` (e.g. `41°`)
  - To the right: condition string 14px `#5a8aaa`, feels-like 11px `#2a5070`, hi/lo 11px `#2a5070`
- **Details strip** (border-top + border-bottom `#1e3a54`, padding 6px 0):
  - Wind label + value, Humidity label + value, Sunrise ↑ amber, Sunset ↓ amber
- **5-day forecast** (fills remaining height):
  - Header: `5-DAY FORECAST` 8px dim letterspaced
  - 5 rows, each `#0a1428` bg, border `#1e3a54`, radius 4px, padding 5px 10px:
    - Day name 11px bold `#5a8aaa` · Condition string 10px dim flex-fill · Hi° bold white / Lo° dim

**Radar panel** (right, 310px fixed, centered column), padding 10px 16px:
- `◈ AREA CONTACTS` label (8px dim, letter-spacing 4px)
- Radar circle (190×190px, radial gradient dark, border `#00d4ff22`):
  - Two range rings (33%, 66% diameter, `#00d4ff14`)
  - Crosshairs (`#00d4ff0a`)
  - Sweep arc (conic-gradient, rotated) — static in LVGL (no animation to save flash)
  - Home dot (6px cyan, center, `lv_arc` or small filled circle)
  - Aircraft blips: cyan (`#00d4ff`) for airliner/unknown, amber (`#f59e0b`) for low-altitude (<5000ft)
  - Blip position: computed from distance/bearing relative to home, scaled to radar circle radius
- Contact count: large number cyan 26px bold + `CONTACTS` unit dim
- `✈  VIEW AIRCRAFT` button (cyan bg, dark text, radius 4px)
  - Tapping anywhere on radar panel also triggers navigation to aircraft list

**Dynamic behavior:** The radar panel is only added to the screen when `aircraftCount > 0`. When count drops to 0, radar panel is removed and weather panel expands to full width (see §3.3). This is implemented by rebuilding the home screen layout on each `update()` call when the aircraft count crosses zero.

---

### 3.3 Home Screen — No Aircraft

**Layout:** Full-width body (800px), same topbar/statusbar.

**Left column** (340px fixed):
- Temperature: 80px bold `#eaf6ff`
- Condition: 16px `#5a8aaa`
- Feels-like: 13px `#2a5070`
- Hi/lo: 12px `#2a5070`
- Divider (`#1e3a54`)
- 2-col detail grid: Wind, Humidity labels + values
- Sunrise/sunset: amber values, 14px

**Right column** (flex-fill, border-left `#1e3a54`, padding-left 20px):
- `5-DAY FORECAST` header 8px dim letterspaced + divider
- 5 rows with breathing room (9px padding each), border-bottom `#1e3a54`:
  - Day 13px bold `#5a8aaa` (38px) · Condition 11px dim · Hi 14px bold white · Lo 12px dim

**Status bar text:** `NO AIRCRAFT DETECTED` instead of `OPENSKY OK`.
**Watermark:** `NO CONTACTS IN RANGE` at bottom center, 9px `#1e3a54` (barely visible).

---

### 3.4 Aircraft Detail Screen

**Top bar** (800×70px — slightly taller than home topbar):
- Left: `‹ BACK` button (border `#00d4ff44`, cyan text, 10px)
- Center: callsign 36px bold `#00d4ff`, letter-spacing 3px
- Right: distance + bearing `42.3 nm · 047° NE` 22px bold `#eaf6ff` + `FROM PORTLAND ME` label 9px dim

**Body** (top 70px, bottom 26px, padding 14px 18px, flex column, gap 10px):

**Row 1 — Identity cards** (flex row, flex-shrink 0):
- Airline card (`#162033`, border `#1e3a54`): label `AIRLINE` + value e.g. `United Airlines`
- Aircraft card: label `AIRCRAFT` + value e.g. `Boeing 737-800` + sub `ICAO: B738`
- Squawk card (margin-left auto): label `SQUAWK` + value amber `#f59e0b`

**Row 2 — Route block** (flex-shrink 0, `#0a1428`, border `#004466`, radius 6px, padding 12px 18px):
- Label: `ROUTE` 8px dim letterspaced
- Value: callsign-resolved route string, e.g. `BOS → LAX` 28px bold `#00d4ff`
- Sub: full airport names e.g. `Boston · Los Angeles` 11px `#5a8aaa`
- States:
  - **Loading:** `LOOKING UP...` dim italic
  - **Resolved:** `BOS → LAX` with full names
  - **Not found / API error:** `ROUTE UNAVAILABLE` dim, no sub-text
  - **API quota exhausted:** `ROUTE N/A` dim

**Row 3 — Instrument tiles** (flex row, flex: 1 — fills remaining height):
- 4 tiles, equal width, `#162033`, border `#1e3a54`, radius 5px:
  - `ALTITUDE` · value ft (white)
  - `SPEED` · value kts (white)
  - `HEADING` · value degrees + cardinal (white)
  - `VERT SPEED` · value fpm, color-coded: green if positive, red (`#ef4444`) if negative, white if 0

**No coordinates row** — lat/lon removed from this screen entirely.

---

## 4. Route Lookup — AeroDataBox Integration

### API
- **Service:** AeroDataBox via api.market (instant signup, no approval required)
- **Endpoint:** `GET https://aerodatabox.p.rapidapi.com/flights/callsign/{callsign}` with `X-RapidAPI-Key` header, or equivalent api.market endpoint
- **Credential:** `AERODATABOX_API_KEY` in `.env` → `AERODATABOX_API_KEY_MACRO` in `credentials.h`
- **Free tier:** ~300–600 requests/month (api.market free plan)
- **Optional:** if key is the placeholder value `"your-aerodatabox-api-key"`, skip the lookup and show `ROUTE UNAVAILABLE` — device works fully without a key

### Caching Strategy (NVS)
- Namespace: `"route_cache"`
- Key: callsign string (e.g. `"UAL1234"`)
- Value: packed string `"BOS|Boston Logan|LAX|Los Angeles Intl"` (pipe-separated)
- Lookup flow:
  1. Check NVS for callsign key → if found, use cached value
  2. If not found, fire HTTP GET to Aviation Edge
  3. On success: parse origin/destination, write to NVS, display
  4. On failure (no data, quota exhausted, timeout): display `ROUTE UNAVAILABLE`
- NVS entries never expire — airline routes don't change

### New Class: `RouteCache`
- `src/services/RouteCache.h` / `.cpp`
- `bool lookup(const String& callsign, String& origin, String& destination)`
- `void store(const String& callsign, const String& origin, const String& destination)`
- Internally uses `Preferences` (Arduino NVS wrapper)

### Trigger
- Route lookup fires when the aircraft detail screen is loaded, not on every OpenSky poll.
- The detail screen shows `LOOKING UP...` in the route block while the HTTP request is in flight (non-blocking via async HTTP or polled in `tick()`).

---

## 5. Distance & Bearing Calculation

Replace the removed "Route Unknown" content. Computed from OpenSky lat/lon vs. `HOME_LAT_MACRO` / `HOME_LON_MACRO`.

**Haversine formula** for distance (nautical miles):
```
d = 2r · arcsin(√(sin²(Δlat/2) + cos(lat1)·cos(lat2)·sin²(Δlon/2)))
```
Where `r = 3440.065` nm (Earth radius in nautical miles).

**Bearing:**
```
θ = atan2(sin(Δlon)·cos(lat2), cos(lat1)·sin(lat2) − sin(lat1)·cos(lat2)·cos(Δlon))
```
Convert to cardinal: N/NE/E/SE/S/SW/W/NW (8-point compass, 45° sectors).

**Location:** `src/utils/GeoUtils.h` — two inline functions: `float distanceNm(float lat1, float lon1, float lat2, float lon2)` and `String bearingCardinal(float lat1, float lon1, float lat2, float lon2)`.

Displayed in:
- **Home radar panel:** blip position (scaled bearing + distance to radar circle coords)
- **Aircraft detail topbar:** `42.3 nm · 047° NE`

---

## 6. Blip Positioning on Radar

Radar circle is 190×190px. Center = home. Max visible range = configurable, default 150nm.

Given distance `d` and bearing `θ` from home:
```
scale = min(d / MAX_RANGE_NM, 1.0) * (circle_radius - blip_margin)
blip_x = center_x + scale * sin(θ_rad)
blip_y = center_y - scale * cos(θ_rad)   // y-axis inverted (screen coords)
```

Blips clamped to circle boundary if aircraft is beyond `MAX_RANGE_NM`. Color: amber if altitude < 5000ft, cyan otherwise.

---

## 7. Implementation Order (Approach 3)

1. **Color constants + GeoUtils** — replace all `COLOR_*` defines, add `GeoUtils.h`, verify build
2. **Home screen** — rebuild `buildTopBar`, `buildWeatherCard`, `buildAircraftCard`/radar panel; implement dynamic show/hide logic; update `update_home_screen()`
3. **Aircraft detail screen** — rebuild `build_aircraft_screen()`, add route block with loading state; update `update_aircraft_screen()`
4. **No-aircraft screen** — rebuild `build_no_aircraft_screen()` to match no-aircraft home layout
5. **RouteCache + Aviation Edge HTTP** — new `RouteCache` class, wire into detail screen load
6. **`.env` / `credentials.h`** — `AVIATION_EDGE_API_KEY_MACRO` already added to `extra_script.py`

---

## 8. Constraints

- **Flash budget:** Currently 85.3% (~275KB headroom). No new libraries. `RouteCache` uses `Preferences` (already linked). HTTP client for Aviation Edge uses the existing `HTTPClient` already in use for OpenSky/OpenWeather.
- **RAM:** 14.4% used. Haversine uses stack-only floats. NVS cache adds no heap.
- **No animation:** Radar sweep is a static rotated arc widget, not animated — saves CPU and avoids LVGL animation complexity.
- **No swipe gestures:** Out of scope per existing deferred list.
