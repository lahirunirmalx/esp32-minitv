# ESP32 Mini TV firmware

Standalone ESP-IDF v5.5 firmware scaffold for the **Freenove ESP32 Mini TV
(FNK0112)** — with Claude Code skills baked in, so you can check this folder
out, plug the device in, and prompt:

> create an animated hello world app

…and Claude will scaffold the app, build, and flash it. New to embedded?
Claude also explains every piece of code it writes in simple terms (see
"Beginner mode" below).

<!-- TODO(images): hero shot of the Mini TV cube running the demo app -->
<!-- ![Mini TV running the demo app](docs/img/hero.jpg) -->

## What's inside

- **Pure ESP-IDF v5.5** (no Arduino) for the classic ESP32 (ESP-32S module,
  4 MB flash, no PSRAM)
- **LVGL 9.2** UI on the 240×240 ST7789 (80 MHz SPI, partial double-buffer)
- **Mooncake 2.1** app framework — tap the pad on top of the cube to cycle
  apps (demo + device-info ship out of the box)
- **Theme layer** matched to the device's shell color (black / white /
  orange / custom), configured from the setup portal, persisted in NVS
- **WiFi** STA with captive-portal onboarding, **BLE** GATT message pipe
  (advertises as `MiniTV`), **HTTPS web client** (cert bundle attached)
- Both dependencies vendored under `components/` — **offline build**

## Prerequisites (one-time)

You need ESP-IDF **v5.5** and a serial driver for the board's CH340 USB chip.

### Linux / macOS

```bash
./scripts/setup_idf.sh        # installs ESP-IDF v5.5 into ~/esp/esp-idf-v5.5 (~2 GB)
```

The script checks prerequisites (`git`, `python3`, `cmake` — it tells you
the exact `apt`/`brew` command if something's missing), adds you to the
`dialout` group on Linux, and explains the CH34x driver on macOS if needed.
Already have IDF v5.5 somewhere? `export IDF_PATH=/path/to/esp-idf` and skip
this.

### Windows

1. Run the official **ESP-IDF Windows installer** from
   <https://dl.espressif.com/dl/esp-idf/> and choose **v5.5** — it installs
   the toolchain *and* the CH340 USB driver.
2. Use `scripts\flash.ps1` from any PowerShell (it finds the IDF install
   automatically), or work inside the "ESP-IDF 5.5 PowerShell" start-menu
   shortcut with plain `idf.py` commands.

## Quick start

```bash
./scripts/flash.sh             # Linux/macOS: build + flash + monitor (Ctrl+] exits)
```

```powershell
.\scripts\flash.ps1            # Windows
```

If ESP-IDF is missing, `flash.sh` offers to install it for you. The port is
auto-detected (`/dev/ttyUSB*` on Linux, `/dev/cu.usbserial*` on macOS, CH340
COM port on Windows).

**First boot:** the screen shows *"hold top pad 3 s to set up WiFi"*. Hold
the pad → join the `MiniTV-Setup` WiFi (password `12345678`) → a setup page
opens (or browse to <http://192.168.4.1>) → enter your WiFi and pick your
device's **shell color** — the UI theme matches it. Tap the pad to switch
between apps.

<!-- TODO(images): GIF of first-boot setup: hint banner -> portal page -> themed demo -->
<!-- ![First boot setup](docs/img/setup.gif) -->

## Creating an app

Ask Claude Code (the project ships skills that know this device's rules), or
by hand:

```bash
python scripts/new_app.py myapp       # scaffold + auto-register
# edit main/apps/app_myapp/app_myapp.cpp (build()/tick())
./scripts/flash.sh
```

<!-- TODO(images): GIF of prompting Claude "create an animated hello world app" -->
<!-- ![Claude creating an app](docs/img/claude-app.gif) -->

### Beginner mode

By default Claude explains every piece of code it adds in simple terms —
**where** it went, **why**, and **what it does** on the device. If you're
experienced, say "skip explanations" once and it switches to terse expert
mode (the `Audience:` setting in [CLAUDE.md](CLAUDE.md)).

## The built-in apps

| App | What it shows |
| --- | --- |
| demo | Animated hello screen — the canonical UI/animation pattern to copy |
| info | WiFi + IP, BLE, heap (current + minimum-ever), firmware version, theme — your on-device debug screen |
| weather | Live weather via open-meteo (no API key) — the canonical network-fetch pattern to copy; edit the coordinates at the top of `app_weather.cpp` |

After 1 minute idle the backlight turns off; after 5 minutes the device deep
sleeps (touch the pad to wake — it reboots in a couple of seconds).

## Testing the BLE pipe

The device advertises as **MiniTV** with one service: RX (write) and TX
(read/notify) characteristics.

1. Install **nRF Connect** (Android/iOS) and scan — connect to `MiniTV`.
2. Open the unknown service, write any text to the **write** characteristic.
3. The info app shows `ble: connected`; apps read the message with
   `ble_service::take_rx()` and can push back with `ble_service::notify()`
   (subscribe to the notify characteristic to see it).

## Hardware cheat sheet

| Part | Details |
| --- | --- |
| Display | ST7789 240×240, SPI: SCLK 14, MOSI 13, DC 2, CS 15 (no RST pin) |
| Backlight / panel VDD | GPIO 19 (PWM) / GPIO 21 — both active-low |
| Touch | Capacitive pad on top of the cube → GPIO 32 (T9). The screen itself is not touch |
| Serial | CH340: `/dev/ttyUSB*` (Linux), `/dev/cu.usbserial*` (macOS), `COMx` (Windows), 115200 |

More: `.claude/skills/minitv-create-app/references/hardware.md`

## Troubleshooting quick hits

- **No serial port** → use a data-capable USB cable (many are charge-only).
  Linux: `sudo usermod -aG dialout $USER` + re-login. Windows/macOS: CH340
  driver (the Windows IDF installer includes it).
- **Board resets in a loop** → don't re-enable the brownout detector; this
  board's USB power dips during WiFi init (already handled in
  `sdkconfig.defaults`).
- More in `.claude/skills/minitv-build-flash/SKILL.md`.
