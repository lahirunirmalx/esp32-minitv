/**
 * @file ble_service.cpp
 * @brief See ble_service.h. Standard NimBLE peripheral shape (bleprph):
 *        sync -> advertise -> on disconnect re-advertise.
 */
#include "ble_service.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <esp_log.h>
#include <nvs_flash.h>

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

namespace ble_service {

static const char* TAG = "ble";

static std::atomic<bool> s_connected{false};
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_tx_attr_handle = 0;
static uint8_t s_own_addr_type = 0;
static std::string s_device_name = "MiniTV";

static std::mutex s_rx_mutex;
static std::string s_rx;        // most recent RX message
static bool s_rx_fresh = false;
static std::string s_tx;        // value served on TX reads

// Service  6f6d0001-... ("minitv message pipe"), RX ...0002, TX ...0003
static const ble_uuid128_t kSvcUuid =
    BLE_UUID128_INIT(0x4d, 0x54, 0x56, 0x00, 0x00, 0x00, 0x10, 0x00,
                     0x80, 0x00, 0x00, 0x80, 0x01, 0x00, 0x6d, 0x6f);
static const ble_uuid128_t kRxUuid =
    BLE_UUID128_INIT(0x4d, 0x54, 0x56, 0x00, 0x00, 0x00, 0x10, 0x00,
                     0x80, 0x00, 0x00, 0x80, 0x02, 0x00, 0x6d, 0x6f);
static const ble_uuid128_t kTxUuid =
    BLE_UUID128_INIT(0x4d, 0x54, 0x56, 0x00, 0x00, 0x00, 0x10, 0x00,
                     0x80, 0x00, 0x00, 0x80, 0x03, 0x00, 0x6d, 0x6f);

static int chr_access(uint16_t /*conn*/, uint16_t /*attr*/,
                      ble_gatt_access_ctxt* ctxt, void* /*arg*/)
{
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR: {
            char buf[256];
            uint16_t len = 0;
            ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &len);
            buf[len] = 0;
            {
                std::lock_guard<std::mutex> lock(s_rx_mutex);
                s_rx.assign(buf, len);
                s_rx_fresh = true;
            }
            ESP_LOGI(TAG, "rx %u bytes", len);
            return 0;
        }
        case BLE_GATT_ACCESS_OP_READ_CHR: {
            std::lock_guard<std::mutex> lock(s_rx_mutex);
            return os_mbuf_append(ctxt->om, s_tx.data(), (uint16_t)s_tx.size()) == 0
                       ? 0
                       : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        default:
            return BLE_ATT_ERR_UNLIKELY;
    }
}

static const ble_gatt_chr_def kChrs[] = {
    {
        .uuid = &kRxUuid.u,
        .access_cb = chr_access,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
    },
    {
        .uuid = &kTxUuid.u,
        .access_cb = chr_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &s_tx_attr_handle,
    },
    {}, // terminator
};

static const ble_gatt_svc_def kSvcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &kSvcUuid.u,
        .characteristics = kChrs,
    },
    {}, // terminator
};

static void advertise();

static int gap_event(ble_gap_event* event, void* /*arg*/)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                s_conn_handle = event->connect.conn_handle;
                s_connected.store(true);
                ESP_LOGI(TAG, "connected");
            } else {
                advertise();
            }
            return 0;
        case BLE_GAP_EVENT_DISCONNECT:
            s_connected.store(false);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            ESP_LOGI(TAG, "disconnected, re-advertising");
            advertise();
            return 0;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            advertise();
            return 0;
        default:
            return 0;
    }
}

static void advertise()
{
    ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t*)s_device_name.c_str();
    fields.name_len = (uint8_t)s_device_name.size();
    fields.name_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGW(TAG, "adv_set_fields rc=%d", rc);
        return;
    }

    ble_gap_adv_params adv = {};
    adv.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_own_addr_type, nullptr, BLE_HS_FOREVER, &adv,
                           gap_event, nullptr);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGW(TAG, "adv_start rc=%d", rc);
    }
}

static void on_sync()
{
    ble_hs_id_infer_auto(0, &s_own_addr_type);
    advertise();
    ESP_LOGI(TAG, "advertising as %s", s_device_name.c_str());
}

static void host_task(void* /*param*/)
{
    nimble_port_run();             // returns only on nimble_port_stop()
    nimble_port_freertos_deinit();
}

void start(const char* device_name)
{
    s_device_name = device_name;

    if (nimble_port_init() != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed");
        return;
    }

    ble_hs_cfg.sync_cb = on_sync;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(device_name);

    int rc = ble_gatts_count_cfg(kSvcs);
    if (rc == 0) rc = ble_gatts_add_svcs(kSvcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatt registration rc=%d", rc);
        return;
    }

    nimble_port_freertos_init(host_task);
}

bool is_connected() { return s_connected.load(); }

bool take_rx(std::string& out)
{
    std::lock_guard<std::mutex> lock(s_rx_mutex);
    if (!s_rx_fresh) return false;
    out = s_rx;
    s_rx_fresh = false;
    return true;
}

void notify(const std::string& msg)
{
    {
        std::lock_guard<std::mutex> lock(s_rx_mutex);
        s_tx = msg;
    }
    if (!s_connected.load() || s_tx_attr_handle == 0) return;
    os_mbuf* om = ble_hs_mbuf_from_flat(msg.data(), (uint16_t)msg.size());
    if (!om) return;
    ble_gatts_notify_custom(s_conn_handle, s_tx_attr_handle, om);
}

} // namespace ble_service
