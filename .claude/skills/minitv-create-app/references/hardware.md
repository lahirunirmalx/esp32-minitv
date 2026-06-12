# Mini TV hardware reference (Freenove FNK0112)

Module marking: **ESP-32S** (classic ESP32, dual-core Xtensa LX6 @ 240 MHz).
4 MB flash (DIO @ 40 MHz), **no PSRAM**. CH340 USB-UART (`/dev/ttyUSB*`,
115200). All pins below are authoritative — they were confirmed on real
hardware and live in [main/hal/hal_config.h](../../../main/hal/hal_config.h).

## Pin map

| Function | GPIO | Notes |
|---|---|---|
| LCD SCLK | 14 | SPI2/HSPI IO_MUX pin → 80 MHz capable |
| LCD MOSI | 13 | SPI2/HSPI IO_MUX pin |
| LCD DC | 2 | data/command select (strapping pin, fine as output) |
| LCD CS | 15 | strapping pin, fine as output |
| LCD RST | — | not wired; software reset (SWRESET) only |
| Backlight | 19 | LEDC PWM, **active-low** (low = bright) |
| Panel VDD | 21 | **active-low** enable; must be LOW or the panel is dead |
| Touch pad | 32 | TOUCH_PAD_NUM9 (T9), pad on top of the cube |
| UART TX/RX | 1 / 3 | console via CH340 |

Free GPIOs are extremely limited on this board; the header is not broken out.
Treat the pin map as closed — there are no spare peripherals to wire.

## Display: ST7789, 240×240 IPS

- RGB565, big-endian on the wire (the LVGL flush byte-swaps in place)
- Init quirks (already handled in `st7789.cpp`): SWRESET path (no RST pin),
  `INVON` required (IPS), `MADCTL 0x00` (no rotation)
- 80 MHz SPI is stable because SCLK/MOSI are IO_MUX pins; if a future board
  rev shows garbling, step `HAL_LCD_SPI_HZ` down to 40 MHz
- LVGL: two 60-line partial DMA buffers (≈28 KB each), partial render mode

## Touch: native capacitive sensing (no controller chip)

The "touch sensor" is the bare ESP32 touch peripheral on GPIO32 — a single
pad under the top of the case. There is **no touch on the display**, no
position, no multi-touch. `hal/touch/touch.cpp` does: IIR filter @ 10 Hz,
16-sample baseline at boot (nothing may touch the pad during the first
~400 ms), press = deviation > baseline/8.

Gesture vocabulary (decided in the supervisor, `app_main.cpp`):

| Gesture | Action |
|---|---|
| tap (<600 ms) | next app (or wake display if asleep) |
| hold ≥3 s | captive portal (another 3 s hold inside cancels/reboots) |

Apps that want the tap themselves can read `LV_EVENT_CLICKED` on a clickable
object — but remember every tap *also* cycles apps unless you change the
supervisor, so prefer passive displays.

## Power quirks (do not "fix")

- `CONFIG_ESP_BROWNOUT_DET=n` — USB power dips below threshold during WiFi
  PHY init and hard-resets the board if the detector is on. Leave disabled.
- Backlight off ≠ panel off: while awake, VDD (GPIO21) stays low/enabled;
  only the backlight PWM idles.
- After `HAL_DEEP_SLEEP_MS` (5 min) of idle the supervisor enters deep sleep:
  panel VDD off, GPIO holds on both active-low lines, touch-pad wake armed.
  Waking = a full reboot. Apps don't need to handle this — but long-running
  worker threads must tolerate dying at any time (they already must, since
  deep sleep, factory reset, or a flash can kill them mid-loop).

## Memory budget (no PSRAM — ~300 KB DRAM total)

Already resident: LVGL draw buffers ~56 KB, LVGL objects, WiFi (~50 KB when
active), NimBLE (~60 KB), TLS during a request (~40 KB transient, freed by
dynamic-buffer config). Practical app budget: **≲20 KB steady-state**. Check
with `heap_caps_get_free_size(MALLOC_CAP_DEFAULT)` logged from a worker.

## Flash budget

Single factory partition, 2.75 MB. Current firmware ≈1.3 MB — room for
fonts/assets, but each enabled Montserrat size costs flash; enable new sizes
in `sdkconfig.defaults` (`CONFIG_LV_FONT_MONTSERRAT_*`) only as needed.
