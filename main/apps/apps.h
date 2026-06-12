/**
 * @file apps.h
 * @brief Installs every app into Mooncake and fills app_registry.
 */
#pragma once

namespace apps {

// Install all apps (in cycle order). Call once from the UI task after
// lvgl_port::init() and theme::init().
void install_all();

} // namespace apps
