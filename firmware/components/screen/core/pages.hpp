// screen: the pages the display rotates through, and the rotation with fades. Pure C++.
#pragma once

#include <cstdint>
#include <optional>

#include "screen/core/font.hpp"
#include "sensor/core/airq.hpp"
#include "weather/core/forecast.hpp"

namespace screen {

enum class Page : std::uint8_t { Clock, Air, Weather };
inline constexpr int kPageCount = 3;
[[nodiscard]] const char* page_name(Page p) noexcept;              // "clock", "air", "weather"
[[nodiscard]] std::optional<Page> page_from_name(const char* s) noexcept;

// Everything a page may need. Filled by the render task once per frame.
struct Inputs {
    std::uint32_t t_ms = 0;  // monotonic, drives the animations
    bool time_valid = false;
    int hour = 0, minute = 0, second = 0;
    int weekday = 0, day = 1, month = 1;  // weekday 0 = Sunday, month 1..12
    bool sensor_ok = false;
    sensor::Reading reading{};
    sensor::Quality quality{};
    bool alert = false;      // poor air and not snoozed
    bool weather_ok = false;
    weather::Forecast weather{};
    char place[20] = "MUNICH";  // shown on the weather page (upper case)
};

void render_clock(const Canvas& cv, const Inputs& in) noexcept;
void render_weather(const Canvas& cv, const Inputs& in) noexcept;
void render_page(const Canvas& cv, Page p, const Inputs& in) noexcept;

// Which page to show and how bright (fade out, switch, fade in).
class Rotator {
public:
    struct Config {
        std::uint32_t dwell_ms[kPageCount] = {12000, 15000, 12000};  // clock, air, weather
        std::uint32_t fade_ms = 350;                                  // each half of a transition
    };
    struct Frame { Page page; int brightness; };  // brightness 0..256

    Rotator() noexcept : Rotator(Config{}) {}
    explicit Rotator(const Config& c) noexcept : cfg_(c) {}

    // `available`: bit (1 << page) set for pages that have something to show (the clock always does).
    // `pin`: page to stay on (e.g. air while the alert is active).
    Frame tick(std::uint32_t now, unsigned available, std::optional<Page> pin) noexcept;
    void next(std::uint32_t now) noexcept { request_ = Request::Next; (void)now; }       // e.g. button
    void show(Page p, std::uint32_t now) noexcept { request_ = Request::Show; wanted_ = p; (void)now; }
    Page current() const noexcept { return cur_; }

private:
    enum class Request : std::uint8_t { None, Next, Show };
    Page following(Page p, unsigned available) const noexcept;
    void start_switch(Page to, std::uint32_t now) noexcept;

    Config cfg_;
    Page cur_ = Page::Clock, to_ = Page::Clock, wanted_ = Page::Clock;
    std::uint32_t since_ = 0, fade_start_ = 0;
    bool started_ = false, fading_ = false;
    Request request_ = Request::None;
};

}  // namespace screen
