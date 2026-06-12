# Connectivity API surface for Mini TV apps

Everything network-ish lives in `main/net/`. The supervisor's net task
(core 0) owns WiFi connect/reconnect and the BLE stack — apps never init
these; they only consume the APIs below.

## Threading rule (applies to all of this)

UI callbacks (`build()`/`tick()`/`teardown()`) run on core 1 and must never
block. `web_client::get/post` BLOCK for up to 10 s — call them only from:

- a detached `std::thread` (automatically pinned to core 0 with a 16 KB stack
  via sdkconfig), or
- a FreeRTOS task created with `xTaskCreatePinnedToCore(..., 0)`.

Hand results to the UI through `std::atomic` (plain values) or a
`FreeRTOS queue` (structs/strings — guard strings with a mutex instead).

## WiFi state — `net/wifi_manager.h`

```cpp
bool wifi_manager::is_connected();   // atomic read, UI-safe
const std::string& wifi_manager::ssid();
```

Credentials are set via the captive portal (3 s hold on the top pad). Apps
should render an offline state instead of trying to manage WiFi.

## HTTP(S) — `net/web_client.h`

```cpp
web_client::Response r = web_client::get("https://api.example.com/x");      // blocks!
web_client::Response p = web_client::post(url, body, "application/json");   // blocks!
if (r.ok()) { /* r.status, r.body (default cap 16 KB) */ }
```

HTTPS works out of the box (IDF cert bundle attached). Raise the body cap
only with care — no PSRAM.

Canonical fetch loop for an app that polls an API.

**THREAD-LIFETIME RULE (a real crash taught this): spawn the worker ONCE per
app lifetime, never per open.** App objects are installed once and live
forever, but onOpen()/build() runs on every open. A thread spawned per open
leaks when the user cycles apps rapidly (the old worker hasn't exited yet),
and when pthread creation eventually fails, `std::thread` calls abort() —
C++ exceptions are disabled — rebooting the device. An immortal worker gated
by an atomic has no failure mode.

```cpp
// app_<name>.h members:
bool _worker_started = false;       // UI task only
std::atomic<bool> _active{false};   // open -> worker may fetch
std::atomic<int>  _value{0};
std::atomic<bool> _fresh{false};

// build(): enable fetching; spawn the worker on the FIRST open only
_active.store(true);
if (!_worker_started) {
    _worker_started = true;
    std::thread([this] {            // lands on core 0 via sdkconfig
        int sleep_s = 0;
        while (true) {              // never exits
            if (_active.load() && sleep_s <= 0 && wifi_manager::is_connected()) {
                auto r = web_client::get("https://api.example.com/data");
                if (r.ok()) { _value.store(/* parse r.body */); _fresh.store(true); }
                sleep_s = 60;       // poll cadence in seconds
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (sleep_s > 0) sleep_s--;
        }
    }).detach();
}

// tick(): repaint when fresh
if (_fresh.exchange(false)) lv_label_set_text_fmt(_label, "%d", _value.load());

// teardown(): the worker idles until the next open
_active.store(false);
```

JSON: IDF bundles cJSON (`#include <cJSON.h>`, component `json` is already
linked). Parse on the worker thread, never in tick().

**Working reference:** `main/apps/app_weather/` implements this whole
pattern end-to-end (worker thread, 1 s stop-check loop, atomics handoff,
error/no-wifi states, cJSON parsing). Copy it.

Time: SNTP starts automatically once WiFi connects (net_task), so
`time(nullptr)` / `localtime` are correct a few seconds after going online —
and HTTPS cert validation works. Timezone is UTC unless you `setenv("TZ",...)`.

## BLE — `net/ble_service.h`

The device advertises as **"MiniTV"** with one custom service (128-bit UUID,
see ble_service.cpp): RX characteristic (write) and TX (read/notify).

```cpp
std::string msg;
if (ble_service::take_rx(msg)) { /* newest message written by the phone */ }
ble_service::notify("state=42");          // push to the connected peer
bool up = ble_service::is_connected();
```

`take_rx()` is mutex-guarded and cheap — polling it from `tick()` is the
intended pattern (e.g. a "messages" app that displays whatever the phone
sends). Messages are capped at ~255 bytes (single MTU).

## Captive portal — `net/captive_portal.h`

Started by the supervisor on a 3 s hold; serves WiFi credentials + shell
color/theme setup at `http://192.168.4.1` (AP `MiniTV-Setup` / `12345678`).
Apps never call it. If an app needs a new persisted setting, follow the
theme NVS pattern (`ui/theme.cpp`) — namespace + typed keys + defaults — and
add a field to the portal form in `captive_portal.cpp`.
