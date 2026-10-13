#include "screen/idf/screen_task.hpp"

#include <atomic>
#include <cctype>
#include <cstdlib>
#include <string>
#include <ctime>

#include <sys/time.h>

#include "clock/idf/clock.hpp"
#include "config.hpp"
#include "display/idf/display.hpp"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "screen/core/draw.hpp"
#include "screen/core/pages.hpp"
#include "screen/core/fun.hpp"
#include "screen/core/memory.hpp"
#include "sensor/idf/bme688.hpp"
#include "settings/idf/settings.hpp"
#include "weather/idf/client.hpp"

namespace screen {
namespace {

const char* const kTag = "screen";
constexpr std::uint32_t kFramePeriodMs = 1000 / CONFIG_DESK_FPS;
constexpr std::uint32_t kSnoozeMs = 15u * 60u * 1000u;

std::atomic<bool> g_ack{false};
std::atomic<int> g_show{-1};       // page requested by the API (-1 none, kPageCount = next)
std::atomic<int> g_page{0};

Rotator::Config rotation_config() {
    Rotator::Config c;
    c.dwell_ms[static_cast<int>(Page::Clock)] = CONFIG_DESK_PAGE_CLOCK_S * 1000u;
    c.dwell_ms[static_cast<int>(Page::Air)] = CONFIG_DESK_PAGE_AIR_S * 1000u;
    c.dwell_ms[static_cast<int>(Page::Day)] = CONFIG_DESK_PAGE_DAY_S * 1000u;
    return c;
}
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

