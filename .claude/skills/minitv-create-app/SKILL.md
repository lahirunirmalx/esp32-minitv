---
name: minitv-create-app
description: Creating a new app, screen, animation, or UI feature for the ESP32 Mini TV firmware in this repo. Use whenever the user asks to create/add/build an app, screen, watch face, animation, or display feature for the Mini TV / this device (e.g. "create an animated hello world app", "add a clock screen", "show weather on the device"). Generates a themed Mooncake/LVGL app via scripts/new_app.py, implements the UI under the device's constraints (240x240 ST7789, single top-pad gesture input, no PSRAM, dual-core split), then builds and flashes with the minitv-build-flash skill, and (when Audience is beginner in the project CLAUDE.md) explains the added code in simple terms — where it went, why, and what it does.
license: MIT
metadata:
  domain: embedded
  triggers: mini tv, minitv, app, screen, animation, watch face, LVGL, mooncake, hello world, display, UI
  role: specialist
  scope: implementation
  output-format: code
---

# Creating Mini TV Apps

## 1 · Device constraints (read first, non-negotiable)

| Constraint | Consequence |
|---|---|
| 240×240 ST7789, RGB565 | Design for a square screen; Montserrat 14/24/48 are the enabled fonts |
| **ASCII-only display strings** | The built-in fonts cover ASCII plus only `°` and `•`. Never put `—`, `·`, `…`, or curly quotes in an LVGL label or portal HTML — they render as blank gaps. Always use plain `-`, `|`, `...`, `'` |
| **Display is NOT a touchscreen** | The only input is one capacitive pad on **top of the cube**. Apps get tap/hold gestures only — **never** design positional touch, swipes, sliders, scroll, or on-screen buttons. A tap surfaces as `LV_EVENT_CLICKED` at screen center; app cycling on tap is handled by the supervisor, not by apps |
| No PSRAM, ~4 MB flash | LVGL + its 2×28 KB draw buffers + WiFi + BLE are already resident. Keep per-app allocations modest (≲20 KB); cap HTTP bodies (`web_client` default 16 KB) |
| Dual-core split | UI task owns LVGL **exclusively on core 1** and must never block. HTTP, JSON parsing, or any work >a few ms goes on core 0 (a `std::thread` lands there automatically via sdkconfig) and hands results back via `std::atomic`/FreeRTOS queue. **LVGL calls only ever from `build()`/`tick()`/`teardown()`** |
| Theme layer | **Never hardcode colors.** Use `theme::palette()` (`bg/surface/text/text_muted/accent/accent_alt`) and the `theme::make_label` / `theme::make_card` / `theme::apply_screen` helpers. The palette follows the device's shell color (black/white/orange/custom) chosen in the captive portal |

## 2 · Workflow

1. **Generate the skeleton** (from the project root):

   ```bash
   python scripts/new_app.py <name>        # e.g. python scripts/new_app.py hello
   ```

   This creates `main/apps/app_<name>/app_<name>.{h,cpp}` derived from
   `ui::ScreenApp` and registers it in `main/apps/apps.cpp` (markers
   `<<APP_INCLUDES>>` / `<<APP_INSTALL>>`). Never hand-create app files or
   hand-edit the markers; never edit `main/CMakeLists.txt` (sources are
   globbed). To remove an app: `python scripts/new_app.py <name> --remove`.

2. **Implement the UI** in `build()` (widgets) and `tick()` (throttled
   repaint, default 500 ms — adjust with `setRefreshMs()`). The base class
   owns the root container, applies the theme background, and deletes
   everything (running animations included) on close. Override `teardown()`
   only if you start timers/threads that must be stopped.

3. **Build + flash** — invoke the `minitv-build-flash` skill
   (`./scripts/flash.sh` from the project root).

4. **Explain what you built (audience-aware).** Check the `Audience:` line
   in the project CLAUDE.md ("Code explanations" section):
   - `beginner` (default) → after reporting the build/flash result, explain
     every piece of code you added or changed in simple terms — **where** it
     went, **why** it's needed, **what it does** on the device — following
     `references/explaining-code.md` (load it now).
   - `expert` → no explanation; just list changed files + build result.
   - If the user states their level in conversation ("I'm new to this" /
     "I'm experienced, skip explanations"), update the `Audience:` line in
     CLAUDE.md so the preference persists.

The reference implementation is [main/apps/app_demo/app_demo.cpp](../../../main/apps/app_demo/app_demo.cpp) —
bouncing themed label + animated arc + a status line fed from core-0 state.
Mirror its structure.

## 3 · Reference loading (conditional — don't slurp all)

| Load | When |
|---|---|
| `references/lvgl-animations.md` | The request involves animation, motion, easing, transitions |
| `references/connectivity.md` | The app needs WiFi/HTTP data, BLE messages, or network status |
| `references/hardware.md` | New peripherals, pins, SPI/LEDC/touch questions, or anything off the beaten path |
| `references/explaining-code.md` | `Audience: beginner` is set (project CLAUDE.md) and you're about to present the finished change |

## 4 · Patterns crib

Animated value (full recipes in `references/lvgl-animations.md`):

```cpp
lv_anim_t a;
lv_anim_init(&a);
lv_anim_set_var(&a, obj);
lv_anim_set_exec_cb(&a, [](void* var, int32_t v) { lv_obj_set_y((lv_obj_t*)var, v); });
lv_anim_set_values(&a, 40, 90);
lv_anim_set_duration(&a, 900);
lv_anim_set_playback_duration(&a, 900);          // bounce back
lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
lv_anim_start(&a);
```

Network data into the UI (full recipe in `references/connectivity.md` —
load it whenever an app spawns a worker):

```cpp
// core 0: ONE immortal worker per app, spawned on FIRST open only and gated
// by _active. NEVER a thread per open - rapid app cycling leaks workers and
// a failed thread creation abort()s the firmware (exceptions are disabled).
_active.store(true);
if (!_worker_started) { _worker_started = true; std::thread([this]{ /* see reference */ }).detach(); }
// core 1, in tick():
if (_fresh.exchange(false)) lv_label_set_text_fmt(_label, "%d", _value.load());
// teardown(): _active.store(false);
```

Custom palette by prompt: when the user asks for a specific look ("make it
neon green"), call `theme::set_custom({...})` once (e.g. from a setup snippet
or temporary code path) — it persists to NVS — rather than styling one app
differently from the rest.

## 5 · Checklist before flashing

- [ ] App generated by `new_app.py`, not by hand; registered automatically
- [ ] All colors from `theme::palette()`; no `lv_color_hex(...)` literals in app code
- [ ] Display strings are pure ASCII (no `—`/`·`/`…`/curly quotes; `°` and `•` are the only allowed extras)
- [ ] No blocking calls (`web_client`, `vTaskDelay`, file I/O) in `build()`/`tick()`
- [ ] No positional-touch UI (buttons/sliders/swipe)
- [ ] Heap use modest; HTTP bodies capped
- [ ] `idf.py build` clean, then flash via minitv-build-flash
