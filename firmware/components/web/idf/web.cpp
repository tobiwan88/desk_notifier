#include "web/idf/web.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

#include "engine/core/errors.hpp"
#include "engine/core/parse.hpp"
#include "engine/idf/render_task.hpp"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "net/idf/net.hpp"
#include "sdkconfig.h"
#include "storage/idf/storage.hpp"

namespace web {
namespace {

const char* const kTag = "web";
std::string g_token;
bool g_open = false;

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
    if (g_open) return true;
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
    const auto ei = engine::eye_info();
    const auto* app = esp_app_get_description();
    char b[896];
    std::snprintf(b, sizeof b,
        R"({"version":"%s","idf":"%s","hostname":"%s","uptime_s":%lld,"heap_internal":%u,"heap_min":%u,"psram_free":%u,)"
        R"("fps":%u.%u,"fill_ms_avg":%lu,"frame_ms_avg":%lu,"frame_ms_max":%lu,"rssi":%d,"ip":"%s","source":{"left":"%.*s","right":"%.*s"},)"
        R"("auth":"%s","render_stack_free":%u,"eye":{"style":"%.*s","iris":"%.*s","auto":%s,"anim":"%.*s"},"fs_free":%lu,"clip_max_bps":%d,"spi_hz":%d})",
        app->version, app->idf_ver, net::hostname().c_str(), static_cast<long long>(esp_timer_get_time() / 1000000),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)), static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)), s.fps_x10 / 10, s.fps_x10 % 10,
        static_cast<unsigned long>(s.fill_us_avg / 1000), static_cast<unsigned long>(s.frame_us_avg / 1000), static_cast<unsigned long>(s.frame_us_max / 1000), net::rssi(), net::ip().c_str(),
        static_cast<int>(s.source[0].size()), s.source[0].data(), static_cast<int>(s.source[1].size()), s.source[1].data(),
        g_open ? "open" : "token", engine::render_stack_free_bytes(),
        static_cast<int>(ei.style_name.size()), ei.style_name.data(), static_cast<int>(ei.iris_name.size()), ei.iris_name.data(),
        ei.auto_idle ? "true" : "false", static_cast<int>(ei.anim.size()), ei.anim.data(), static_cast<unsigned long>(storage::free_bytes()),
        CONFIG_EYES_CLIP_MAX_BPS, CONFIG_EYES_LCD_SPI_HZ);
    send_json(r, "200 OK", b);
    return ESP_OK;
}


// last path segment of /api/clips/<name>
std::string_view uri_name(httpd_req_t* r) {
    std::string_view u = r->uri;
    if (auto q = u.find('?'); q != std::string_view::npos) u = u.substr(0, q);
    constexpr std::string_view p = "/api/clips/";
    return u.size() > p.size() ? u.substr(p.size()) : std::string_view{};
}

esp_err_t get_clips(httpd_req_t* r) {
    if (!authorized(r)) return ESP_OK;
    auto l = storage::list();
    if (!l) { send_error(r, l.error(), "list failed"); return ESP_OK; }
    std::string b = R"({"clips":[)";
    bool first = true;
    for (const auto& c : *l) {
        char e[160];
        std::snprintf(e, sizeof e, R"(%s{"name":"%.31s","size":%lu,"frames":%u,"fps":%u.%u})", first ? "" : ",", c.name.c_str(),
                      static_cast<unsigned long>(c.size), c.frames, c.fps_x10 / 10, c.fps_x10 % 10);
        b += e;
        first = false;
    }
    char t[48];
    std::snprintf(t, sizeof t, R"(],"free":%lu})", static_cast<unsigned long>(storage::free_bytes()));
    b += t;
    send_json(r, "200 OK", b.c_str());
    return ESP_OK;
}

