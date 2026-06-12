# LVGL 9 animation recipes for the Mini TV

LVGL v9.2 API (vendored in `components/lvgl/`). All of this runs on the UI
task — animations are driven by `lv_timer_handler()` in the supervisor loop,
so apps just declare them. Deleting an object (or the app's root on close)
kills its running animations automatically.

## Basics

```cpp
static void exec_cb(void* var, int32_t v) { lv_obj_set_x((lv_obj_t*)var, v); }

lv_anim_t a;
lv_anim_init(&a);
lv_anim_set_var(&a, obj);
lv_anim_set_exec_cb(&a, exec_cb);
lv_anim_set_values(&a, 0, 200);          // from, to
lv_anim_set_duration(&a, 1000);          // ms
lv_anim_set_delay(&a, 200);              // optional start delay
lv_anim_start(&a);
```

Useful exec targets: `lv_obj_set_x/y`, `lv_obj_set_width/height`,
`lv_arc_set_value`, `lv_bar_set_value`, `lv_obj_set_style_opa` (via a
wrapper), `lv_img_set_angle`, `lv_obj_set_style_translate_x/y`.

## Easing paths

`lv_anim_set_path_cb(&a, ...)` with one of:

- `lv_anim_path_linear` (default)
- `lv_anim_path_ease_in` / `ease_out` / `ease_in_out` — natural motion
- `lv_anim_path_overshoot` — playful pop past the target
- `lv_anim_path_bounce` — bounce at the end
- `lv_anim_path_step` — jump at the end

## Looping / ping-pong

```cpp
lv_anim_set_playback_duration(&a, 900);            // animate back (ping-pong)
lv_anim_set_playback_delay(&a, 100);
lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
lv_anim_set_repeat_delay(&a, 0);
```

## Color/opacity pulses

Animate opacity for a pulse without touching theme colors:

```cpp
static void opa_cb(void* var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)var, (lv_opa_t)v, 0);
}
// values 80..255, ping-pong, infinite -> breathing effect
```

For color transitions prefer two themed objects cross-faded by opacity over
animating raw colors — keeps everything palette-driven.

## Completion callbacks & chaining

```cpp
lv_anim_set_completed_cb(&a, [](lv_anim_t* anim) {
    // start the next animation here for a sequence
});
```

For multi-property choreography give each property its own lv_anim with
matching durations/delays (LVGL has no timeline object in 9.2).

## Text & number tickers

Animate an int and repaint a label from the exec callback:

```cpp
static void count_cb(void* var, int32_t v) {
    lv_label_set_text_fmt((lv_obj_t*)var, "%ld", (long)v);
}
```

## Scrolling long text

```cpp
lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
lv_obj_set_width(label, 200);   // narrower than the text -> auto-marquee
```

## Performance notes (no PSRAM, 60-line partial buffers)

- Prefer animating position/size of a few objects over full-screen effects;
  every dirty area is re-rendered and SPI-flushed.
- `lv_obj_set_style_translate_x/y` is cheaper than re-layout via `set_x/y`
  when flex/grid is involved (it skips layout recomputation).
- Avoid animating >5–6 objects at 16 ms cadence simultaneously; the flush is
  80 MHz SPI but the render is CPU-bound on core 1.
- Canvas-based effects (lv_canvas) need a full-color buffer in RAM — keep
  canvases small (e.g. 100×100 RGB565 = 20 KB) or avoid them.
