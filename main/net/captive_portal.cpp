/**
 * @file captive_portal.cpp
 * @brief See captive_portal.h. Pure ESP-IDF: esp_wifi SoftAP, a tiny UDP
 *        DNS responder that answers every A query with 192.168.4.1 (so
 *        phones pop their captive-portal sheet), and esp_http_server.
 */
#include "captive_portal.h"
#include "wifi_manager.h"
#include "../ui/theme.h"

#include <cstring>
#include <string>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_http_server.h>
#include <esp_system.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>

namespace captive_portal {

static const char* TAG = "portal";

/* ----------------------------- DNS hijack -------------------------------- */
// Minimal DNS responder: every query gets an A record pointing at the AP IP.
static void dns_task(void* /*arg*/)
{
    const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sock, (sockaddr*)&addr, sizeof(addr));

    uint8_t buf[512];
    while (true) {
        sockaddr_in src = {};
        socklen_t slen = sizeof(src);
        const int len = recvfrom(sock, buf, sizeof(buf) - 16, 0, (sockaddr*)&src, &slen);
        if (len < 12) continue;

        buf[2] = 0x81; buf[3] = 0x80;            // response, recursion available
        buf[6] = buf[4]; buf[7] = buf[5];        // ANCOUNT = QDCOUNT
        buf[8] = 0; buf[9] = 0; buf[10] = 0; buf[11] = 0;

        int p = len;
        buf[p++] = 0xC0; buf[p++] = 0x0C;        // name: pointer to question
        buf[p++] = 0x00; buf[p++] = 0x01;        // type A
        buf[p++] = 0x00; buf[p++] = 0x01;        // class IN
        buf[p++] = 0; buf[p++] = 0; buf[p++] = 0; buf[p++] = 30;   // TTL 30s
        buf[p++] = 0x00; buf[p++] = 0x04;        // RDLENGTH
        buf[p++] = 192; buf[p++] = 168; buf[p++] = 4; buf[p++] = 1; // 192.168.4.1

        sendto(sock, buf, p, 0, (sockaddr*)&src, slen);
    }
}

/* ------------------------------ HTML form -------------------------------- */
static std::string html_page()
{
    const char* shell = theme::shell_name();
    auto sel = [&](const char* v) { return strcmp(shell, v) == 0 ? " checked" : ""; };

    std::string h;
    h.reserve(4096);
    h += "<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
         "<title>Mini TV Setup</title><style>"
         "body{font-family:sans-serif;background:#0e0e12;color:#f2f2f2;margin:0;padding:24px}"
         "h1{color:#ff9f1c;font-size:1.4em}fieldset{border:1px solid #333;border-radius:8px;"
         "margin:0 0 16px;padding:12px}legend{color:#9a9aa4}label{display:block;margin:8px 0 4px}"
         "input[type=text],input[type=password]{width:100%;box-sizing:border-box;padding:8px;"
         "border-radius:6px;border:1px solid #444;background:#1c1c24;color:#f2f2f2}"
         ".sw{display:inline-flex;align-items:center;margin-right:14px}"
         ".dot{width:18px;height:18px;border-radius:50%;display:inline-block;margin:0 6px;"
         "border:1px solid #666}"
         "button{margin-top:8px;width:100%;padding:12px;border:0;border-radius:8px;"
         "background:#ff9f1c;color:#000;font-size:1em;font-weight:bold}"
         ".cust{display:none}#sc:checked~.cust{display:block}"
         "</style></head><body><h1>Mini TV Setup</h1><form method='POST' action='/save'>";

    h += "<fieldset><legend>WiFi</legend>"
         "<label>Network name (SSID)</label><input type=text name=ssid value='" +
         wifi_manager::ssid() + "'>"
         "<label>Password</label><input type=password name=pass></fieldset>";

    h += "<fieldset><legend>Device shell color</legend>"
         "<p style='color:#9a9aa4;margin:4px 0 10px'>Pick the color of your Mini TV's case - "
         "the screen theme is matched to it.</p>";
    h += std::string("<span class=sw><input type=radio name=shell value=black") + sel("black") +
         "><span class=dot style='background:#111'></span>Black</span>";
    h += std::string("<span class=sw><input type=radio name=shell value=white") + sel("white") +
         "><span class=dot style='background:#f5f5f7'></span>White</span>";
    h += std::string("<span class=sw><input type=radio name=shell value=orange") + sel("orange") +
         "><span class=dot style='background:#ff7a00'></span>Orange</span>";
    // The custom radio must be a direct SIBLING of the .cust div — the
    // `#sc:checked~.cust` CSS reveal only works between siblings.
    h += std::string("<br><input id=sc type=radio name=shell value=custom") +
         sel("custom") + "><label for=sc style='display:inline;margin-left:6px'>Custom palette</label>"
         "<div class=cust>"
         "<label>Background</label><input type=text name=c_bg placeholder='#0e0e12'>"
         "<label>Surface</label><input type=text name=c_surface placeholder='#1c1c24'>"
         "<label>Text</label><input type=text name=c_text placeholder='#f2f2f2'>"
         "<label>Muted text</label><input type=text name=c_muted placeholder='#9a9aa4'>"
         "<label>Accent</label><input type=text name=c_accent placeholder='#ff9f1c'>"
         "<label>Second accent</label><input type=text name=c_accent2 placeholder='#4cc9f0'>"
         "</div></fieldset>";

    h += "<button type=submit>Save &amp; restart</button></form>";

    // Escape hatch: wipes every stored setting (WiFi, theme, app data).
    h += "<form method='POST' action='/reset' "
         "onsubmit=\"return confirm('Erase ALL settings (WiFi, theme, app data)?')\">"
         "<button type=submit style='background:#444;color:#ff6b6b;margin-top:16px'>"
         "Factory reset</button></form></body></html>";
    return h;
}

