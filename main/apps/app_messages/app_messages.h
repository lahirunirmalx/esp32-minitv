/**
 * @file app_messages.h
 * @brief BLE messages app: shows the latest text written to the device's
 *        BLE RX characteristic (e.g. from LightBlue / nRF Connect) and
 *        acknowledges each message back over the TX notify characteristic.
 */
#pragma once
#include "../../ui/screen_app.h"

class AppMessages : public ui::ScreenApp {
public:
    AppMessages() { setAppInfo().name = "messages"; }

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _msg = nullptr;
    lv_obj_t* _meta = nullptr;
    lv_obj_t* _status = nullptr;
    int _count = 0;
};
