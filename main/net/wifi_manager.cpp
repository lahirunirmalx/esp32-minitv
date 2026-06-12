/**
 * @file wifi_manager.cpp
 * @brief See wifi_manager.h. Pure esp_wifi (no Arduino).
 */
#include "wifi_manager.h"
#include <atomic>
#include <cstring>
#include <mutex>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_log.h>
#include <nvs.h>

namespace wifi_manager {

static const char* TAG = "wifi";
static const char* NVS_NS = "wifi";

static std::atomic<bool> s_connected{false};
static std::string s_ssid;
static std::string s_pass;
static bool s_started = false;
static std::mutex s_ip_mutex;
static std::string s_ip;

static void event_handler(void* /*arg*/, esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected.store(false);
        {
            std::lock_guard<std::mutex> lock(s_ip_mutex);
            s_ip.clear();
        }
        // Reconnect cadence is owned by the net task loop, not here — a tight
        // esp_wifi_connect() retry loop here would starve other net work.
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* ev = (ip_event_got_ip_t*)data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&ev->ip_info.ip));
        char buf[16];
        snprintf(buf, sizeof(buf), IPSTR, IP2STR(&ev->ip_info.ip));
        {
            std::lock_guard<std::mutex> lock(s_ip_mutex);
            s_ip = buf;
        }
        s_connected.store(true);
    }
}

static void load_credentials()
{
    s_ssid.clear();
    s_pass.clear();
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    char buf[65] = {};
    size_t len = sizeof(buf);
    if (nvs_get_str(h, "ssid", buf, &len) == ESP_OK) s_ssid = buf;
    len = sizeof(buf);
    if (nvs_get_str(h, "pass", buf, &len) == ESP_OK) s_pass = buf;
    nvs_close(h);
}

void init()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &event_handler, nullptr, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &event_handler, nullptr, nullptr));

    load_credentials();
    ESP_LOGI(TAG, "init, ssid=%s", s_ssid.empty() ? "<none>" : s_ssid.c_str());
}

bool has_credentials() { return !s_ssid.empty(); }

void connect()
{
    if (s_ssid.empty()) return;

    wifi_config_t wc = {};
    strncpy((char*)wc.sta.ssid, s_ssid.c_str(), sizeof(wc.sta.ssid) - 1);
    strncpy((char*)wc.sta.password, s_pass.c_str(), sizeof(wc.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));

    if (!s_started) {
        ESP_ERROR_CHECK(esp_wifi_start()); // STA_START event triggers connect
        s_started = true;
    } else {
        esp_wifi_connect();
    }
}

bool is_connected() { return s_connected.load(); }

void save_credentials(const std::string& ssid, const std::string& pass)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NVS_NS, NVS_READWRITE, &h));
    nvs_set_str(h, "ssid", ssid.c_str());
    nvs_set_str(h, "pass", pass.c_str());
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
    s_ssid = ssid;
    s_pass = pass;
    ESP_LOGI(TAG, "credentials saved (ssid=%s)", ssid.c_str());
}

const std::string& ssid() { return s_ssid; }

std::string ip()
{
    std::lock_guard<std::mutex> lock(s_ip_mutex);
    return s_ip;
}

bool peek_credentials()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = 0;
    const bool found = (nvs_get_str(h, "ssid", nullptr, &len) == ESP_OK && len > 1);
    nvs_close(h);
    return found;
}

} // namespace wifi_manager
