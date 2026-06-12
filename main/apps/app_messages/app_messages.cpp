/**
 * @file app_messages.cpp
 * @brief See app_messages.h.
 *
 * BLE pattern notes:
 *   - ble_service::take_rx() is a cheap mutex-guarded poll designed for
 *     tick(); no worker thread is needed (contrast with app_weather, where
 *     the blocking HTTP fetch forces one).
 *   - ble_service::notify() is quick and non-blocking (drops the message
 *     when no peer is subscribed), so acking from tick() is fine.
 */
#include "app_messages.h"
#include "../../net/ble_service.h"

static void anim_opa_cb(void* var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t*)var, (lv_opa_t)v, 0);
}

void AppMessages::build(lv_obj_t* root)
{
    const auto& pal = theme::palette();
    setRefreshMs(100); // poll fast so a message appears the moment it lands

    lv_obj_t* title = theme::make_label(root, "Messages", false, &lv_font_montserrat_24);
    lv_obj_set_style_text_color(title, pal.accent, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    lv_obj_t* card = theme::make_card(root);
    lv_obj_set_size(card, 216, 130);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 4);

    _msg = theme::make_label(card, "waiting for a BLE message...", true,
                             &lv_font_montserrat_14);
    lv_label_set_long_mode(_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(_msg, 192);
    lv_obj_align(_msg, LV_ALIGN_TOP_LEFT, 0, 0);

    _meta = theme::make_label(root, "", true, &lv_font_montserrat_14);
    lv_obj_align(_meta, LV_ALIGN_BOTTOM_MID, 0, -32);

    _status = theme::make_label(root, "", true, &lv_font_montserrat_14);
    lv_obj_align(_status, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void AppMessages::tick()
{
    std::string rx;
    if (ble_service::take_rx(rx)) {
        _count++;
        lv_label_set_text(_msg, rx.c_str());
        lv_obj_set_style_text_color(_msg, theme::palette().text, 0);
        lv_label_set_text_fmt(_meta, "#%d - %u bytes", _count, (unsigned)rx.size());

        // Quick fade-in so a repeated identical message is still visibly "new".
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, _msg);
        lv_anim_set_exec_cb(&a, anim_opa_cb);
        lv_anim_set_values(&a, LV_OPA_30, LV_OPA_COVER);
        lv_anim_set_duration(&a, 350);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);

        // Acknowledge to the phone (visible if it subscribed to TX notify).
        char ack[48];
        snprintf(ack, sizeof(ack), "ack #%d (%u bytes)", _count, (unsigned)rx.size());
        ble_service::notify(ack);
    }

    lv_label_set_text(_status, ble_service::is_connected()
                                   ? "connected - write to RX"
                                   : "advertising as MiniTV...");
}