/* --------------------------- form decoding ------------------------------- */
static std::string url_decode(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '+') {
            out += ' ';
        } else if (in[i] == '%' && i + 2 < in.size()) {
            const char hex[3] = {in[i + 1], in[i + 2], 0};
            out += (char)strtol(hex, nullptr, 16);
            i += 2;
        } else {
            out += in[i];
        }
    }
    return out;
}

static std::string form_field(const std::string& body, const std::string& key)
{
    const std::string needle = key + "=";
    size_t pos = 0;
    while (pos < body.size()) {
        size_t end = body.find('&', pos);
        if (end == std::string::npos) end = body.size();
        if (body.compare(pos, needle.size(), needle) == 0) {
            return url_decode(body.substr(pos + needle.size(), end - pos - needle.size()));
        }
        pos = end + 1;
    }
    return "";
}

// "#rrggbb" / "rrggbb" -> color; falls back to `fallback` when empty/invalid.
static lv_color_t parse_hex(const std::string& s, lv_color_t fallback)
{
    std::string t = s;
    if (!t.empty() && t[0] == '#') t = t.substr(1);
    if (t.size() != 6) return fallback;
    char* endp = nullptr;
    const long v = strtol(t.c_str(), &endp, 16);
    if (endp != t.c_str() + 6) return fallback;
    return lv_color_hex((uint32_t)v);
}

/* ----------------------------- HTTP server ------------------------------- */
static esp_err_t root_get(httpd_req_t* req)
{
    const std::string page = html_page();
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, page.c_str(), (ssize_t)page.size());
    return ESP_OK;
}

static esp_err_t save_post(httpd_req_t* req)
{
    std::string body;
    char buf[256];
    int remaining = (int)req->content_len;
    while (remaining > 0) {
        const int n = httpd_req_recv(req, buf, std::min(remaining, (int)sizeof(buf)));
        if (n <= 0) return ESP_FAIL;
        body.append(buf, n);
        remaining -= n;
    }

    const std::string ssid = form_field(body, "ssid");
    const std::string pass = form_field(body, "pass");
    const std::string shell = form_field(body, "shell");

    if (!ssid.empty()) wifi_manager::save_credentials(ssid, pass);

    if (shell == "custom") {
        const auto& cur = theme::palette();
        theme::Palette p;
        p.bg         = parse_hex(form_field(body, "c_bg"), cur.bg);
        p.surface    = parse_hex(form_field(body, "c_surface"), cur.surface);
        p.text       = parse_hex(form_field(body, "c_text"), cur.text);
        p.text_muted = parse_hex(form_field(body, "c_muted"), cur.text_muted);
        p.accent     = parse_hex(form_field(body, "c_accent"), cur.accent);
        p.accent_alt = parse_hex(form_field(body, "c_accent2"), cur.accent_alt);
        theme::set_custom(p);
    } else if (!shell.empty()) {
        theme::set_shell_by_name(shell.c_str());
    }

    const char* done =
        "<html><body style='font-family:sans-serif;background:#0e0e12;color:#f2f2f2;"
        "text-align:center;padding-top:40px'><h1 style='color:#ff9f1c'>Saved</h1>"
        "<p>The Mini TV is restarting...</p></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, done, HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "settings saved, rebooting");
    vTaskDelay(pdMS_TO_TICKS(800));
    esp_restart();
    return ESP_OK;
}

static esp_err_t reset_post(httpd_req_t* req)
{
    const char* done =
        "<html><body style='font-family:sans-serif;background:#0e0e12;color:#f2f2f2;"
        "text-align:center;padding-top:40px'><h1 style='color:#ff6b6b'>Factory reset</h1>"
        "<p>All settings erased. The Mini TV is restarting...</p></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, done, HTTPD_RESP_USE_STRLEN);

    ESP_LOGW(TAG, "factory reset: erasing NVS and rebooting");
    vTaskDelay(pdMS_TO_TICKS(800));
    nvs_flash_erase(); // wipes wifi creds, theme, and all app namespaces
    esp_restart();
    return ESP_OK;
}

// Anything else (connectivity checks etc.) -> redirect to the portal.
static esp_err_t redirect_handler(httpd_req_t* req, httpd_err_code_t /*err*/)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", PORTAL_URL);
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

void run()
{
    ESP_LOGI(TAG, "starting captive portal (AP=%s)", AP_SSID);

    esp_wifi_stop();
    esp_netif_create_default_wifi_ap();

    wifi_config_t ap = {};
    strncpy((char*)ap.ap.ssid, AP_SSID, sizeof(ap.ap.ssid) - 1);
    strncpy((char*)ap.ap.password, AP_PASS, sizeof(ap.ap.password) - 1);
    ap.ap.ssid_len = strlen(AP_SSID);
    ap.ap.max_connection = 2;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    xTaskCreatePinnedToCore(dns_task, "dns", 3072, nullptr, 3, nullptr, 0);

    httpd_handle_t server = nullptr;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.core_id = 0;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));

    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get, .user_ctx = nullptr};
    const httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = save_post, .user_ctx = nullptr};
    const httpd_uri_t reset = {.uri = "/reset", .method = HTTP_POST, .handler = reset_post, .user_ctx = nullptr};
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);
    httpd_register_uri_handler(server, &reset);
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, redirect_handler);

    ESP_LOGI(TAG, "portal up at %s", PORTAL_URL);
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // parked: server runs on its own task
    }
}

} // namespace captive_portal
