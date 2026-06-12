/**
 * @file captive_portal.h
 * @brief Device setup portal: SoftAP + DNS hijack + a small web form for
 *        WiFi credentials, the device's shell color (black/white/orange —
 *        picks the matching UI palette), and an optional fully custom
 *        palette (hex fields).
 *
 * Opened by holding the top touch pad for 3 s. run() never returns: on save
 * it persists everything to NVS and reboots; cancelling is another 3 s hold
 * (handled by the UI task, which also reboots).
 */
#pragma once

namespace captive_portal {

inline constexpr const char* AP_SSID = "MiniTV-Setup";
inline constexpr const char* AP_PASS = "12345678";
inline constexpr const char* PORTAL_URL = "http://192.168.4.1";

// Switch WiFi to AP mode, start the DNS hijack + HTTP server, serve the
// setup form. Call from the net task (core 0). Never returns.
[[noreturn]] void run();

} // namespace captive_portal
