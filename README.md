Plane-Tracker — quick dev guide

This project targets an ESP32-S3 with an Elecrow 5" HMI display (LovyanGFX).

Quick setup (macOS / zsh)

1) Install PlatformIO Core (recommended):

```bash
python3 -m pip install --user pipx
python3 -m pipx ensurepath
# restart your terminal, then:
python3 -m pipx install platformio
```

Verify:

```bash
pio --version
```

2) Build, upload, monitor (from project root):

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

3) VS Code tasks

Open the Command Palette (Cmd+Shift+P) -> Tasks: Run Task -> choose one of:
- PlatformIO: Build
- PlatformIO: Upload
- PlatformIO: Monitor

If VS Code can't find `pio`, make sure you restarted VS Code after installing PlatformIO or add the install location (`~/.local/bin` or `~/.local/pipx/bin`) to your PATH in `~/.zshrc`.

Preparing for GitHub / secrets
------------------------------
1. Copy the sanitized config template before your first build:

	```bash
	cp src/config/Config.example.h src/config/Config.h
	```

2. Fill in Wi-Fi, OpenWeather, and OpenSky credentials inside `Config.h`. This file stays local only.
3. `src/config/Config.h`, `.pio/`, and `logs/*.log` are already ignored by `.gitignore`. Double-check with `git status -u` to ensure no secrets are staged.
4. If you ever committed real keys in another repo, rotate them before pushing here.
5. When you’re ready to publish to a private GitHub repo:

	```bash
	git init
	git add .
	git commit -m "Initial commit: Plane-Tracker"
	git remote add origin git@github.com:YOUR_USER/plane-tracker.git
	git branch -M main
	git push -u origin main
	```

Notes

- Do not commit real API keys. Use `src/config/Config.example.h` as a template.
- The display code uses LovyanGFX with PSRAM for the framebuffer. Ensure PSRAM is enabled on your board.

Display tuning & color calibration
---------------------------------
If your Elecrow 5" HMI looks tinted (too blue or too warm), run the smoke test to cycle full-screen red/green/blue so you can see which channels are weak or inverted.

How to run the smoke test:

1. Build with the `SMOKE_TEST` macro defined. In PlatformIO you can add `-DSMOKE_TEST` to build flags or temporarily modify `platformio.ini`.

2. Upload and open the serial monitor. The smoke test will cycle RED -> GREEN -> BLUE full-screen so you can visually inspect channel balance.

Tuning knobs to try (in `src/DisplayManager.h` inside LGFX constructor):

- `cfg.pclk_active_neg` (0 or 1): try flipping this if colors look swapped or phase-shifted.
- `cfg.freq_write`: try 12000000 (12MHz) or 10000000 (10MHz) depending on stability; higher freq may change color rendering.
- `cfg.hsync_front_porch`, `cfg.hsync_pulse_width`, `cfg.hsync_back_porch`, and the `vsync_*` values: small adjustments can fix timing artifacts.
- `cfg.pin_d* / pin_r* / pin_g*` mappings: ensure they match your board/wiring (the default is from the Elecrow example).
- Backlight: `lcd->setBrightness(value)` — reducing brightness can hide tint but won't fix color balance.

If cycling the colors reveals a weak channel (e.g., blue is dim), try toggling `cfg.pclk_active_neg` and toggling `cfg.freq_write` between 10MHz and 12MHz. After each change, rebuild and re-run the smoke test.

If you want, I can add a small runtime setting to toggle `pclk_active_neg` at boot via a button or serial command so you can try both without rebuilding.

