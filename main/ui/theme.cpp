/**
 * @file theme.cpp
 * @brief See theme.h. NVS layout (namespace "theme"):
 *          shell   : u8   (Shell enum value)
 *          custom  : blob (6x uint32 hex colors: bg, surface, text,
 *                          text_muted, accent, accent_alt)
 */
#include "theme.h"
#include <nvs.h>
#include <esp_log.h>
#include <cstring>

namespace theme {

static const char* TAG = "theme";
static const char* NVS_NS = "theme";

// One curated palette per shell color. The accent always echoes the orange
// branding so the three shells feel like one family.
static Palette make_black()
{
    return {
        lv_color_hex(0x0e0e12), // bg: near-black, matches the shell
        lv_color_hex(0x1c1c24), // surface
        lv_color_hex(0xf2f2f2), // text
        lv_color_hex(0x9a9aa4), // text_muted
        lv_color_hex(0xff9f1c), // accent: warm amber
        lv_color_hex(0x4cc9f0), // accent_alt: cool cyan counterpoint
    };
}

static Palette make_white()
{
    return {
        lv_color_hex(0xf5f5f7), // bg: soft white, matches the shell
        lv_color_hex(0xffffff), // surface
        lv_color_hex(0x1a1a1e), // text
        lv_color_hex(0x6e6e78), // text_muted
        lv_color_hex(0xff6b35), // accent: vivid orange
        lv_color_hex(0x2a9d8f), // accent_alt: teal counterpoint
    };
}

static Palette make_orange()
{
    return {
        lv_color_hex(0x2b1700), // bg: deep burnt orange-brown
        lv_color_hex(0x3d2200), // surface
        lv_color_hex(0xfff3e0), // text: cream
        lv_color_hex(0xd9a36b), // text_muted
        lv_color_hex(0xffb300), // accent: golden
        lv_color_hex(0xff5e3a), // accent_alt: hot coral
    };
}

static Shell s_shell = Shell::Black;
static Palette s_palette = make_black();
static Palette s_custom = make_black();

static Palette palette_for(Shell s)
{
    switch (s) {
        case Shell::White:  return make_white();
        case Shell::Orange: return make_orange();
        case Shell::Custom: return s_custom;
        case Shell::Black:
        default:            return make_black();
    }
}

static void persist()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "shell", (uint8_t)s_shell);
    if (s_shell == Shell::Custom) {
        uint32_t blob[6] = {
            lv_color_to_u32(s_custom.bg),        lv_color_to_u32(s_custom.surface),
            lv_color_to_u32(s_custom.text),      lv_color_to_u32(s_custom.text_muted),
            lv_color_to_u32(s_custom.accent),    lv_color_to_u32(s_custom.accent_alt),
        };
        nvs_set_blob(h, "custom", blob, sizeof(blob));
    }
    nvs_commit(h);
    nvs_close(h);
}

void init()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        uint8_t v = 0;
        if (nvs_get_u8(h, "shell", &v) == ESP_OK && v <= (uint8_t)Shell::Custom) {
            s_shell = (Shell)v;
        }
        uint32_t blob[6];
        size_t len = sizeof(blob);
        if (nvs_get_blob(h, "custom", blob, &len) == ESP_OK && len == sizeof(blob)) {
            s_custom = {lv_color_hex(blob[0]), lv_color_hex(blob[1]),
                        lv_color_hex(blob[2]), lv_color_hex(blob[3]),
                        lv_color_hex(blob[4]), lv_color_hex(blob[5])};
        }
        nvs_close(h);
    }
    s_palette = palette_for(s_shell);
    ESP_LOGI(TAG, "shell=%s", shell_name());
}

const Palette& palette() { return s_palette; }
Shell shell() { return s_shell; }

const char* shell_name()
{
    switch (s_shell) {
        case Shell::White:  return "white";
        case Shell::Orange: return "orange";
        case Shell::Custom: return "custom";
        case Shell::Black:
        default:            return "black";
    }
}

void set_shell(Shell s)
{
    s_shell = s;
    s_palette = palette_for(s);
    persist();
    ESP_LOGI(TAG, "shell set to %s", shell_name());
}

bool set_shell_by_name(const char* name)
{
    if (!name) return false;
    if (strcmp(name, "black") == 0)  { set_shell(Shell::Black);  return true; }
    if (strcmp(name, "white") == 0)  { set_shell(Shell::White);  return true; }
    if (strcmp(name, "orange") == 0) { set_shell(Shell::Orange); return true; }
    if (strcmp(name, "custom") == 0) { set_shell(Shell::Custom); return true; }
    return false;
}

void set_custom(const Palette& p)
{
    s_custom = p;
    set_shell(Shell::Custom);
}

void apply_screen(lv_obj_t* root)
{
    lv_obj_set_style_bg_color(root, s_palette.bg, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(root, s_palette.text, 0);
}

lv_obj_t* make_card(lv_obj_t* parent)
{
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_style_bg_color(card, s_palette.surface, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_text_color(card, s_palette.text, 0);
    return card;
}

lv_obj_t* make_label(lv_obj_t* parent, const char* txt, bool muted, const lv_font_t* font)
{
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, muted ? s_palette.text_muted : s_palette.text, 0);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    return l;
}

} // namespace theme
