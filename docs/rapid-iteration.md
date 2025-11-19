# Rapid Iteration Guide

The goal is to shorten the hands-on loop from "edit → build → flash → verify" so display tweaks and service changes don’t require a full production boot every time.

## PlatformIO environments
- `pio run -e full -t upload` — full stack (Wi-Fi, OpenSky, weather). Use when you need end-to-end validation.
- `pio run -e smoke-test -t upload` — compiles `src/display_smoke_test.cpp` with `-DSMOKE_TEST`, skipping Wi-Fi/services for instant display or touch tuning.
  - Monitor quickly with `pio device monitor -e smoke-test` (inherits ports/filters from `full`).

## Workflows
1. **UI-only changes**
   - Switch to `smoke-test`; it boots to the RGB test harness immediately.
   - Adjust `DisplayManager` colors/layout without waiting for Wi-Fi/NTP/OpenSky.

2. **Service logic / parsing**
   - Keep structs/functions isolated in their `.cpp` files so only that translation unit rebuilds.
   - Consider stubbing HTTP responses with cached JSON so you don’t wait for network on every flash (toggle via a serial command or compile-time flag).

3. **Runtime toggles (recommended next step)**
   - Add serial commands that flip booleans (e.g., `DEV WIFI OFF`, `DEV WEATHER STUB`) so you can iterate without reflashing. The infrastructure already parses commands like `RAW` and `I2CSCAN`—extend that switch.

4. **Automate build/upload/monitor**
   - Bind a VS Code task or shell alias (e.g., `alias flashfull='pio run -e full -t upload && pio device monitor -b 115200'`) to avoid manual command repetition.
   - Keep the board connected; `ARDUINO_USB_CDC_ON_BOOT=1` ensures the serial port comes up immediately after flashing.

## Tips to avoid slow builds
- Avoid touching wide-impact headers (`Config.h`, `DisplayManager.h`) unless necessary—PlatformIO can reuse most object files otherwise.
- Don’t run `pio run` with no `-e`; with multiple envs that would build all of them.
- Keep experimental constants inside `.cpp` files or a small `dev_settings.h` so changes stay localized.

## Next enhancements
- Serial dev console to toggle logging or load stub data (no recompile needed).
- Cached HTTP payloads for weather/aircraft UI development.
- Pre-commit hooks or scripts to trigger the fast env automatically when editing display files.
