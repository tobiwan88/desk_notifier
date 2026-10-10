#include "engine/idf/render_task.hpp"

#include <algorithm>

#include "clip/idf/player.hpp"
#include "config.hpp"
#include "display/idf/display.hpp"
#include "engine/core/scene.hpp"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "settings/idf/settings.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace engine {
namespace {

const char* const kTag = "render";
QueueHandle_t g_queue = nullptr;
Scene<clip::ClipPlayer>* g_scene = nullptr;
portMUX_TYPE g_stats_mux = portMUX_INITIALIZER_UNLOCKED;
Stats g_stats{};
std::uint64_t g_fill_us = 0;  // render task only

std::uint32_t now_ms() { return static_cast<std::uint32_t>(esp_timer_get_time() / 1000); }

// Renders one display's plan; returns the first error.
std::expected<void, Err> draw(Scene<clip::ClipPlayer>& scene, Eye eye, const FramePlan& plan) {
    auto rects = plan.dirty;
    if (plan.full_redraw) { rects.clear(); (void)rects.push_back(gfx::Rect{0, 0, gfx::kScreen, gfx::kScreen}); }
    for (const gfx::Rect& r : rects) {
        for (const gfx::Rect s : gfx::Strips(r, static_cast<int>(cfg::kStripLines), plan.trim)) {
            auto buf = display::acquire(100);
            if (!buf) return std::unexpected(buf.error());
            const std::int64_t f0 = esp_timer_get_time();
            auto f = scene.fill(eye, s, buf->px.first(static_cast<std::size_t>(s.area())));
            g_fill_us += static_cast<std::uint64_t>(esp_timer_get_time() - f0);
            if (!f) {
                // buffer was acquired but never pushed: push black so the pool stays consistent
                std::fill(buf->px.begin(), buf->px.begin() + s.area(), gfx::Rgb565be{0});
            }
            if (auto p = display::push(eye, std::move(*buf), s); !p) return p;
        }
    }
    return {};
}

void task(void*) {
    esp_task_wdt_add(nullptr);
    Scene<clip::ClipPlayer> scene(esp_timer_get_time() & 0xFFFFFFFF, settings::load_style().value_or(eyes::kRealistic));
    g_scene = &scene;
    scene.clip(Eye::Left).set_slot(0);
    scene.clip(Eye::Right).set_slot(1);
    std::uint32_t frame_n = 0;
    std::uint64_t sum_us = 0;
    std::uint32_t max_us = 0, window_start = now_ms();
    int stalls = 0;
    TickType_t last_wake = xTaskGetTickCount();
    std::uint32_t delay_ms = 33;

    for (;;) {
        const std::int64_t t0 = esp_timer_get_time();
        const std::uint32_t now = now_ms();
        Command cmd;
        while (xQueueReceive(g_queue, &cmd, 0) == pdTRUE) scene.apply(cmd, now);
        scene.tick(now);
        if (scene.style_save_pending()) { (void)settings::save_style(scene.style()); scene.style_saved(); }

        std::uint32_t next_delay = 100;
        bool failed = false;
        for (std::size_t i = 0; i < kEyes; ++i) {
            const Eye eye = static_cast<Eye>(i);
            const FramePlan plan = scene.next(eye, now);
            next_delay = std::min(next_delay, plan.delay_ms);
            if (auto r = draw(scene, eye, plan); !r) { failed = true; break; }
            if (auto w = display::wait_idle(100); !w) { failed = true; break; }  // keep FIFO across displays
        }
        stalls = failed ? stalls + 1 : 0;
        if (stalls >= 3) {
            ESP_LOGE(kTag, "display stalled, rebooting");
            esp_restart();
        }
        esp_task_wdt_reset();

        const auto us = static_cast<std::uint32_t>(esp_timer_get_time() - t0);
        sum_us += us; max_us = std::max(max_us, us); ++frame_n;
        const std::uint32_t nowm = now_ms();
        if (nowm - window_start >= 1000) {
            taskENTER_CRITICAL(&g_stats_mux);
            g_stats.fps_x10 = static_cast<std::uint16_t>(frame_n * 10000 / (nowm - window_start));
            g_stats.frame_us_avg = static_cast<std::uint32_t>(sum_us / frame_n);
            g_stats.frame_us_max = max_us;
            g_stats.fill_us_avg = static_cast<std::uint32_t>(g_fill_us / frame_n);
            g_fill_us = 0;
            taskEXIT_CRITICAL(&g_stats_mux);
            frame_n = 0; sum_us = 0; max_us = 0; window_start = nowm;
        }
        g_stats.source = {scene.current(Eye::Left), scene.current(Eye::Right)};
        delay_ms = next_delay;
        const TickType_t before = last_wake;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay_ms));
        (void)before;
        if (xTaskGetTickCount() - last_wake > pdMS_TO_TICKS(200)) last_wake = xTaskGetTickCount();  // overrun: reset reference
    }
}

}  // namespace

std::expected<void, Err> start_render_task() noexcept {
    g_queue = xQueueCreate(8, sizeof(Command));
    if (!g_queue) return std::unexpected(Err::Io);
    if (xTaskCreate(task, "render", 6144, nullptr, 8, nullptr) != pdPASS) return std::unexpected(Err::Io);
    return {};
}

std::expected<void, Err> post(const Command& c, std::uint32_t timeout_ms) noexcept {
    if (!g_queue) return std::unexpected(Err::Busy);
    return xQueueSend(g_queue, &c, pdMS_TO_TICKS(timeout_ms)) == pdTRUE ? std::expected<void, Err>{} : std::unexpected(Err::Busy);
}

Stats stats() noexcept {
    taskENTER_CRITICAL(&g_stats_mux);
    Stats s = g_stats;
    taskEXIT_CRITICAL(&g_stats_mux);
    return s;
}

}  // namespace engine
