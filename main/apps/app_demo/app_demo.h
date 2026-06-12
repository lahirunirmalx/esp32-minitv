/**
 * @file app_demo.h
 * @brief Animated demo app — the canonical pattern for every app on this
 *        device. Shows: themed UI, lv_anim usage, the ScreenApp lifecycle,
 *        and reading core-0 state without blocking the UI task.
 */
#pragma once
#include "../../ui/screen_app.h"

class AppDemo : public ui::ScreenApp {
public:
    AppDemo() { setAppInfo().name = "demo"; }

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _hello = nullptr;
    lv_obj_t* _arc = nullptr;
    lv_obj_t* _status = nullptr;
};