    Rotator rot(rotation_config());
    Schedule schedule;
    schedule.wake = CONFIG_DESK_DAY_WAKE; schedule.work = CONFIG_DESK_DAY_WORK; schedule.lunch = CONFIG_DESK_DAY_LUNCH;
    schedule.end = CONFIG_DESK_DAY_END; schedule.bed = CONFIG_DESK_DAY_BED;
    schedule.birthday = CONFIG_DESK_DAY_BIRTHDAY; schedule.anniversary = CONFIG_DESK_DAY_ANNIVERSARY;
    {
        const char* name = CONFIG_DESK_DAY_BIRTHDAY_NAME;
        std::size_t i = 0;
        for (; name[i] && i + 1 < sizeof schedule.birthday_name; ++i)
            schedule.birthday_name[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[i])));
        schedule.birthday_name[i] = 0;
    }
    AirTrend trend;
    const std::uint32_t seed = esp_random();  // a different string of little surprises after every boot
    RainWatch rain;
    Plant plant = plant_load(settings::get("plant").value_or("").c_str());
    int golden_count = std::atoi(settings::get("golden").value_or("0").c_str());
    bool golden_seen = false;
    std::uint32_t plant_tick = 0;
    int plant_saves = 0;
    std::uint32_t snooze_until = 0, next_frame = 0, sum_us = 0, max_us = 0, n = 0;
    int stalls = 0;
    for (;;) {
        const std::uint32_t now = now_ms();
        const auto snap = sensor::latest();
        const bool snoozed = static_cast<std::int32_t>(snooze_until - now) > 0;
        const bool alert = snap.ok && snap.quality.level == sensor::Level::Poor && !snoozed;
        const bool pressed = button_pressed(now);
        if (g_ack.exchange(false) || (pressed && alert)) {
            snooze_until = now + kSnoozeMs;
            next_frame = 0;  // redraw right away
            ESP_LOGI(kTag, "ack: alert snoozed for 15 min");
        } else if (pressed) {
            rot.next(now);  // without an alert the button flips to the next page
        }
        if (const int req = g_show.exchange(-1); req >= 0) {
            if (req >= kPageCount) rot.next(now);
            else rot.show(static_cast<Page>(req), now);
        }
        if (static_cast<std::int32_t>(now - next_frame) >= 0) {
            next_frame = now + kFramePeriodMs;
            Inputs in;
            in.t_ms = now;
            std::tm tm{};
            if (clock_sync::local(tm)) {
                in.time_valid = true;
                in.hour = tm.tm_hour; in.minute = tm.tm_min; in.second = tm.tm_sec;
                in.weekday = tm.tm_wday; in.day = tm.tm_mday; in.month = tm.tm_mon + 1; in.year = tm.tm_year + 1900;
                timeval tv{};
                gettimeofday(&tv, nullptr);
                in.millis = static_cast<int>(tv.tv_usec / 1000);
            }
            in.sensor_ok = snap.ok;
            in.reading = snap.reading;
            in.quality = snap.quality;
            trend.add(now, snap.quality.score, snap.ok && snap.quality.level != sensor::Level::WarmingUp);
            in.air_drop = trend.sudden_drop();
            in.alert = alert;
            const auto wx = weather::latest();
            in.weather_ok = wx.ok;
            in.weather = wx.forecast;
            rain.update(now, wx.ok, weather::wet(weather::now_sky(wx.forecast)));
            in.since_rain_min = rain.since_min(now);
            if (in.time_valid && now - plant_tick >= 60000u) {  // the window-sill plant counts the minutes of good air
                plant_tick = now;
                const bool closed = plant_minute(plant, in.year * 10000 + in.month * 100 + in.day, snap.ok, snap.quality.level);
                if (closed || ++plant_saves >= 30) {  // save at the end of a day, and every half hour
                    plant_saves = 0;
                    char buf[64];
                    plant_save(plant, buf, sizeof buf);
                    settings::set("plant", buf);
                }
            }
            in.plant = plant;
            const bool golden = fun::golden(now, seed) >= 0;  // the golden acorn: counted once, shown on the air page
            if (golden && !golden_seen) {
                ++golden_count;
                settings::set("golden", std::to_string(golden_count));
                ESP_LOGI(kTag, "golden acorn #%d", golden_count);
            }
            golden_seen = golden;
            in.golden_count = golden_count;
            in.schedule = schedule;
            in.seed = seed;

            unsigned avail = 0;
            if (CONFIG_DESK_PAGE_CLOCK_S > 0) avail |= 1u << static_cast<int>(Page::Clock);
            if (CONFIG_DESK_PAGE_AIR_S > 0) avail |= 1u << static_cast<int>(Page::Air);
            if (CONFIG_DESK_PAGE_DAY_S > 0 && in.time_valid) avail |= 1u << static_cast<int>(Page::Day);
            // bad air: the squirrel stays on screen until the alert is acknowledged
            const bool pin_air = alert || (golden && CONFIG_DESK_PAGE_AIR_S > 0);
            const auto frame = rot.tick(now, avail, pin_air ? std::optional<Page>(Page::Air) : std::nullopt);
            g_page = static_cast<int>(frame.page);

            const std::int64_t t0 = esp_timer_get_time();
            render_page(cv, frame.page, in);
            draw::dim(cv, frame.brightness);
            const bool ok = static_cast<bool>(display::blit(cv.px));
            const auto us = static_cast<std::uint32_t>(esp_timer_get_time() - t0);
            stalls = ok ? 0 : stalls + 1;
            if (stalls >= 3) { ESP_LOGE(kTag, "display stalled, rebooting"); esp_restart(); }
            if (n >= 1000) { sum_us = 0; max_us = 0; n = 0; }  // keep the average recent
            sum_us += us; max_us = us > max_us ? us : max_us; ++n;
            taskENTER_CRITICAL(&g_mux);
            g_stats = Stats{sum_us / n, max_us, g_stats.frames + 1, alert, snoozed};
            taskEXIT_CRITICAL(&g_mux);
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

}  // namespace

std::expected<void, engine::Err> start() noexcept {
    if (xTaskCreate(task, "screen", 6144, nullptr, 8, nullptr) != pdPASS) return std::unexpected(engine::Err::Io);
    return {};
}

void ack() noexcept { g_ack = true; }
void show_page(Page p) noexcept { g_show = static_cast<int>(p); }
void next_page() noexcept { g_show = kPageCount; }
Page page() noexcept { return static_cast<Page>(g_page.load()); }

Stats stats() noexcept {
    taskENTER_CRITICAL(&g_mux);
    const Stats s = g_stats;
    taskEXIT_CRITICAL(&g_mux);
    return s;
}

}  // namespace screen
