# Plane Tracker

An ESP32-S3 firmware project that displays real-time weather and nearby aircraft tracking data on a 5" RGB display. This is a hobby project for aviation enthusiasts who want to monitor local airspace directly on their desk.

**Current Status:** v4 architecture rewrite in progress—modernizing the codebase for maintainability and extensibility.

---

## Features

- **Live Aircraft Tracking**: Pulls nearby aircraft from OpenSky Network within a configurable bounding box
- **Local Weather**: Real-time temperature, humidity, and conditions from OpenWeather API
- **Touch Interface**: Capacitive touch gestures to browse aircraft details and adjust brightness
- **Night Mode**: Auto-dims display during configured hours to avoid light pollution
- **Web Dashboard**: Local HTTP status dashboard showing telemetry and live data
- **Modular Architecture**: Clean separation between core orchestration, services, and UI layers

---

## Hardware

- **Microcontroller**: ESP32-S3
- **Display**: Elecrow 5" RGB panel (800×480) with GT911 capacitive touch
- **Build System**: PlatformIO + Arduino framework
- **UI Framework**: LVGL v9.4.0 (for UI rendering), LovyanGFX v1.1.16 (display driver)

---

## Getting Started

### Prerequisites

1. **PlatformIO CLI** installed ([docs](https://docs.platformio.org/))
2. **Clone this repository** locally
3. **API Credentials** (see Configuration section below)

### Setup

1. Copy the configuration template:
   ```bash
   cp src/config/Config.example.h src/config/Config.h
   ```

2. Edit `src/config/Config.h` with your credentials and location:
   - WiFi SSID and password
   - OpenWeather API key (get one free at [openweathermap.org](https://openweathermap.org/api))
   - OpenSky Network OAuth credentials (register at [opensky-network.org](https://opensky-network.org/))
   - Location coordinates (latitude/longitude)

3. **Deploy** to the ESP32-S3:
   ```bash
   pio run -e full -t upload
   pio device monitor -b 115200
   ```

### Build Environments

**Full Application** (with all services):
```bash
pio run -e full
pio run -e full -t upload
```

**Display Smoke Test** (LVGL + hardware only, no services):
```bash
pio run -e smoke-test -t upload
```

---

## Architecture

The rewrite introduces a modular core orchestration system:

```
src/
├── core/
│   ├── App.h/.cpp                    # Main orchestrator & lifecycle
│   ├── Scheduler.h/.cpp              # Non-blocking periodic task scheduler
│   └── HealthMonitor.h/.cpp          # System telemetry (uptime, memory, WiFi)
├── hal/
│   └── ElecrowDisplayProfile.h       # Hardware constants (pins, timing, I2C)
├── web/
│   └── WebDashboard.h/.cpp           # Local HTTP server & status endpoints
├── services/
│   ├── OpenSkyService.h/.cpp         # Aircraft API client
│   └── WeatherService.h/.cpp         # Weather API client
├── models/
│   ├── Aircraft.h                    # Aircraft telemetry struct
│   └── WeatherData.h                 # Weather data struct
├── LVGLDisplayManager.h/.cpp         # LVGL screen rendering & touch handling
└── main.cpp                          # Minimal bootstrap
```

### Key Design Patterns

- **Hardware Abstraction Layer (HAL)**: All GPIO pins and timing constants live in `hal/ElecrowDisplayProfile.h`—one source of truth for board-specific config
- **Non-blocking Scheduler**: `core::Scheduler` manages up to 12 independent timers (weather polls, aircraft updates, display refresh) without blocking
- **App Orchestrator**: `core::App` coordinates lifecycle, WiFi connection, services, and periodic task dispatch
- **Health Monitoring**: `core::HealthMonitor` tracks system metrics for diagnostic dashboards

---

## Configuration

Edit `src/config/Config.h` (copy from `Config.example.h`) to customize:

| Parameter | Default | Notes |
|-----------|---------|-------|
| `WEATHER_UPDATE_INTERVAL` | 30 min | How often to fetch weather |
| `PLANE_UPDATE_INTERVAL` | 60 sec | How often to fetch aircraft data |
| `VISIBILITY_RANGE` | ~13 km | Search radius for nearby aircraft |
| `HOME_LAT` / `HOME_LON` | Portland, ME | Center of tracking bounding box |
| `BRIGHTNESS_HIGH` / `BRIGHTNESS_LOW` | 150 / 50 | Day/night brightness levels |
| `NIGHT_START_HOUR` / `NIGHT_END_HOUR` | 22 / 6 | Auto-dim schedule |



---

## Web Dashboard

Once the device connects to WiFi, access the local dashboard:

- **Status Page**: `http://<device-ip>/`
- **JSON API**: `http://<device-ip>/api/status`

The dashboard displays:
- Live aircraft count
- Current temperature and weather condition
- System uptime and memory usage
- WiFi signal strength

---

## Roadmap (v4 Rewrite)

- ✅ **Phase 0–5 (Done)**: Core orchestrator, scheduler, HAL extraction, web skeleton
- 🔄 **Phase 6 (Next)**: Full web settings UI with persistence (Wi-Fi, API keys, location, intervals)
- 🔄 **Phase 7**: Runtime configuration management and NVS storage integration
- 🔄 **Phase 8**: UI redesign, legacy code removal, final documentation

See [vision.md](docs/vision.md) and [rapid-iteration.md](docs/rapid-iteration.md) for detailed project vision and development philosophy.

---

## Development

### Serial Debugging

Enable verbose serial logging by adding `-DDEBUG_SERIAL` to build_flags in platformio.ini:

```bash
pio device monitor -b 115200
```

### Touch Diagnostics

Type `RAW` in the serial monitor to toggle raw touch coordinate dump (useful for calibration).

### I2C Enumeration

Type `I2CSCAN` to enumerate connected I2C devices (run only before display initialization).

---

## Important Notes

### Hardware Configuration

- **RGB Panel Pins**: 16 data lines (GPIO 0–15), H-enable, V-sync, H-sync, pixel clock
- **Touch I2C**: GT911 at address 0x14 on I2C1 (SDA GPIO 19 / SCL GPIO 20)
- **PSRAM Framebuffer**: Required for LVGL rendering at 800×480; enabled by default
- **Write Frequency**: 15 MHz (tuned for Elecrow panel; do not change without testing)

All hardware-specific constants are centralized in `src/hal/ElecrowDisplayProfile.h`. Do not modify these without hardware validation.

### API Services

- **OpenWeather**: Free tier allows ~1,000 requests/day; weather updates every 30 minutes by default
- **OpenSky Network**: Free tier allows state vector queries; aircraft updates every 60 seconds by default

Be mindful of rate limits when adjusting update intervals.

---

## Building & Contributing

This project uses **PlatformIO** for builds and dependency management. VS Code tasks are pre-configured:

- `PlatformIO: Build` — Compile the firmware
- `PlatformIO: Upload` — Deploy to device
- `PlatformIO: Monitor` — Open serial console

Fork, branch, and submit PRs following Arduino coding conventions in the existing codebase. When adding new features, maintain the modular architecture—services should remain isolated from UI logic.

---

## License

[Add your license here, e.g., MIT, GPL-3.0]

---

## Questions?

- Check [vision.md](docs/vision.md) for project philosophy
- Review [rapid-iteration.md](docs/rapid-iteration.md) for development workflow
- Inspect serial logs (`logs/` directory) for boot diagnostics
- Consult official docs: [PlatformIO](https://docs.platformio.org/), [LovyanGFX](https://github.com/lovyan03/LovyanGFX), [LVGL](https://lvgl.io/)
- open `http://<device-ip>/api/status` for JSON status

Current dashboard is intentionally minimal and read-focused while the full settings flow is being implemented.

## Configuration and Secrets

1. Copy config template:

```bash
cp src/config/Config.example.h src/config/Config.h
```

2. Fill in local credentials and location values in `Config.h`.
3. Never commit real credentials.

Expected gitignore behavior:

- `src/config/Config.h` should remain untracked
- logs and build artifacts should remain untracked

## Notes for Contributors During Rewrite

- Prefer adding new functionality under `src/core`, `src/hal`, `src/web`, `src/services`.
- Keep UI changes inside LVGL display manager until UI module extraction is complete.
- Keep memory behavior conservative (single aircraft array allocation; avoid frequent heap churn).
- Build after each structural change.

## Immediate Next Milestone

- Add full web settings endpoints and persistence (Wi-Fi/API/location/timing)
- Integrate runtime config updates into core scheduler and services
- Continue migration off legacy display path
