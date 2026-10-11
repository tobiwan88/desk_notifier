#include "weather/idf/client.hpp"

#include <algorithm>
#include <cctype>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "net/idf/net.hpp"
#include "sdkconfig.h"
#include "settings/idf/settings.hpp"

namespace weather {
namespace {

const char* const kTag = "weather";
constexpr std::size_t kMaxBody = 4096;
constexpr std::uint32_t kStaleS = 3 * 3600;

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
Forecast g_forecast{};
bool g_have = false;
Forecast g_demo{};
std::int64_t g_demo_until_us = 0;  // 0 = no demo
std::int64_t g_fetched_us = 0;
int g_errors = 0, g_http = 0;
TaskHandle_t g_task = nullptr;

std::string url() {
    const std::string lat = settings::get("wx_lat").value_or(CONFIG_DESK_WEATHER_LAT);
    const std::string lon = settings::get("wx_lon").value_or(CONFIG_DESK_WEATHER_LON);
    return std::string(CONFIG_DESK_WEATHER_URL) + "?latitude=" + lat + "&longitude=" + lon +
           "&current=temperature_2m,is_day,weather_code,wind_speed_10m"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
           "&timezone=auto&forecast_days=1";
}

// One GET. Returns the HTTP status (or -1) and fills `body`.
int fetch(std::string& body) {
    const std::string u = url();
    esp_http_client_config_t c = {};
    c.url = u.c_str();
    c.timeout_ms = 10000;
    c.buffer_size = 1024;
    c.crt_bundle_attach = esp_crt_bundle_attach;  // used only if the URL (or a redirect) is https
    esp_http_client_handle_t h = esp_http_client_init(&c);
    if (!h) return -1;
    int status = -1;
    if (esp_http_client_open(h, 0) == ESP_OK) {
        esp_http_client_fetch_headers(h);
        status = esp_http_client_get_status_code(h);
        char buf[256];
        for (;;) {
            const int n = esp_http_client_read(h, buf, sizeof buf);
            if (n <= 0) break;
            body.append(buf, static_cast<std::size_t>(n));
            if (body.size() > kMaxBody) { status = -1; break; }
        }
    }
    esp_http_client_close(h);
    esp_http_client_cleanup(h);
    return status;
}

void task(void*) {
    std::uint32_t wait_ms = 0;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));  // timeout, or refresh()
        if (!net::connected()) { wait_ms = 5000; continue; }
        std::string body;
        const int status = fetch(body);
        const auto f = status == 200 ? parse_open_meteo(body) : std::nullopt;
        taskENTER_CRITICAL(&g_mux);
        g_http = status;
        if (f) { g_forecast = *f; g_have = true; g_fetched_us = esp_timer_get_time(); g_errors = 0; }
        else ++g_errors;
        const int errors = g_errors;
        taskEXIT_CRITICAL(&g_mux);
        if (f) {
            ESP_LOGI(kTag, "%.1f C, code %d, %.0f/%.0f C, rain %d%%", static_cast<double>(f->temp_c), f->code,
                     static_cast<double>(f->max_c), static_cast<double>(f->min_c), f->precip_pct);
            wait_ms = static_cast<std::uint32_t>(CONFIG_DESK_WEATHER_REFRESH_MIN) * 60000u;
        } else {
            ESP_LOGW(kTag, "fetch failed (http %d, %u bytes)", status, static_cast<unsigned>(body.size()));
            wait_ms = std::min<std::uint32_t>(60000u << std::min(errors - 1, 3), 10u * 60000u);  // 1, 2, 4, 8, 10 min
        }
    }
}

}  // namespace

std::expected<void, engine::Err> start() noexcept {
    // The stack lives in PSRAM: internal RAM is scarce and this task only runs a few seconds per refresh.
    if (xTaskCreateWithCaps(task, "weather", 6144, nullptr, 3, &g_task, MALLOC_CAP_SPIRAM) == pdPASS) return {};
    if (xTaskCreate(task, "weather", 6144, nullptr, 3, &g_task) == pdPASS) return {};
    return std::unexpected(engine::Err::Io);
}

Status latest() noexcept {
    Status s;
    taskENTER_CRITICAL(&g_mux);
    s.forecast = g_forecast;
    s.errors = g_errors;
    s.http_status = g_http;
    const bool have = g_have;
    const std::int64_t at = g_fetched_us;
    const Forecast demo = g_demo;
    const std::int64_t demo_until = g_demo_until_us;
    taskEXIT_CRITICAL(&g_mux);
    if (demo_until != 0 && esp_timer_get_time() < demo_until) {
        s.forecast = demo;
        s.ok = true;
        return s;
    }
    if (have) {
        s.age_s = static_cast<std::uint32_t>((esp_timer_get_time() - at) / 1000000);
        s.ok = s.age_s < kStaleS;
    }
    return s;
}

void set_demo(const Forecast& f, std::uint32_t seconds) noexcept {
    taskENTER_CRITICAL(&g_mux);
    g_demo = f;
    g_demo_until_us = seconds ? esp_timer_get_time() + static_cast<std::int64_t>(seconds) * 1000000 : 0;
    taskEXIT_CRITICAL(&g_mux);
}

void refresh() noexcept {
    if (g_task) xTaskNotifyGive(g_task);
}

std::string place() noexcept {
    std::string p = settings::get("wx_place").value_or(CONFIG_DESK_WEATHER_PLACE);
    for (char& c : p) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));  // the font is upper case
    return p;
}

}  // namespace weather
