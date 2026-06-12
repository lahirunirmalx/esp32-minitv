---
name: minitv-build-flash
description: Building, flashing, and monitoring the ESP32 Mini TV firmware in this repo. Use whenever the user asks to build, compile, flash, upload, deploy, or monitor the Mini TV / this device, and as the final step after creating or changing an app with minitv-create-app. Wraps ESP-IDF v5.5 (idf.py) with the board's port detection (/dev/ttyUSB*, CH340) and the known troubleshooting playbook.
license: MIT
metadata:
  domain: embedded
  triggers: build, flash, upload, deploy, monitor, idf.py, esptool, serial, mini tv, minitv
  role: specialist
  scope: implementation
  output-format: code
---

# Build & Flash the Mini TV

## 1 · The one-liner

From the project root:

```bash
./scripts/flash.sh            # Linux/macOS: build + flash + monitor (exit monitor: Ctrl+])
./scripts/flash.sh build      # build only (no device needed)
./scripts/flash.sh monitor    # monitor only
```

```powershell
.\scripts\flash.ps1           # Windows (PowerShell); same build/monitor modes
```

The scripts source ESP-IDF and auto-detect the port. Use them unless you
need raw `idf.py`.

**ESP-IDF missing?** On Linux/macOS run `./scripts/setup_idf.sh` (flash.sh
offers this automatically when IDF isn't found — ~2 GB download into
`~/esp/esp-idf-v5.5`). On Windows, ESP-IDF must be installed via the official
installer (<https://dl.espressif.com/dl/esp-idf/>, pick v5.5) — don't try to
script it; the installer also provides the CH340 driver.

## 2 · Manual commands

```bash
source ~/esp/esp-idf-v5.5/export.sh        # or $IDF_PATH/export.sh — needs v5.5.x
cd <project root>
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

- Target is already pinned (`esp32`); run `idf.py set-target esp32` only on a
  pristine checkout if `build/` is absent and CMake complains.
- The build is **fully offline** — LVGL and Mooncake are vendored in
  `components/` (see `components/VENDORED.md`). If the component manager
  tries to fetch anything, something is misconfigured.
- First build compiles ~1500 files (LVGL); subsequent builds are incremental.

## 3 · Port & permissions

The board's CH340 USB-UART enumerates as:

| OS | Port | Driver note |
| --- | --- | --- |
| Linux | `/dev/ttyUSB*` | in-kernel; permission denied → `sudo usermod -aG dialout $USER` + re-login |
| macOS | `/dev/cu.usbserial*` or `/dev/cu.wchusbserial*` | native on 10.14+; else WCH CH34x driver |
| Windows | `COMx` (Device Manager: "USB-SERIAL CH340") | IDF installer includes it; else CH341SER from wch.cn |

No port at all on any OS → almost always a charge-only USB cable.
Flash write errors at 460800 baud → retry with `idf.py -p PORT -b 115200 flash`.

## 4 · Troubleshooting (board-specific, learned on real hardware)

| Symptom | Cause / fix |
|---|---|
| Boot loops with brownout messages | `CONFIG_ESP_BROWNOUT_DET` was re-enabled — it must stay `n` on this board (USB power dips during WiFi PHY init). Restore `sdkconfig.defaults`, `rm sdkconfig`, rebuild |
| `region iram0_0_seg overflowed` at link | The IRAM-relief block in `sdkconfig.defaults` (`CONFIG_ESP_WIFI_IRAM_OPT=n` etc.) was lost. Restore it, `rm sdkconfig`, rebuild |
| App image too big for partition | Factory partition is 2.75 MB (`partitions.csv`). Check `idf.py size`; trim fonts (`CONFIG_LV_FONT_MONTSERRAT_*`) or assets |
| sdkconfig changes not taking effect | `sdkconfig.defaults` only seeds a **fresh** sdkconfig: `rm sdkconfig && idf.py build` |
| New app source not compiled | Sources are globbed at configure time; `new_app.py` touches `main/CMakeLists.txt` for you. If you added files by hand: `touch main/CMakeLists.txt` |
| Screen stays black after flash | Backlight starts ON at boot here; black screen usually means panel VDD (GPIO21) isn't asserted — check `backlight::init()` ran before `st7789::chip_init()` |
| Garbled/torn display | Drop `HAL_LCD_SPI_HZ` to 40 MHz in `main/hal/hal_config.h` |
| Touch never registers | Pad baseline calibrates in the first ~400 ms of boot — don't hold the cube top while it resets |
| Monitor shows gibberish | Wrong baud — console is 115200 |

## 5 · Audience-aware reporting

Check the `Audience:` line in the project CLAUDE.md ("Code explanations"
section):

- `beginner` (default) → report results in plain language. Build errors:
  quote only the 1–3 lines that matter, say what they mean in simple terms,
  what you changed to fix them, and why that fixes it. Success: one sentence
  on what was flashed and what the user should now see on the device.
- `expert` → terse output: exit status, size if relevant, error excerpt
  verbatim, fix applied. No teaching.

## 6 · After flashing

`idf.py monitor` shows the boot log: ST7789 init, touch baseline, theme
shell, WiFi state, BLE advertising. First-time setup: hold the top pad 3 s →
join AP `MiniTV-Setup` (pass `12345678`) → http://192.168.4.1 → set WiFi +
the device's shell color (this picks the UI palette). Tap = next app.
