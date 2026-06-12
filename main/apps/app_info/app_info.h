/**
 * @file app_info.h
 * @brief Device info app: WiFi state + IP, BLE state, free heap, theme shell.
 *        Doubles as the on-device debugging screen — if something feels
 *        wrong (no network, low memory), tap to this app first.
 */
#pragma once
#include "../../ui/screen_app.h"

class AppInfo : public ui::ScreenApp {
public:
    AppInfo() { setAppInfo().name = "info"; }

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _wifi = nullptr;
    lv_obj_t* _ble = nullptr;
    lv_obj_t* _heap = nullptr;
    lv_obj_t* _min_heap = nullptr;
    lv_obj_t* _theme = nullptr;
};
