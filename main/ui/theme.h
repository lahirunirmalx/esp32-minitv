/**
 * @file theme.h
 * @brief Device-wide theme layer. Every app pulls its colors from here so the
 *        whole UI matches the device's outer shell.
 *
 * The Mini TV cube ships in three shell colors — black, white, orange — and
 * each gets its own curated palette. The shell color is chosen during device
 * setup (captive portal) and persisted in NVS; it can be changed later from
 * the portal, or replaced wholesale with a custom palette (set via the portal
 * hex fields, or generated in firmware by calling set_custom()).
 *
 * RULES FOR APPS (enforced by the minitv-create-app skill):
 *   - never hardcode colors; use theme::palette()
 *   - call theme::apply_screen(root) on your app's root object
 *   - re-read the palette in onOpen() so a palette change shows after an
 *     app cycle (no reboot needed)
 */
#pragma once
#include <lvgl.h>
#include <cstdint>

namespace theme {

enum class Shell : uint8_t {
    Black = 0,   // dark UI, warm amber accent
    White,       // light UI, vivid orange accent
    Orange,      // deep burnt-orange UI, cream text, golden accent
    Custom,      // user-defined palette (portal hex fields / set_custom())
};

struct Palette {
    lv_color_t bg;          // screen background
    lv_color_t surface;     // cards / containers on top of bg
    lv_color_t text;        // primary text
    lv_color_t text_muted;  // secondary text
    lv_color_t accent;      // highlights, progress, active states
    lv_color_t accent_alt;  // second accent for charts/animations
};

// Load shell choice (+ custom palette, if any) from NVS namespace "theme".
// Defaults to Shell::Black on first boot. Call once before any app opens.
void init();

// The active palette. Cheap to call; apps should call it in onOpen().
const Palette& palette();

Shell shell();
const char* shell_name();              // "black" / "white" / "orange" / "custom"

// Select a built-in shell palette and persist it.
void set_shell(Shell s);
bool set_shell_by_name(const char* name);  // accepts the strings above

// Install + persist a fully custom palette (switches shell to Custom).
void set_custom(const Palette& p);

// Style helpers so apps don't repeat boilerplate.
void apply_screen(lv_obj_t* root);                  // bg color + text defaults
lv_obj_t* make_card(lv_obj_t* parent);              // themed surface container
lv_obj_t* make_label(lv_obj_t* parent, const char* txt,
                     bool muted = false, const lv_font_t* font = nullptr);

} // namespace theme
