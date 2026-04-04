# Copilot Instructions for Plane-Tracker

## Big Picture
- ESP32-S3 + Arduino (PlatformIO) driving an Elecrow 5" RGB panel via LovyanGFX. The device shows local weather + nearby aircraft from OpenSky.
- Core modules:
  - `src/main.cpp` boots hardware, manages Wi-Fi, schedules updates, and funnels data to the display.
  - `src/DisplayManager.*` owns all LovyanGFX setup, touch/gesture handling, and UI rendering (home, aircraft detail, no-aircraft states).
  - `src/services/OpenSkyService.*` handles OAuth token fetch/refresh and the `states/all` polling within a Portland-centric bounding box.
  - `src/services/WeatherService.*` performs OpenWeather requests and parses `main` + first `weather[]` entry into `WeatherData` structs.
  - `src/models/` defines the lightweight `Aircraft`/`WeatherData` containers passed between layers.
  - `src/config/Config.example.h` is the template—copy to `Config.h` locally with Wi-Fi/API credentials, location, and timing constants.

## Developer Workflow
- Build/upload/monitor from repo root:
  - `pio run`
  - `pio run -t upload`
  - `pio device monitor -b 115200 -p /dev/cu.usbserial-110`
- VS Code tasks (`PlatformIO: Build/Upload/Monitor`) mirror those commands.
- Enable verbose serial logging by adding `-DDEBUG_SERIAL` to build_flags in platformio.ini; the device halts on fatal init errors so logs are essential.
- Display smoke test lives in `src/display_smoke_test.cpp`; rebuild with `-DSMOKE_TEST` (see `platformio.ini` comment) when tuning RGB timing/touch without the full app.

## Architecture & Patterns
- Memory: a single `Aircraft` array `new`’d once in `setup()`. Avoid additional heap churn—pass pointers to existing structs.
- Display pipeline: `DisplayManager::update()` is called every second with cached weather + the current aircraft pointer. Gestures (swipe left/right/up/down) switch screens or adjust brightness, so any UI additions belong inside `DisplayManager`.
- Touch/I2C: GT911 touch sits on I2C1 (SDA 19/SCL 20). Do not run Wire scans after `DisplayManager::initialize()`—it disrupts the shared bus.
- Data flow: `OpenSkyService::fetchAircraft()` filters out ground traffic, skips private registrations, and guesses airline/type from callsigns instead of performing expensive metadata lookups. Weather parsing currently exposes temp/humidity/pressure plus `condition/description`; any extra UI metrics must be surfaced through `WeatherService::parseWeatherData()` first.
- Timing: Update cadences come from `Config` (weather 30 min, aircraft 60 s, display 1 s). `Config::PLANE_DISPLAY_TIME` drives auto-rotation when multiple aircraft exist.

## Integration Gotchas
- OpenSky OAuth: credentials + token endpoint live in `Config`. `OpenSkyService` refreshes tokens when `millis() > tokenExpiryTime`; ensure `TOKEN_LIFETIME` matches reality when tweaking.
- LovyanGFX config (`DisplayManager.h`) mirrors the official Elecrow reference: RGB pins, 15 MHz write clock, PSRAM framebuffer. If colors look wrong, adjust `cfg.pclk_active_neg`/`cfg.freq_write` and test via the smoke harness.
- Wi-Fi/TZ: `Config::GMT_OFFSET_SEC`/`DAYLIGHT_OFFSET_SEC` must match the deployment locale or the status clock drifts; prefer updating those instead of patching formatting code.

## How to Extend Safely
- New aircraft fields: add to `models/Aircraft`, populate inside `OpenSkyService::fetchAircraft`, then render inside `DisplayManager::showPlane`. Keep parsing light—OpenSky responses are large.
- New weather metrics: update `WeatherData`, parse them in `WeatherService::parseWeatherData`, then draw in `DisplayManager::showWeather`. Reuse cached values to avoid flicker.
- New config knobs: declare once in `Config`, reference via `Config::NAME` everywhere else. Never hardcode secrets in code or commits—commit only `Config.example.h`.

## Debug Aids
- Serial commands: typing `I2CSCAN` in the monitor reruns an I2C scan (only use before the display/touch init) and `RAW` toggles raw touch dumps for calibration.
- Logs directory stores captured monitor sessions under `logs/*.log`; inspect recent files when diagnosing boot/display issues.

Questions or ambiguities? Review `README.md` for setup steps or inspect `src/main.cpp` + `DisplayManager.cpp` for the canonical coding style before making major changes.