esp_err_t put_clip(httpd_req_t* r) {
    if (!authorized(r)) return ESP_OK;
    const std::string_view name = uri_name(r);
    if (!engine::clip_name_valid(name)) { send_error(r, engine::Err::BadName, "name must match ^[a-z0-9_-]{1,31}$"); return ESP_OK; }
    if (r->content_len == 0) { send_error(r, engine::Err::LengthRequired, "Content-Length required"); return ESP_OK; }
    if (storage::is_playing(name)) { send_error(r, engine::Err::Conflict, "clip is playing"); return ESP_OK; }
    auto up = storage::Upload::begin(name, static_cast<std::uint32_t>(r->content_len));
    if (!up) { send_error(r, up.error(), "upload rejected"); return ESP_OK; }
    constexpr std::size_t kChunk = 4096;
    auto* buf = static_cast<std::byte*>(heap_caps_malloc(kChunk, MALLOC_CAP_SPIRAM));
    if (!buf) buf = static_cast<std::byte*>(heap_caps_malloc(kChunk, MALLOC_CAP_DEFAULT));
    if (!buf) { send_error(r, engine::Err::Io, "no memory"); return ESP_OK; }
    std::size_t left = r->content_len;
    engine::Err err = engine::Err::Ok;
    while (left > 0) {
        const int n = httpd_req_recv(r, reinterpret_cast<char*>(buf), std::min(left, kChunk));
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) { err = engine::Err::Io; break; }
        if (auto w = up->write({buf, static_cast<std::size_t>(n)}); !w) { err = w.error(); break; }
        left -= static_cast<std::size_t>(n);
    }
    heap_caps_free(buf);
    if (err != engine::Err::Ok) { send_error(r, err, "upload failed"); return ESP_OK; }
    auto ci = up->commit();
    if (!ci) { send_error(r, ci.error(), ci.error() == engine::Err::BadClip ? "invalid clip" : "commit failed"); return ESP_OK; }
    char b[160];
    std::snprintf(b, sizeof b, R"({"name":"%.31s","size":%lu,"frames":%u})", ci->name.c_str(), static_cast<unsigned long>(ci->size), ci->frames);
    send_json(r, "201 Created", b);
    return ESP_OK;
}

esp_err_t delete_clip(httpd_req_t* r) {
    if (!authorized(r)) return ESP_OK;
    auto d = storage::remove(uri_name(r));
    if (!d) { send_error(r, d.error(), d.error() == engine::Err::Conflict ? "clip is playing" : "no such clip"); return ESP_OK; }
    httpd_resp_set_status(r, "204 No Content");
    httpd_resp_send(r, nullptr, 0);
    return ESP_OK;
}

std::atomic<bool> g_ota_busy{false};

void reboot_later(void*) {
    vTaskDelay(pdMS_TO_TICKS(800));
    esp_restart();
}

esp_err_t post_ota(httpd_req_t* r) {
    if (!authorized(r)) return ESP_OK;
    if (r->content_len == 0) { send_error(r, engine::Err::LengthRequired, "Content-Length required"); return ESP_OK; }
    const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
    if (!part || r->content_len > part->size) { send_error(r, engine::Err::TooLarge, "image larger than the OTA slot"); return ESP_OK; }
    bool expected = false;
    if (!g_ota_busy.compare_exchange_strong(expected, true)) { send_error(r, engine::Err::Busy, "update already running"); return ESP_OK; }
    struct Guard { ~Guard() { g_ota_busy = false; } } guard;
    esp_ota_handle_t h = 0;
    if (esp_ota_begin(part, r->content_len, &h) != ESP_OK) { send_error(r, engine::Err::Ota, "ota begin failed"); return ESP_OK; }
    constexpr std::size_t kChunk = 4096;
    auto* buf = static_cast<char*>(heap_caps_malloc(kChunk, MALLOC_CAP_SPIRAM));
    if (!buf) buf = static_cast<char*>(heap_caps_malloc(kChunk, MALLOC_CAP_DEFAULT));
    if (!buf) { esp_ota_abort(h); send_error(r, engine::Err::Io, "no memory"); return ESP_OK; }
    std::size_t left = r->content_len;
    bool ok = true;
    while (left > 0) {
        const int n = httpd_req_recv(r, buf, std::min(left, kChunk));
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0 || esp_ota_write(h, buf, static_cast<std::size_t>(n)) != ESP_OK) { ok = false; break; }
        left -= static_cast<std::size_t>(n);
    }
    heap_caps_free(buf);
    if (!ok) { esp_ota_abort(h); send_error(r, engine::Err::Ota, "receive or write failed"); return ESP_OK; }
    if (esp_ota_end(h) != ESP_OK || esp_ota_set_boot_partition(part) != ESP_OK) { send_error(r, engine::Err::Ota, "image rejected"); return ESP_OK; }
    send_json(r, "200 OK", R"({"rebooting":true})");
    xTaskCreate(reboot_later, "reboot", 2048, nullptr, 5, nullptr);
    return ESP_OK;
}

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

