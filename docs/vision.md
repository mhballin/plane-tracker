# Plane-Tracker Vision & Strategy

_Last updated: 2025-11-19_

## Mission
Deliver a reliable ESP32-S3 dashboard that shows local weather plus nearby aircraft on the Elecrow 5" RGB panel. The device boots unattended, maintains Wi-Fi connection, and auto-rotates through planes within the configured home radius—serving as a glanceable desktop flight board.

## Hardware / Software Stack
- **MCU / Framework:** ESP32-S3, Arduino via PlatformIO (`esp32-s3-devkitc-1`)
- **Display pipeline:** LovyanGFX RGB panel (800×480) + GT911 capacitive touch, PSRAM framebuffer
- **APIs:** OpenSky Network (OAuth2) for aircraft tracking, OpenWeather for current conditions
- **Configuration:** `src/config/Config.h` (gitignored) with `Config.example.h` template for secrets management

## Architecture Snapshot
- **`src/main.cpp`** — System orchestrator: Wi-Fi connection, NTP sync, aircraft array allocation, periodic update scheduling (weather 30min, aircraft 60s, display 1s)
- **`src/DisplayManager.*`** — Owns LovyanGFX initialization, touch/gesture processing, screen state management (home/aircraft/no-aircraft), and all UI rendering with caching
- **`src/services/OpenSkyService.*`** — OAuth token lifecycle, bounding-box aircraft polling, ground/private traffic filtering, callsign-based airline/type inference
- **`src/services/WeatherService.*`** — OpenWeather HTTP requests with retry/backoff, parsing of current conditions + extended metrics (feels-like, hi/lo temps, wind, visibility, pressure)
- **`src/models/`** — Lightweight data structures (`Aircraft`, `WeatherData`) passed between layers
- **`src/display_smoke_test.cpp`** — Touch diagnostics harness (compiled with `-DSMOKE_TEST`): visualizes touch trails, reports gesture metrics (distance/velocity/duration/direction), validates threshold tuning

## Current State (Nov 2025) ✅
**Infrastructure:**
- Private GitHub repo (`mhballin/plane-tracker`) with secrets hardened (`.gitignore`, `Config.example.h`)
- Dual PlatformIO environments: `[env:full]` (production app) and `[env:smoke-test]` (touch diagnostics with UART serial output)
- Rapid iteration documentation (`docs/rapid-iteration.md`) and AI agent guidance (`.github/copilot-instructions.md`)

**Data & Network:**
- **Weather metrics:** Fully implemented—temperature, feels-like, hi/lo, humidity, pressure, wind speed, visibility, description
- **Network resiliency:** HTTP retry with exponential backoff (3 attempts, incremental delay); descriptive error messages surfaced in status bar
- **Status bar:** Live clock + transient status messages; updates every cycle to prevent stale timestamps

**Touch & Gestures:**
- **Gesture detection:** Empirically tuned from real touch data
  - Distance threshold: 60px (catches short intentional swipes)
  - Velocity threshold: 200px/s (filters accidental touches)
  - Duration limit: 700ms (accommodates slower deliberate gestures)
  - Axis dominance: 1.5× ratio prevents diagonal misclassification
- **Navigation:** Swipe left (home→aircraft), swipe right (aircraft→home), swipe up/down (brightness ±25)
- **Screen switching:** Cache invalidation on state changes ensures proper redraws when navigating between views
- **Diagnostics harness:** Touch trail visualization, per-gesture analysis (pass/fail on each threshold), serial logging of coordinates/metrics

**Build & Flash:**
- RAM usage: 14.1% (46KB / 327KB)
- Flash usage: 80.2% (1.05MB / 1.31MB)
- Clean builds in ~8–40s depending on cache state

## Completed Milestones
- ✅ Repository audit and baseline build verification
- ✅ Secret sanitization and private repo migration
- ✅ Vision and rapid iteration workflow documentation
- ✅ Network resiliency implementation (retry/backoff, error propagation)
- ✅ Real weather metrics parsing and UI integration
- ✅ Touch diagnostics harness creation
- ✅ Gesture threshold tuning from empirical data
- ✅ Screen switching redraw bug fix

## Active Focus
**Touch reliability has been achieved.** Gesture detection now works consistently with tuned thresholds validated against real-world swipe patterns. Screen navigation (home ↔ aircraft) is reliable, and brightness adjustment gestures respond properly.

## Deferred / Future Work
The following enhancements remain on the backlog, prioritized after core functionality is stable:
- **Tap detection:** Quick actions (brightness toggle, return-to-home) via short touch duration (<200ms, <30px travel)
- **Theme redesign:** Light and dark color palettes optimized for 800×480 IPS readability
- **Layout refinement:** 
  - Home screen: improved info hierarchy (current conditions, forecast cards, aircraft count badge)
  - Aircraft detail: metric grouping (altitude/speed/heading, distance/bearing prominence, origin→destination)
- **UI polish:** Eliminate remaining flicker, modularize layout helpers, apply consistent spacing/typography
- **Enhanced aircraft data:** Real-time distance/bearing calculation from `Config::HOME_*`, origin/destination hydration when available
- **CI/CD:** GitHub Actions build pipeline with status badge
- **Calibration workflow:** Optional GT911 touch calibration UI (currently relies on factory defaults)

## Near-Term Roadmap

**Immediate Next Steps (Post-Touch Tuning):**
1. **Visual polish:** Address remaining UI flicker, implement light theme palette, refactor layout helpers
2. **Enhanced aircraft data:** Distance/bearing calculation, origin/destination display when available
3. **Tap gestures:** Quick-action shortcuts (brightness toggle, instant home return)
4. **Documentation refresh:** Updated README with current feature set, troubleshooting guide, serial command reference

**v1.0 Release Criteria:**
- 24-hour uptime validation (no manual resets, continuous data refresh)
- Complete setup documentation (<15min from clone to flash)
- All core features functional: weather display, aircraft tracking, gesture navigation, brightness control
- Known issues documented (rate limits, missing metadata, etc.)

## Success Criteria
- ✅ Device boots unattended and maintains Wi-Fi connectivity
- ✅ Display shows real-time weather metrics (no placeholders)
- ✅ Status bar clock stays accurate with proper timezone handling
- ✅ Touch gestures work reliably for navigation and brightness adjustment
- ✅ Screen transitions render correctly without cache artifacts
- ⏳ 24-hour stability test (pending extended runtime validation)
- ⏳ New contributor onboarding in <15 minutes (pending README refresh)

## Technical Achievements
- **Touch precision:** 60px min distance, 200px/s min velocity, 700ms max duration, 1.5× axis dominance—validated against 8+ real gesture samples
- **Network resilience:** 3-attempt retry with exponential backoff, descriptive error messages in UI
- **Memory efficiency:** 14.1% RAM usage, 80.2% flash (within safe operating margins)
- **Rapid iteration:** Dual build environments (full app + diagnostics), <40s clean builds, dedicated harness for touch tuning

## Open Questions / Risks
- **Long-term stability:** Need 24h+ runtime telemetry (Wi-Fi reconnection frequency, OpenSky rate limit impacts, memory leaks)
- **Rate limiting:** Anonymous OpenSky access may require local metadata caching if usage patterns trigger limits
- **Touch calibration:** Currently relies on GT911 factory defaults; may need calibration UI if accuracy degrades across different panels
- **Flash capacity:** At 80% usage, future feature additions may require code optimization or selective feature builds
