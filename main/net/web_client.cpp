/**
 * @file web_client.cpp
 * @brief See web_client.h.
 */
#include "web_client.h"
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>

namespace web_client {

static const char* TAG = "web";

static Response perform(esp_http_client_handle_t client, size_t max_body)
{
    Response r;
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return r;
    }
    esp_http_client_fetch_headers(client);
    r.status = esp_http_client_get_status_code(client);

    char buf[512];
    while (r.body.size() < max_body) {
        const int n = esp_http_client_read(client, buf,
                                           (int)std::min(sizeof(buf), max_body - r.body.size()));
        if (n <= 0) break;
        r.body.append(buf, n);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return r;
}

static esp_http_client_handle_t make_client(const std::string& url)
{
    esp_http_client_config_t cfg = {};
    cfg.url = url.c_str();
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.timeout_ms = 10000;
    return esp_http_client_init(&cfg);
}

Response get(const std::string& url, size_t max_body)
{
    esp_http_client_handle_t client = make_client(url);
    if (!client) return {};
    return perform(client, max_body);
}

Response post(const std::string& url, const std::string& body,
              const char* content_type, size_t max_body)
{
    esp_http_client_handle_t client = make_client(url);
    if (!client) return {};
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", content_type);

    Response r;
    esp_err_t err = esp_http_client_open(client, (int)body.size());
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return r;
    }
    esp_http_client_write(client, body.c_str(), (int)body.size());
    esp_http_client_fetch_headers(client);
    r.status = esp_http_client_get_status_code(client);

    char buf[512];
    while (r.body.size() < max_body) {
        const int n = esp_http_client_read(client, buf,
                                           (int)std::min(sizeof(buf), max_body - r.body.size()));
        if (n <= 0) break;
        r.body.append(buf, n);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return r;
}

} // namespace web_client
