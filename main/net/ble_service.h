/**
 * @file ble_service.h
 * @brief NimBLE GATT server: the device advertises as "MiniTV" with one
 *        custom service apps can use as a message pipe:
 *          - RX characteristic (write):        phone/host -> device
 *          - TX characteristic (read|notify):  device -> phone/host
 *
 * Runs entirely on core 0 (NimBLE host task). Apps consume RX messages with
 * the UI-safe take_rx() poll from tick(), and push updates with notify().
 */
#pragma once
#include <string>

namespace ble_service {

// Start the NimBLE stack, register the GATT service, begin advertising.
// Call once from the net task (after NVS init). Safe to skip entirely if
// BLE isn't wanted — nothing else depends on it.
void start(const char* device_name = "MiniTV");

bool is_connected();

// One-shot: pops the most recent message written to RX (true if there was
// one). Cheap and mutex-guarded — fine to call from the UI task's tick().
bool take_rx(std::string& out);

// Send a message to the connected peer via TX notify (no-op when not
// connected or message exceeds MTU).
void notify(const std::string& msg);

} // namespace ble_service
