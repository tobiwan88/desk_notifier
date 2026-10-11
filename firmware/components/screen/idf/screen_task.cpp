#include "screen/idf/screen_task.hpp"

#include <atomic>
#include <ctime>

#include "clock/idf/clock.hpp"
#include "config.hpp"
#include "display/idf/display.hpp"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "screen/core/view.hpp"
#include "sensor/idf/bme688.hpp"

namespace screen {
namespace {

const char* const kTag = "screen";
constexpr std::uint32_t kFramePeriodMs = 1000;
constexpr std::uint32_t kSnoozeMs = 15u * 60u * 1000u;

std::atomic<bool> g_ack{false};
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
Stats g_stats{};

std::uint32_t now_ms() { return static_cast<std::uint32_t>(esp_timer_get_time() / 1000); }

// Active-low button with a simple 30 ms debounce; returns true once per press.
bool button_pressed(std::uint32_t now) {
    static bool last = false;
    static std::uint32_t since = 0;
    if (cfg::kButton < 0) return false;
    const bool down = gpio_get_level(static_cast<gpio_num_t>(cfg::kButton)) == 0;
    if (down != last && now - since > 30) {
        since = now;
        last = down;
        return down;
    }
    return false;
}

void task(void*) {
    esp_task_wdt_add(nullptr);
    auto* fb = static_cast<gfx::Rgb565be*>(heap_caps_malloc(sizeof(gfx::Rgb565be) * gfx::kScreen * gfx::kScreen, MALLOC_CAP_SPIRAM));
    if (!fb) fb = static_cast<gfx::Rgb565be*>(heap_caps_malloc(sizeof(gfx::Rgb565be) * gfx::kScreen * gfx::kScreen, MALLOC_CAP_DEFAULT));
    if (!fb) { ESP_LOGE(kTag, "no frame buffer"); vTaskDelete(nullptr); return; }
    const Canvas cv{{fb, static_cast<std::size_t>(gfx::kScreen) * gfx::kScreen}};
    if (cfg::kButton >= 0) {
        gpio_config_t io = {};
        io.pin_bit_mask = 1ULL << cfg::kButton;
        io.mode = GPIO_MODE_INPUT;
        io.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io);
    }

    std::uint32_t snooze_until = 0, next_frame = 0, sum_us = 0, max_us = 0, n = 0;
    int stalls = 0;
    for (;;) {
        const std::uint32_t now = now_ms();
        if (g_ack.exchange(false) || button_pressed(now)) {
            snooze_until = now + kSnoozeMs;
            next_frame = 0;  // redraw right away
            ESP_LOGI(kTag, "ack: alert snoozed for 15 min");
        }
        if (static_cast<std::int32_t>(now - next_frame) >= 0) {
            next_frame = now + kFramePeriodMs;
            const auto snap = sensor::latest();
            HomeData d;
            d.t_ms = now;
            std::tm tm{};
            if (clock_sync::local(tm)) { d.time_valid = true; d.hour = tm.tm_hour; d.minute = tm.tm_min; d.second = tm.tm_sec; }
            d.sensor_ok = snap.ok;
            d.reading = snap.reading;
            d.quality = snap.quality;
            const bool snoozed = static_cast<std::int32_t>(snooze_until - now) > 0;
            d.alert = snap.ok && snap.quality.level == sensor::Level::Poor && !snoozed;
            const std::int64_t t0 = esp_timer_get_time();
            render_home(cv, d);
            const bool ok = static_cast<bool>(display::blit(cv.px));
            const auto us = static_cast<std::uint32_t>(esp_timer_get_time() - t0);
            stalls = ok ? 0 : stalls + 1;
            if (stalls >= 3) { ESP_LOGE(kTag, "display stalled, rebooting"); esp_restart(); }
            sum_us += us; max_us = us > max_us ? us : max_us; ++n;
            taskENTER_CRITICAL(&g_mux);
            g_stats = Stats{sum_us / n, max_us, n, d.alert, snoozed};
            taskEXIT_CRITICAL(&g_mux);
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

}  // namespace

std::expected<void, engine::Err> start() noexcept {
    if (xTaskCreate(task, "screen", 5120, nullptr, 8, nullptr) != pdPASS) return std::unexpected(engine::Err::Io);
    return {};
}

void ack() noexcept { g_ack = true; }

Stats stats() noexcept {
    taskENTER_CRITICAL(&g_mux);
    const Stats s = g_stats;
    taskEXIT_CRITICAL(&g_mux);
    return s;
}

}  // namespace screen
