/**
 * @file web_client.h
 * @brief Simple blocking HTTP(S) client wrapper over esp_http_client with the
 *        ESP-IDF certificate bundle attached, so https:// URLs just work.
 *
 * THREADING: these calls BLOCK for the duration of the request — never call
 * them from the UI task. Run them on the net task, a std::thread (pinned to
 * core 0 via sdkconfig), or a dedicated worker, then hand the result to the
 * UI through a queue/atomic. Pattern:
 *
 *   std::thread([this] {                       // lands on core 0
 *       web_client::Response r = web_client::get("https://api.example.com/x");
 *       if (r.ok()) { ... parse ...; _shared_state.store(...); }
 *   }).detach();
 *   // UI tick() reads _shared_state — never the Response directly.
 */
#pragma once
#include <string>

namespace web_client {

struct Response {
    int status = -1;       // HTTP status, or -1 on transport error
    std::string body;      // capped at max_body bytes
    bool ok() const { return status >= 200 && status < 300; }
};

// Blocking GET. max_body caps RAM use (no PSRAM on this board!).
Response get(const std::string& url, size_t max_body = 16 * 1024);

// Blocking POST with a request body (content_type e.g. "application/json").
Response post(const std::string& url, const std::string& body,
              const char* content_type = "application/json",
              size_t max_body = 16 * 1024);

} // namespace web_client
