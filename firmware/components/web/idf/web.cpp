#include "web/idf/web.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <string>

#include "engine/core/errors.hpp"
#include "engine/core/parse.hpp"
#include "engine/idf/render_task.hpp"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "net/idf/net.hpp"
#include "sdkconfig.h"

namespace web {
namespace {

const char* const kTag = "web";
std::string g_token;

void send_error(httpd_req_t* r, engine::Err e, std::string_view msg) {
    const auto info = engine::describe(e);
    char status[24];
    std::snprintf(status, sizeof status, "%d", info.http);
    httpd_resp_set_status(r, status);
    httpd_resp_set_type(r, "application/json");
    char body[160];
    std::snprintf(body, sizeof body, R"({"error":{"code":"%.*s","message":"%.*s"}})", static_cast<int>(info.code.size()),
                  info.code.data(), static_cast<int>(msg.size()), msg.data());
    httpd_resp_sendstr(r, body);
}

void send_json(httpd_req_t* r, const char* status, const char* body) {
    httpd_resp_set_status(r, status);
    httpd_resp_set_type(r, "application/json");
    httpd_resp_sendstr(r, body);
}

bool authorized(httpd_req_t* r) {
    char h[96];
    if (httpd_req_get_hdr_value_str(r, "Authorization", h, sizeof h) != ESP_OK) { send_error(r, engine::Err::Unauthorized, "missing token"); return false; }
    if (!engine::ct_equal(engine::bearer_token(h), g_token)) { send_error(r, engine::Err::Unauthorized, "wrong token"); return false; }
    return true;
}

// Reads a small JSON body into `buf`; sends the error response itself on failure.
bool read_body(httpd_req_t* r, std::array<char, engine::kMaxBody + 1>& buf, std::string_view& out) {
    if (r->content_len > engine::kMaxBody) { send_error(r, engine::Err::TooLarge, "body over 1 KB"); return false; }
    std::size_t got = 0;
    while (got < r->content_len) {
        const int n = httpd_req_recv(r, buf.data() + got, r->content_len - got);
        if (n <= 0) { send_error(r, engine::Err::Io, "receive failed"); return false; }
        got += static_cast<std::size_t>(n);
    }
    out = {buf.data(), got};
    return true;
}

esp_err_t post_eye(httpd_req_t* r) {
    if (!authorized(r)) return ESP_OK;
    std::array<char, engine::kMaxBody + 1> buf;
    std::string_view body;
    if (!read_body(r, buf, body)) return ESP_OK;
    auto cmd = engine::parse_eye(body);
    if (!cmd) { send_error(r, cmd.error().code, cmd.error().message.view()); return ESP_OK; }
    if (!engine::post(engine::Command{*cmd}, 50)) { send_error(r, engine::Err::Busy, "command queue full"); return ESP_OK; }
    send_json(r, "202 Accepted", R"({"queued":true})");
    return ESP_OK;
}

esp_err_t post_play(httpd_req_t* r) {
    if (!authorized(r)) return ESP_OK;
    std::array<char, engine::kMaxBody + 1> buf;
    std::string_view body;
    if (!read_body(r, buf, body)) return ESP_OK;
    auto cmd = engine::parse_play(body);
    if (!cmd) { send_error(r, cmd.error().code, cmd.error().message.view()); return ESP_OK; }
    if (!engine::post(engine::Command{*cmd}, 50)) { send_error(r, engine::Err::Busy, "command queue full"); return ESP_OK; }
    send_json(r, "202 Accepted", R"({"queued":true})");
    return ESP_OK;
}

esp_err_t post_stop(httpd_req_t* r) {
    if (!authorized(r)) return ESP_OK;
    std::array<char, engine::kMaxBody + 1> buf;
    std::string_view body;
    if (!read_body(r, buf, body)) return ESP_OK;  // body ignored
    if (!engine::post(engine::Command{engine::StopCmd{}}, 50)) { send_error(r, engine::Err::Busy, "command queue full"); return ESP_OK; }
    send_json(r, "202 Accepted", R"({"queued":true})");
    return ESP_OK;
}

esp_err_t get_status(httpd_req_t* r) {
    const auto s = engine::stats();
    const auto* app = esp_app_get_description();
    char b[640];
    std::snprintf(b, sizeof b,
        R"({"version":"%s","idf":"%s","hostname":"%s","uptime_s":%lld,"heap_internal":%u,"heap_min":%u,"psram_free":%u,)"
        R"("fps":%u.%u,"frame_ms_avg":%lu,"frame_ms_max":%lu,"rssi":%d,"ip":"%s","source":{"left":"%.*s","right":"%.*s"},)"
        R"("fs_free":0,"clip_max_bps":0,"spi_hz":%d})",
        app->version, app->idf_ver, net::hostname().c_str(), static_cast<long long>(esp_timer_get_time() / 1000000),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)), static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)), s.fps_x10 / 10, s.fps_x10 % 10,
        static_cast<unsigned long>(s.frame_us_avg / 1000), static_cast<unsigned long>(s.frame_us_max / 1000), net::rssi(), net::ip().c_str(),
        static_cast<int>(s.source[0].size()), s.source[0].data(), static_cast<int>(s.source[1].size()), s.source[1].data(), CONFIG_EYES_LCD_SPI_HZ);
    send_json(r, "200 OK", b);
    return ESP_OK;
}

esp_err_t get_root(httpd_req_t* r) {
    send_json(r, "200 OK", R"({"name":"eyes","hint":"use eyes-studio or /api/status"})");
    return ESP_OK;
}

}  // namespace

std::expected<void, engine::Err> start() noexcept {
    g_token = net::token();
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.stack_size = 8192;
    c.max_uri_handlers = 16;
    c.recv_wait_timeout = 10;
    c.send_wait_timeout = 10;
    c.lru_purge_enable = true;
    c.uri_match_fn = httpd_uri_match_wildcard;
    httpd_handle_t h = nullptr;
    if (httpd_start(&h, &c) != ESP_OK) return std::unexpected(engine::Err::Io);
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = get_root, .user_ctx = nullptr},
        {.uri = "/api/status", .method = HTTP_GET, .handler = get_status, .user_ctx = nullptr},
        {.uri = "/api/eye", .method = HTTP_POST, .handler = post_eye, .user_ctx = nullptr},
        {.uri = "/api/play", .method = HTTP_POST, .handler = post_play, .user_ctx = nullptr},
        {.uri = "/api/stop", .method = HTTP_POST, .handler = post_stop, .user_ctx = nullptr},
    };
    for (const auto& u : routes) httpd_register_uri_handler(h, &u);
    ESP_LOGI(kTag, "http server up");
    return {};
}

}  // namespace web