esp_err_t get_root(httpd_req_t* r) {
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    httpd_resp_set_hdr(r, "Cache-Control", "no-cache");
    httpd_resp_send(r, index_html_start, index_html_end - index_html_start - 1);  // EMBED_TXTFILES appends a NUL
    return ESP_OK;
}

void append_list(std::string& out, const char* key, const auto& names) {
    out += '"'; out += key; out += "\":[";
    bool first = true;
    for (const auto n : names) { if (!first) out += ','; first = false; out += '"'; out.append(n); out += '"'; }
    out += ']';
}

esp_err_t get_animations(httpd_req_t* r) {  // catalogue + current state; no auth (nothing sensitive)
    std::string b = "{";
    append_list(b, "styles", eyes::kStyleNames); b += ',';
    append_list(b, "irises", eyes::kIrisNames); b += ',';
    append_list(b, "animations", eyes::kAnimNames); b += ",\"specials\":{";
    bool first = true;
    for (std::size_t i = 0; i < eyes::kSpecialNames.size(); ++i) {
        if (eyes::kSpecialNames[i].empty()) continue;
        if (!first) b += ',';
        first = false;
        b += '"'; b.append(eyes::kStyleNames[i]); b += "\":\""; b.append(eyes::kSpecialNames[i]); b += '"';
    }
    const auto i = engine::eye_info();
    b += "},\"state\":{\"style\":\""; b.append(i.style_name); b += "\",\"iris\":\""; b.append(i.iris_name);
    b += "\",\"auto\":"; b += i.auto_idle ? "true" : "false"; b += ",\"anim\":\""; b.append(i.anim); b += "\"}}";
    send_json(r, "200 OK", b.c_str());
    return ESP_OK;
}

}  // namespace

std::expected<void, engine::Err> start() noexcept {
    g_open = net::open_access();
    g_token = net::token();
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.stack_size = 6144;
    c.max_uri_handlers = 16;
    c.recv_wait_timeout = 10;
    c.send_wait_timeout = 10;
    c.max_open_sockets = 4;
    c.lru_purge_enable = true;
    c.uri_match_fn = httpd_uri_match_wildcard;
    httpd_handle_t h = nullptr;
    if (httpd_start(&h, &c) != ESP_OK) return std::unexpected(engine::Err::Io);
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = get_root, .user_ctx = nullptr},
        {.uri = "/api/status", .method = HTTP_GET, .handler = get_status, .user_ctx = nullptr},
        {.uri = "/api/animations", .method = HTTP_GET, .handler = get_animations, .user_ctx = nullptr},
        {.uri = "/api/eye", .method = HTTP_POST, .handler = post_eye, .user_ctx = nullptr},
        {.uri = "/api/play", .method = HTTP_POST, .handler = post_play, .user_ctx = nullptr},
        {.uri = "/api/stop", .method = HTTP_POST, .handler = post_stop, .user_ctx = nullptr},
        {.uri = "/api/ota", .method = HTTP_POST, .handler = post_ota, .user_ctx = nullptr},
        {.uri = "/api/clips", .method = HTTP_GET, .handler = get_clips, .user_ctx = nullptr},
        {.uri = "/api/clips/*", .method = HTTP_PUT, .handler = put_clip, .user_ctx = nullptr},
        {.uri = "/api/clips/*", .method = HTTP_DELETE, .handler = delete_clip, .user_ctx = nullptr},
    };
    for (const auto& u : routes) httpd_register_uri_handler(h, &u);
    ESP_LOGI(kTag, "http server up");
    return {};
}

}  // namespace web
