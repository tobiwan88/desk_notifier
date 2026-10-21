#include "web/idf/web.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

#include "engine/core/errors.hpp"
#include "clock/idf/clock.hpp"
#include "engine/core/names.hpp"
#include "screen/idf/screen_task.hpp"
#include "sensor/idf/bme688.hpp"
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
#include "settings/idf/settings.hpp"
#include "storage/idf/storage.hpp"
#include "weather/idf/client.hpp"

namespace web {
namespace {

const char* const kTag = "web";
std::string g_token;
bool g_open = false;

void send_error(httpd_req_t* r, engine::Err e, std::string_view msg) {  // msg must not contain quotes
    const auto info = engine::describe(e);
    char status[24];
    std::snprintf(status, sizeof status, "%d", info.http);
    httpd_resp_set_status(r, status);
    httpd_resp_set_type(r, "application/json");
    char body[200];
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

esp_err_t get_status(httpd_req_t* r) {
    const auto sc = screen::stats();
    const auto sn = sensor::latest();
    const auto* app = esp_app_get_description();
    char b[1024];
    std::snprintf(b, sizeof b,
        R"({"version":"%s","idf":"%s","hostname":"%s","uptime_s":%lld,"heap_internal":%u,"heap_min":%u,"psram_free":%u,)"
        R"("frame_ms_avg":%lu,"frame_ms_max":%lu,"rssi":%d,"ip":"%s","time_valid":%s,"auth":"%s","resets":"%s",)"
        R"("sensor":{"ok":%s,"errors":%d,"i2c":"%s"},"alert":%s,"snoozed":%s,"fs_free":%lu,"spi_hz":%d,"page":"%s"})",
        app->version, app->idf_ver, net::hostname().c_str(), static_cast<long long>(esp_timer_get_time() / 1000000),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)), static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
        static_cast<unsigned long>(sc.frame_us_avg / 1000), static_cast<unsigned long>(sc.frame_us_max / 1000), net::rssi(), net::ip().c_str(),
        clock_sync::valid() ? "true" : "false", g_open ? "open" : "token", settings::get("rst").value_or("").c_str(),
        sn.ok ? "true" : "false", sn.errors, sensor::i2c_scan().c_str(), sc.alert ? "true" : "false", sc.snoozed ? "true" : "false",
        static_cast<unsigned long>(storage::free_bytes()), CONFIG_DESK_LCD_SPI_HZ, screen::page_name(screen::page()));
    send_json(r, "200 OK", b);
    return ESP_OK;
}

esp_err_t get_sensor(httpd_req_t* r) {
    const auto s = sensor::latest();
    char b[320];
    std::snprintf(b, sizeof b,
        R"({"ok":%s,"temp_c":%.2f,"humidity":%.1f,"pressure_hpa":%.1f,"gas_ohm":%.0f,"gas_valid":%s,"score":%d,"level":"%s","age_ms":%lu})",
        s.ok ? "true" : "false", static_cast<double>(s.reading.temp_c), static_cast<double>(s.reading.rh),
        static_cast<double>(s.reading.hpa), static_cast<double>(s.reading.gas_ohm), s.reading.gas_valid ? "true" : "false",
        s.quality.score, sensor::level_name(s.quality.level), static_cast<unsigned long>(s.age_ms));
    send_json(r, "200 OK", b);
    return ESP_OK;
}

esp_err_t get_weather(httpd_req_t* r) {
    const auto w = weather::latest();
    char b[320];
    std::snprintf(b, sizeof b,
        R"({"ok":%s,"place":"%s","temp_c":%.1f,"is_day":%s,"code":%d,"sky":"%s","max_c":%.1f,"min_c":%.1f,"precip_pct":%d,"wind_kmh":%.1f,)"
        R"("age_s":%lu,"errors":%d,"http":%d})",
        w.ok ? "true" : "false", weather::place().c_str(), static_cast<double>(w.forecast.temp_c), w.forecast.is_day ? "true" : "false",
        w.forecast.code, weather::sky_name(w.forecast.sky, w.forecast.is_day), static_cast<double>(w.forecast.max_c),
        static_cast<double>(w.forecast.min_c), w.forecast.precip_pct, static_cast<double>(w.forecast.wind_kmh), static_cast<unsigned long>(w.age_s), w.errors, w.http_status);
    send_json(r, "200 OK", b);
    return ESP_OK;
}

// Value of "key" in a flat JSON object, as text (quotes removed). Empty if missing.
std::string json_value(std::string_view b, std::string_view key) {
    const std::string k = std::string("\"") + std::string(key) + "\"";
    std::size_t p = b.find(k);
    if (p == std::string_view::npos) return {};
    p = b.find(':', p + k.size());
    if (p == std::string_view::npos) return {};
    ++p;
    while (p < b.size() && (b[p] == ' ' || b[p] == '"')) ++p;
    std::size_t e = p;
    while (e < b.size() && b[e] != '"' && b[e] != ',' && b[e] != '}' && b[e] != ' ') ++e;
    return std::string(b.substr(p, e - p));
}

// {"lat":48.137,"lon":11.575,"place":"Munich"}: saves the forecast location and fetches again.
esp_err_t post_weather(httpd_req_t* r) {
    if (!authorized(r)) return ESP_OK;
    char body[128]{};
    const int n = httpd_req_recv(r, body, std::min<std::size_t>(r->content_len, sizeof body - 1));
    if (n <= 0) { send_error(r, engine::Err::BadJson, "empty body"); return ESP_OK; }
    const std::string_view b(body, static_cast<std::size_t>(n));
    const std::string lat = json_value(b, "lat"), lon = json_value(b, "lon"), place = json_value(b, "place");
    auto numeric = [](const std::string& v) {
        return !v.empty() && v.size() < 12 && v.find_first_not_of("+-.0123456789") == std::string::npos;
    };
    if (!numeric(lat) || !numeric(lon)) { send_error(r, engine::Err::BadJson, "lat and lon must be numbers"); return ESP_OK; }
    settings::set("wx_lat", lat);
    settings::set("wx_lon", lon);
    if (!place.empty() && place.size() < 20) settings::set("wx_place", place);
    weather::refresh();
    send_json(r, "202 Accepted", R"({"saved":true,"note":"the place name shows after a reboot"})");
    return ESP_OK;
}

// {"temp":3,"sky":"rain","day":true,"wind":45,"seconds":60}: shows a made-up forecast for a while (outfit demo).
// sky: clear, partly, cloudy, fog, drizzle, rain, snow, thunder. {"seconds":0} switches the demo off.
esp_err_t post_weather_demo(httpd_req_t* r) {
    if (!authorized(r)) return ESP_OK;
    char body[160]{};
    const int n = httpd_req_recv(r, body, std::min<std::size_t>(r->content_len, sizeof body - 1));
    if (n <= 0) { send_error(r, engine::Err::BadJson, "empty body"); return ESP_OK; }
    const std::string_view b(body, static_cast<std::size_t>(n));
    auto num = [&](const char* key, float d) {
        const std::string v = json_value(b, key);
        return v.empty() ? d : static_cast<float>(std::atof(v.c_str()));
    };
    static constexpr const char* kSkies[] = {"clear", "partly", "cloudy", "fog", "drizzle", "rain", "snow", "thunder"};
    weather::Forecast f;
    f.temp_c = num("temp", 15);
    f.max_c = f.temp_c + 2;
    f.min_c = f.temp_c - 4;
    f.wind_kmh = num("wind", 0);
    f.is_day = json_value(b, "day") != "false";
    const std::string sky = json_value(b, "sky");
    for (int i = 0; i < 8; ++i)
        if (sky == kSkies[i]) f.sky = static_cast<weather::Sky>(i);
    f.precip_pct = (f.sky >= weather::Sky::Drizzle && f.sky != weather::Sky::Snow) ? 80 : 10;
    weather::set_demo(f, static_cast<std::uint32_t>(num("seconds", 60)));
    send_json(r, "202 Accepted", R"({"ok":true})");
    return ESP_OK;
}

// {"page":"clock"|"air"|"day"} or {"page":"next"} ("weather" still works and means "day"); {"refresh_weather":true} fetches the forecast again.
esp_err_t post_page(httpd_req_t* r) {
    if (!authorized(r)) return ESP_OK;
    char body[96]{};
    const int n = httpd_req_recv(r, body, std::min<std::size_t>(r->content_len, sizeof body - 1));
    if (n <= 0) { send_error(r, engine::Err::BadJson, "empty body"); return ESP_OK; }
    const std::string_view b(body, static_cast<std::size_t>(n));
    if (b.find("refresh_weather") != std::string_view::npos) weather::refresh();
    if (b.find("\"next\"") != std::string_view::npos) {
        screen::next_page();
    } else {
        bool found = false;
        for (int i = 0; i < screen::kPageCount; ++i) {
            const auto p = static_cast<screen::Page>(i);
            const std::string quoted = std::string("\"") + screen::page_name(p) + "\"";
            if (b.find(quoted) != std::string_view::npos) { screen::show_page(p); found = true; break; }
        }
        if (!found && b.find("\"weather\"") != std::string_view::npos) { screen::show_page(screen::Page::Day); found = true; }
        if (!found && b.find("refresh_weather") == std::string_view::npos) { send_error(r, engine::Err::BadJson, "page must be clock, air, day or next"); return ESP_OK; }
    }
    send_json(r, "202 Accepted", R"({"ok":true})");
    return ESP_OK;
}

esp_err_t post_ack(httpd_req_t* r) {
    if (!authorized(r)) return ESP_OK;
    screen::ack();
    send_json(r, "202 Accepted", R"({"acked":true})");
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

// ---- the pet page: open to everyone on the local network, no token ------------------------------------------

void pet_json(std::string& out, const screen::Bond& b, const char* name, bool time_valid) {
    const int lvl = screen::level(b);
    char buf[400];
    std::string done = "[", gifts = "[";
    for (int i = 0; i < screen::kActionCount; ++i)
        if ((b.done >> i) & 1u) { done += done.size() > 1 ? ",\"" : "\""; done += screen::action_name(static_cast<screen::Action>(i)); done += '"'; }
    for (int i = 0; i < screen::kGiftCount; ++i)
        if ((b.gifts >> i) & 1u) { gifts += gifts.size() > 1 ? ",\"" : "\""; gifts += screen::gift_name(i); gifts += '"'; }
    done += ']'; gifts += ']';
    std::snprintf(buf, sizeof buf,
                  R"("level":%d,"hearts":%d,"level_name":"%s","score":%d,"next":%d,"streak":%d,"best_streak":%d,"visits":%d,"name":"%s","time_valid":%s,)",
                  lvl, lvl + 1, screen::level_name(lvl), b.score, lvl >= screen::kLevels - 1 ? 100 : (lvl + 1) * 20, b.streak,
                  b.best_streak, b.total, name, time_valid ? "true" : "false");
    out += buf;
    out += R"("done_today":)" + done + R"(,"gifts":)" + gifts;
}

extern const char pet_html_start[] asm("_binary_pet_html_start");
extern const char pet_html_end[] asm("_binary_pet_html_end");

esp_err_t get_pet_page(httpd_req_t* r) {
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    httpd_resp_set_hdr(r, "Cache-Control", "no-cache");
    httpd_resp_send(r, pet_html_start, pet_html_end - pet_html_start - 1);
    return ESP_OK;
}

esp_err_t get_pet(httpd_req_t* r) {
    const auto s = screen::pet_status();
    std::string out = "{";
    pet_json(out, s.bond, s.name, s.time_valid);
    out += "}";
    send_json(r, "200 OK", out.c_str());
    return ESP_OK;
}

// {"action":"feed","item":"hazelnut"} | {"action":"gift","item":"shell"} | {"action":"party"} | {"name":"Nutty"}
esp_err_t post_pet(httpd_req_t* r) {
    char body[128]{};
    const int n = r->content_len > 0 ? httpd_req_recv(r, body, std::min<std::size_t>(r->content_len, sizeof body - 1)) : 0;
    const std::string_view b(body, static_cast<std::size_t>(std::max(n, 0)));
    if (b.find("\"name\"") != std::string_view::npos) {
        screen::set_pet_name(json_value(b, "name"));
        return get_pet(r);
    }
    const auto action = screen::action_from_name(json_value(b, "action"));
    if (!action) { send_error(r, engine::Err::BadJson, "action: feed, coffee, party, pet, play, water, goodnight or gift"); return ESP_OK; }
    const std::string item = json_value(b, "item");
    int index = 0;
    for (int i = 0; i < (*action == screen::Action::Gift ? screen::kGiftCount : screen::kTreatCount); ++i)
        if (item == (*action == screen::Action::Gift ? screen::gift_name(i) : screen::treat_name(i))) index = i;
    const auto res = screen::pet(*action, index);
    if (!res) { send_json(r, "503 Service Unavailable", R"({"ok":false,"message":"the squirrel is still waking up, try again in a minute"})"); return ESP_OK; }
    const auto& x = res->reaction;
    const auto s = screen::pet_status();
    char head[256];
    std::snprintf(head, sizeof head,
                  R"({"ok":true,"action":"%s","item":"%s","gain":%d,"first_today":%s,"too_full":%s,"welcome_back":%s,"level_up":%s,"new_gift":%s,)",
                  screen::action_name(x.action), x.action == screen::Action::Gift ? screen::gift_name(x.item) : x.action == screen::Action::Feed ? screen::treat_name(x.item) : "",
                  x.gain, x.first_today ? "true" : "false", x.too_full ? "true" : "false", x.welcome_back ? "true" : "false",
                  x.level_up ? "true" : "false", x.new_gift ? "true" : "false");
    std::string out = head;
    pet_json(out, res->bond, s.name, true);
    out += "}";
    send_json(r, "200 OK", out.c_str());
    return ESP_OK;
}

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

extern const char encoder_js_start[] asm("_binary_clip_encoder_js_start");
extern const char encoder_js_end[] asm("_binary_clip_encoder_js_end");

esp_err_t get_encoder(httpd_req_t* r) {
    httpd_resp_set_type(r, "application/javascript");
    httpd_resp_set_hdr(r, "Cache-Control", "no-cache");
    httpd_resp_send(r, encoder_js_start, encoder_js_end - encoder_js_start - 1);
    return ESP_OK;
}

esp_err_t get_root(httpd_req_t* r) {
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    httpd_resp_set_hdr(r, "Cache-Control", "no-cache");
    httpd_resp_send(r, index_html_start, index_html_end - index_html_start - 1);  // EMBED_TXTFILES appends a NUL
    return ESP_OK;
}

}  // namespace

std::expected<void, engine::Err> start() noexcept {
    g_open = net::open_access();
    g_token = net::token();
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.stack_size = 6144;
    c.max_uri_handlers = 20;
    c.recv_wait_timeout = 10;
    c.send_wait_timeout = 10;
    c.max_open_sockets = 4;
    c.lru_purge_enable = true;
    c.uri_match_fn = httpd_uri_match_wildcard;
    httpd_handle_t h = nullptr;
    if (httpd_start(&h, &c) != ESP_OK) return std::unexpected(engine::Err::Io);
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = get_root, .user_ctx = nullptr},
        {.uri = "/clip_encoder.js", .method = HTTP_GET, .handler = get_encoder, .user_ctx = nullptr},
        {.uri = "/pet", .method = HTTP_GET, .handler = get_pet_page, .user_ctx = nullptr},
        {.uri = "/api/pet", .method = HTTP_GET, .handler = get_pet, .user_ctx = nullptr},
        {.uri = "/api/pet", .method = HTTP_POST, .handler = post_pet, .user_ctx = nullptr},
        {.uri = "/api/status", .method = HTTP_GET, .handler = get_status, .user_ctx = nullptr},
        {.uri = "/api/sensor", .method = HTTP_GET, .handler = get_sensor, .user_ctx = nullptr},
        {.uri = "/api/ack", .method = HTTP_POST, .handler = post_ack, .user_ctx = nullptr},
        {.uri = "/api/weather", .method = HTTP_GET, .handler = get_weather, .user_ctx = nullptr},
        {.uri = "/api/weather", .method = HTTP_POST, .handler = post_weather, .user_ctx = nullptr},
        {.uri = "/api/weather/demo", .method = HTTP_POST, .handler = post_weather_demo, .user_ctx = nullptr},
        {.uri = "/api/page", .method = HTTP_POST, .handler = post_page, .user_ctx = nullptr},
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
