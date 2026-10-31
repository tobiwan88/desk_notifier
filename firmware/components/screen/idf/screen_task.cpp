#include "screen/idf/screen_task.hpp"

#include <atomic>
#include <mutex>
#include <cctype>
#include <cstdio>
#include <cstring>
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
#include "screen/core/pair_view.hpp"
#include "sensor/idf/bme688.hpp"
#include "settings/idf/settings.hpp"
#include "weather/idf/client.hpp"

namespace screen {
namespace {

const char* const kTag = "screen";
constexpr std::uint32_t kFramePeriodMs = 1000 / CONFIG_DESK_FPS;
constexpr std::uint32_t kSnoozeMs = 15u * 60u * 1000u;

std::atomic<bool> g_ack{false};
std::atomic<bool> g_bench{false};
std::atomic<int> g_show{-1};       // page requested by the API (-1 none, kPageCount = next)
std::atomic<int> g_page{0};
std::atomic<std::uint32_t> g_pair_code{0};   // Bluetooth pairing passkey on screen (0 = none)
std::atomic<std::uint32_t> g_pair_until{0};  // ms timestamp at which it disappears
constexpr std::uint32_t kPairShowMs = 60u * 1000u;

Rotator::Config rotation_config() {
    Rotator::Config c;
    c.dwell_ms[static_cast<int>(Page::Clock)] = CONFIG_DESK_PAGE_CLOCK_S * 1000u;
    c.dwell_ms[static_cast<int>(Page::Air)] = CONFIG_DESK_PAGE_AIR_S * 1000u;
    c.dwell_ms[static_cast<int>(Page::Day)] = CONFIG_DESK_PAGE_DAY_S * 1000u;
    return c;
}
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

// The friendship with the owner, shared between the web server (visits) and the render task (reactions).
std::mutex g_pet_mx;
Bond g_bond;
char g_name[12] = "";
Reaction g_react{};
std::uint32_t g_react_at = 0, g_react_id = 0;

struct Today { int day, y, m, d, hour; };
std::optional<Today> today_full() {
    std::tm tm{};
    if (!clock_sync::local(tm)) return std::nullopt;
    const int y = tm.tm_year + 1900, m = tm.tm_mon + 1, d = tm.tm_mday;
    return Today{cal::day_number(y, m, d), y, m, d, tm.tm_hour};
}
std::optional<int> today() {
    const auto t = today_full();
    if (!t) return std::nullopt;
    return t->day;
}

void save_bond(const Bond& b) {
    char buf[96];
    bond_save(b, buf, sizeof buf);
    settings::set("bond", buf);
}
Stats g_stats{};

std::uint32_t now_ms() { return static_cast<std::uint32_t>(esp_timer_get_time() / 1000); }

// Active-low button with a simple 30 ms debounce; returns true once per press.
bool button_pressed([[maybe_unused]] std::uint32_t now) {
    if constexpr (cfg::kButton < 0) {
        return false;  // no button on this board (DESK_BTN = -1)
    } else {
        static bool last = false;
        static std::uint32_t since = 0;
        const bool down = gpio_get_level(static_cast<gpio_num_t>(cfg::kButton)) == 0;
        if (down != last && now - since > 30) {
            since = now;
            last = down;
            return down;
        }
        return false;
    }
}

// Console `bench`: draws fixed scenes of each page 20 times and logs the time per frame (no panel traffic), for tuning the renderer.
void run_bench(const Canvas& cv) {
    Inputs in;
    in.time_valid = true; in.hour = 14; in.minute = 5; in.weekday = 2; in.day = 14; in.month = 10; in.year = 2026;
    in.sensor_ok = true; in.quality = {90, sensor::Level::Good}; in.reading.temp_c = 22; in.reading.rh = 45; in.reading.hpa = 960;
    in.weather_ok = true; in.weather.temp_c = 12; in.weather.is_day = true; in.weather.sky = weather::Sky::PartlyCloudy;
    in.weather.max_c = 15; in.weather.min_c = 8; in.weather.precip_pct = 10;
    constexpr int kRuns = 20;
    std::uint32_t us[kPageCount] = {};
    for (int p = 0; p < kPageCount; ++p) {
        const std::int64_t t0 = esp_timer_get_time();
        for (int i = 0; i < kRuns; ++i) {
            in.t_ms = 5000u + static_cast<std::uint32_t>(i) * 100u;
            render_page(cv, static_cast<Page>(p), in);
        }
        us[p] = static_cast<std::uint32_t>((esp_timer_get_time() - t0) / kRuns);
    }
    ESP_LOGW(kTag, "bench render per frame: clock %lu us, air %lu us, day %lu us", static_cast<unsigned long>(us[0]),
             static_cast<unsigned long>(us[1]), static_cast<unsigned long>(us[2]));
}

void task(void*) {
    esp_task_wdt_add(nullptr);
    auto* fb = static_cast<gfx::Rgb565be*>(heap_caps_malloc(sizeof(gfx::Rgb565be) * gfx::kScreen * gfx::kScreen, MALLOC_CAP_SPIRAM));
    if (!fb) fb = static_cast<gfx::Rgb565be*>(heap_caps_malloc(sizeof(gfx::Rgb565be) * gfx::kScreen * gfx::kScreen, MALLOC_CAP_DEFAULT));
    if (!fb) { ESP_LOGE(kTag, "no frame buffer"); vTaskDelete(nullptr); return; }
    const Canvas cv{{fb, static_cast<std::size_t>(gfx::kScreen) * gfx::kScreen}};
    if constexpr (cfg::kButton >= 0) {
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
    {
        const std::lock_guard<std::mutex> lock(g_pet_mx);
        g_bond = bond_load(settings::get("bond").value_or("").c_str());
        const std::string name = settings::get("sqname").value_or("");
        std::snprintf(g_name, sizeof g_name, "%s", name.c_str());
    }
    std::uint32_t seen_react = 0;
    int watered_day = 0;
    const std::uint32_t seed = esp_random();  // a different string of little surprises after every boot
    RainWatch rain;
    Plant plant = plant_load(settings::get("plant").value_or("").c_str());
    int golden_count = std::atoi(settings::get("golden").value_or("0").c_str());
    bool golden_seen = false, coin_seen = false;
    std::uint32_t plant_tick = 0;
    int plant_saves = 0;
    std::uint32_t snooze_until = 0, next_frame = 0, sum_us = 0, max_us = 0, n = 0, render_ema_us = 0, blit_ema_us = 0;  // the split follows the page on screen (about 16 frames)
    int stalls = 0;
    for (;;) {
        if (g_bench.exchange(false)) run_bench(cv);
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
                if (closed && plant.grew) {  // a good-air day: a couple of coins (from five hearts)
                    const std::lock_guard<std::mutex> lock(g_pet_mx);
                    if (level(g_bond) >= kLevels - 1) { earn(g_bond, kCoinsLeaf); save_bond(g_bond); }
                }
                if (closed || ++plant_saves >= 30) {  // save at the end of a day, and every half hour
                    plant_saves = 0;
                    char buf[64];
                    plant_save(plant, buf, sizeof buf);
                    settings::set("plant", buf);
                }
            }
            {  // the friendship and a reaction to a visit
                const std::lock_guard<std::mutex> lock(g_pet_mx);
                in.bond = g_bond;
                std::memcpy(in.pet_name, g_name, sizeof in.pet_name);
                const std::uint32_t age = now - g_react_at;
                if (g_react_id != 0 && age < reaction_ms(g_react.action)) {
                    in.react = {true, g_react, static_cast<float>(age) / static_cast<float>(reaction_ms(g_react.action))};
                }
                if (g_react_id != seen_react) {
                    seen_react = g_react_id;
                    // watering perks up the window-sill plant, once a day
                    const int day = in.year * 10000 + in.month * 100 + in.day;
                    if (g_react.action == Action::Water && in.time_valid && watered_day != day && plant.droop > 0) {
                        watered_day = day;
                        --plant.droop;
                        char buf[64];
                        plant_save(plant, buf, sizeof buf);
                        settings::set("plant", buf);
                    }
                }
            }
            in.plant = plant;
            const bool golden = fun::golden(now, seed, level(in.bond) >= 3) >= 0;  // the golden acorn: counted once, shown on the air page
            if (golden && !golden_seen) {
                ++golden_count;
                {
                    const std::lock_guard<std::mutex> lock(g_pet_mx);
                    earn(g_bond, level(g_bond) >= kLevels - 1 ? kCoinsGolden : kCoinsGoldenEarly);
                    save_bond(g_bond);
                }
                settings::set("golden", std::to_string(golden_count));
                ESP_LOGI(kTag, "golden acorn #%d", golden_count);
            }
            golden_seen = golden;
            const bool coin = level(in.bond) >= kLevels - 1 && fun::coin_found(now, seed) >= 0;  // found in the grass
            if (coin && !coin_seen) {
                const std::lock_guard<std::mutex> lock(g_pet_mx);
                earn(g_bond, kCoinsFound);
                save_bond(g_bond);
            }
            coin_seen = coin;
            in.golden_count = golden_count;
            in.schedule = schedule;
            in.seed = seed;

            unsigned avail = 0;
            if (CONFIG_DESK_PAGE_CLOCK_S > 0) avail |= 1u << static_cast<int>(Page::Clock);
            if (CONFIG_DESK_PAGE_AIR_S > 0) avail |= 1u << static_cast<int>(Page::Air);
            if (CONFIG_DESK_PAGE_DAY_S > 0 && in.time_valid) avail |= 1u << static_cast<int>(Page::Day);
            // bad air: the squirrel stays on screen until the alert is acknowledged
            const bool pin_air = alert || in.react.active || (golden && CONFIG_DESK_PAGE_AIR_S > 0);
            const auto frame = rot.tick(now, avail, pin_air ? std::optional<Page>(Page::Air) : std::nullopt);
            g_page = static_cast<int>(frame.page);

            const std::int64_t t0 = esp_timer_get_time();
            render_page(cv, frame.page, in);
            draw::dim(cv, frame.brightness);
            if (const std::uint32_t code = g_pair_code.load(); code != 0 && static_cast<std::int32_t>(g_pair_until.load() - now) > 0) {
                char digits[12];
                std::snprintf(digits, sizeof digits, "%06lu", static_cast<unsigned long>(code % 1000000u));
                render_pair_code(cv, digits);
            }
            const std::int64_t t1 = esp_timer_get_time();
            const bool ok = static_cast<bool>(display::blit(cv.px));
            const std::int64_t t2 = esp_timer_get_time();
            const auto us = static_cast<std::uint32_t>(t2 - t0);
            stalls = ok ? 0 : stalls + 1;
            if (stalls >= 3) { ESP_LOGE(kTag, "display stalled, rebooting"); esp_restart(); }
            if (n >= 1000) { sum_us = 0; max_us = 0; n = 0; }  // keep the average recent
            sum_us += us; max_us = us > max_us ? us : max_us; ++n;
            render_ema_us = render_ema_us == 0 ? static_cast<std::uint32_t>(t1 - t0) : render_ema_us - render_ema_us / 16 + static_cast<std::uint32_t>(t1 - t0) / 16;
            blit_ema_us = blit_ema_us == 0 ? static_cast<std::uint32_t>(t2 - t1) : blit_ema_us - blit_ema_us / 16 + static_cast<std::uint32_t>(t2 - t1) / 16;
            taskENTER_CRITICAL(&g_mux);
            g_stats = Stats{sum_us / n, max_us, g_stats.frames + 1, render_ema_us, blit_ema_us, alert, snoozed};
            taskEXIT_CRITICAL(&g_mux);
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

}  // namespace

void bench() noexcept { g_bench = true; }

void show_pair_code(std::uint32_t code) noexcept {
    g_pair_until = now_ms() + kPairShowMs;
    g_pair_code = code;
}

std::expected<void, engine::Err> start() noexcept {
    if (xTaskCreate(task, "screen", 6144, nullptr, 8, nullptr) != pdPASS) return std::unexpected(engine::Err::Io);
    return {};
}

void ack() noexcept { g_ack = true; }
void show_page(Page p) noexcept { g_show = static_cast<int>(p); }
void next_page() noexcept { g_show = kPageCount; }

std::optional<PetResult> pet(Action a, int item) noexcept {
    const auto now = today_full();
    if (!now) return std::nullopt;
    const auto day = std::optional<int>(now->day);
    Schedule sched;
    sched.birthday = CONFIG_DESK_DAY_BIRTHDAY; sched.anniversary = CONFIG_DESK_DAY_ANNIVERSARY;
    const bool special = occasion_for(now->y, now->m, now->d, sched) != Occasion::None;
    PetResult res;
    {
        const std::lock_guard<std::mutex> lock(g_pet_mx);
        res.reaction = interact(g_bond, *day, a, item, special);
        res.bond = g_bond;
        g_react = res.reaction;
        g_react_at = now_ms();
        ++g_react_id;
    }
    save_bond(res.bond);
    ESP_LOGI(kTag, "pet: %s, +%d, level %d", action_name(a), res.reaction.gain, res.reaction.level);
    return res;
}

PetStatus pet_status() noexcept {
    PetStatus s;
    const auto day = today();
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(g_pet_mx);
        if (day) {
            const int before = g_bond.score;
            settle(g_bond, *day);  // the gentle fading of days away (never below the level reached)
            changed = g_bond.score != before;
        }
        s.bond = g_bond;
        std::memcpy(s.name, g_name, sizeof s.name);
    }
    if (changed) save_bond(s.bond);
    if (day && s.bond.today != *day) { s.bond.done = 0; s.bond.treats = 0; s.bond.gained = 0; }  // a new day, nothing done yet
    s.time_valid = day.has_value();
    return s;
}

ShopStatus shop_status() noexcept {
    ShopStatus s;
    const auto now = today_full();
    const std::lock_guard<std::mutex> lock(g_pet_mx);
    s.bond = g_bond;
    if (now) {
        s.time_valid = true;
        s.offer_count = daily_offer(g_bond, now->day, now->y, now->m, now->d, s.offer);
        s.look = look_for(g_bond, now->day, now->hour >= 14 ? 1 : 0, now->y, now->m, now->d);
    }
    return s;
}

std::optional<BuyResult> shop_buy(int item) noexcept {
    const auto now = today_full();
    if (!now) return std::nullopt;
    BuyResult r;
    Bond copy;
    {
        const std::lock_guard<std::mutex> lock(g_pet_mx);
        r = buy(g_bond, item, now->day, now->y, now->m, now->d);
        copy = g_bond;
        if (r.error == BuyError::None) {  // unwrap it on screen
            g_react = Reaction{};
            g_react.unbox = true;
            g_react.item = item;
            g_react.action = Action::Gift;
            g_react.level = level(g_bond);
            g_react_at = now_ms();
            ++g_react_id;
        }
    }
    if (r.error == BuyError::None) {
        save_bond(copy);
        ESP_LOGI(kTag, "shop: bought %s, %d coins left", shop_item(item).id, copy.coins);
    }
    return r;
}

void set_pet_name(std::string_view name) noexcept {
    char up[12] = "";
    std::size_t n = 0;
    for (const char c : name) {  // letters, digits and spaces the screen font can show
        if (n + 1 >= sizeof up) break;
        const char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if ((u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == ' ' || u == '-') up[n++] = u;
    }
    up[n] = 0;
    {
        const std::lock_guard<std::mutex> lock(g_pet_mx);
        std::memcpy(g_name, up, sizeof g_name);
    }
    settings::set("sqname", up);
}
Page page() noexcept { return static_cast<Page>(g_page.load()); }

Stats stats() noexcept {
    taskENTER_CRITICAL(&g_mux);
    const Stats s = g_stats;
    taskEXIT_CRITICAL(&g_mux);
    return s;
}

}  // namespace screen
