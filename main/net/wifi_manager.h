/**
 * @file wifi_manager.h
 * @brief WiFi STA management: NVS-stored credentials, connect, auto-flagged
 *        link state. Runs on the net task (core 0); the UI side only calls
 *        the cheap is_connected() read.
 *
 * Credentials live in NVS namespace "wifi", keys "ssid" / "pass" — set by
 * the captive portal (or `idf.py monitor` + nvs bootstrapping).
 */
#pragma once
#include <string>

namespace wifi_manager {

// esp_netif/event-loop/wifi driver bring-up + load credentials from NVS.
// Does not connect yet. Call once from the net task.
void init();

// True if an SSID is stored.
bool has_credentials();

// Start/again-start the STA connection with the stored credentials.
void connect();

// Link state (set by event handlers; cheap atomic read, UI-safe).
bool is_connected();

// Persist new credentials to NVS (used by the captive portal).
void save_credentials(const std::string& ssid, const std::string& pass);

const std::string& ssid();

// Current station IP as text ("" when not connected). Mutex-guarded copy —
// safe to call from the UI task.
std::string ip();

// True if an SSID exists in NVS. Unlike has_credentials() this reads NVS
// directly and needs no wifi init — usable from the UI task at boot (the
// first-boot hint uses it before the net task has initialised WiFi).
bool peek_credentials();

} // namespace wifi_manager
