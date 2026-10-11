#include <vector>

#include "doctest.h"
#include "screen/core/draw.hpp"
#include "screen/core/pages.hpp"
#include "screen/core/squirrel.hpp"
#include "weather/core/forecast.hpp"

using screen::Page;
using screen::Rotator;
using weather::Sky;

namespace {

constexpr const char* kOpenMeteo = R"({"latitude":48.14,"longitude":11.58,"generationtime_ms":0.05,"utc_offset_seconds":7200,
"timezone":"Europe/Berlin","current_units":{"time":"iso8601","interval":"seconds","temperature_2m":"°C","is_day":"","weather_code":"wmo code"},
"current":{"time":"2026-10-11T17:30","interval":900,"temperature_2m":12.3,"is_day":1,"weather_code":3},
"daily_units":{"time":"iso8601","weather_code":"wmo code","temperature_2m_max":"°C","temperature_2m_min":"°C","precipitation_probability_max":"%"},
"daily":{"time":["2026-10-11"],"weather_code":[61],"temperature_2m_max":[14.8],"temperature_2m_min":[-1.5],"precipitation_probability_max":[45]}})";

int count_color(const std::vector<gfx::Rgb565be>& fb, gfx::Rgb565be c) {
    int n = 0;
    for (auto p : fb) n += p == c;
    return n;
}

}  // namespace

TEST_CASE("open-meteo parse") {
    const auto f = weather::parse_open_meteo(kOpenMeteo);
    if (!f) { FAIL_CHECK("no forecast"); return; }
    CHECK(f->temp_c == doctest::Approx(12.3));
    CHECK(f->is_day);
    CHECK(f->code == 61);  // the daily code wins over the current one
    CHECK(f->sky == Sky::Rain);
    CHECK(f->max_c == doctest::Approx(14.8));
    CHECK(f->min_c == doctest::Approx(-1.5));
    CHECK(f->precip_pct == 45);
}

TEST_CASE("open-meteo parse: missing parts and nulls") {
    CHECK_FALSE(weather::parse_open_meteo("").has_value());
    CHECK_FALSE(weather::parse_open_meteo(R"({"error":true,"reason":"bad"})").has_value());
    CHECK_FALSE(weather::parse_open_meteo(R"({"current_units":{"temperature_2m":"C"},"daily":{"weather_code":[1]}})").has_value());
    const auto f = weather::parse_open_meteo(
        R"({"current":{"temperature_2m":-3,"is_day":0,"weather_code":71},"daily":{"weather_code":[null],"precipitation_probability_max":[null]}})");
    if (!f) { FAIL_CHECK("no forecast"); return; }
    CHECK(f->code == 71);  // falls back to the current code
    CHECK(f->sky == Sky::Snow);
    CHECK_FALSE(f->is_day);
    CHECK(f->precip_pct == -1);
    CHECK(f->max_c == doctest::Approx(-3));
}

TEST_CASE("wmo codes map to pictures") {
    CHECK(weather::sky_from_wmo(0) == Sky::Clear);
    CHECK(weather::sky_from_wmo(1) == Sky::Clear);
    CHECK(weather::sky_from_wmo(2) == Sky::PartlyCloudy);
    CHECK(weather::sky_from_wmo(3) == Sky::Cloudy);
    CHECK(weather::sky_from_wmo(45) == Sky::Fog);
    CHECK(weather::sky_from_wmo(53) == Sky::Drizzle);
    CHECK(weather::sky_from_wmo(65) == Sky::Rain);
    CHECK(weather::sky_from_wmo(81) == Sky::Rain);
    CHECK(weather::sky_from_wmo(75) == Sky::Snow);
    CHECK(weather::sky_from_wmo(86) == Sky::Snow);
    CHECK(weather::sky_from_wmo(95) == Sky::Thunder);
    CHECK(std::string_view(weather::sky_name(Sky::Clear, false)) == "CLEAR");
}

TEST_CASE("air levels map to squirrel moods") {
    using sensor::Level;
    using screen::Mood;
    CHECK(screen::mood_for(Level::Good) == Mood::VeryGood);
    CHECK(screen::mood_for(Level::Fair) == Mood::Good);
    CHECK(screen::mood_for(Level::Stale) == Mood::Okay);
    CHECK(screen::mood_for(Level::Poor) == Mood::Bad);
    CHECK(screen::mood_for(Level::WarmingUp) == Mood::Sleepy);
    CHECK(std::string_view(screen::mood_label(Mood::VeryGood)) == "VERY GOOD");
}

TEST_CASE("page names round-trip") {
    for (int i = 0; i < screen::kPageCount; ++i) {
        const auto p = static_cast<Page>(i);
        CHECK(screen::page_from_name(screen::page_name(p)) == p);
    }
    CHECK_FALSE(screen::page_from_name("nope").has_value());
}

TEST_CASE("rotator cycles with fades and skips missing pages") {
    Rotator::Config c;
    c.dwell_ms[0] = 1000; c.dwell_ms[1] = 1000; c.dwell_ms[2] = 1000;
    c.fade_ms = 100;
    Rotator r(c);
    const unsigned all = 0b111, no_weather = 0b011;
    CHECK(r.tick(0, all, {}).page == Page::Clock);
    CHECK(r.tick(999, all, {}).brightness == 256);
    auto f = r.tick(1000, all, {});        // fade out starts
    CHECK(f.page == Page::Clock);
    f = r.tick(1050, all, {});
    CHECK(f.page == Page::Clock);
    CHECK(f.brightness == 128);
    f = r.tick(1150, all, {});              // fading in
    CHECK(f.page == Page::Air);
    CHECK(f.brightness == 128);
    CHECK(r.tick(1200, all, {}).brightness == 256);
    r.tick(2200, no_weather, {});           // weather unavailable: back to clock
    CHECK(r.tick(2400, no_weather, {}).page == Page::Clock);
}

TEST_CASE("rotator pin and requests") {
    Rotator::Config c;
    c.dwell_ms[0] = 1000; c.dwell_ms[1] = 1000; c.dwell_ms[2] = 1000;
    c.fade_ms = 10;
    Rotator r(c);
    r.tick(0, 0b111, {});
    r.tick(5, 0b111, Page::Air);           // alert pins the air page
    CHECK(r.tick(30, 0b111, Page::Air).page == Page::Air);
    CHECK(r.tick(5000, 0b111, Page::Air).page == Page::Air);  // stays while pinned
    r.show(Page::Weather, 5000);
    CHECK(r.tick(5100, 0b111, {}).page == Page::Air);         // fade out first
    CHECK(r.tick(5125, 0b111, {}).page == Page::Weather);
    r.tick(5130, 0b111, {});
    r.next(5130);
    r.tick(5131, 0b111, {});
    CHECK(r.tick(5200, 0b111, {}).page == Page::Clock);       // next after weather wraps to clock
}

TEST_CASE("draw: shapes stay inside the round panel and blend") {
    std::vector<gfx::Rgb565be> fb(240 * 240, 0);
    const screen::Canvas cv{fb};
    const auto red = screen::draw::hex(0xFF0000);
    screen::draw::circle(cv, 120, 120, 30, red);
    const int n = count_color(fb, red);
    CHECK(n > 2700);  // pi * 30^2 = 2827, the edge is blended
    CHECK(n < 2900);
    std::fill(fb.begin(), fb.end(), 0);
    screen::draw::circle(cv, 0, 0, 40, red);  // corner: outside the visible circle
    CHECK(count_color(fb, red) == 0);
    CHECK(screen::draw::mix(0, red, 256) == red);
    CHECK(screen::draw::mix(red, 0, 0) == red);
    std::fill(fb.begin(), fb.end(), red);
    screen::draw::dim(cv, 0);
    CHECK(fb[120 * 240 + 120] == 0);
    CHECK(fb[0] == red);  // corners are not touched
}

TEST_CASE("every page renders something for every state") {
    std::vector<gfx::Rgb565be> a(240 * 240), b(240 * 240);
    screen::Inputs in;
    in.time_valid = true; in.hour = 9; in.minute = 41;
    in.sensor_ok = true; in.reading = {22, 45, 1013, 90000, true};
    in.weather_ok = true;
    for (int lvl = 0; lvl < 5; ++lvl) {
        in.quality = {50, static_cast<sensor::Level>(lvl)};
        in.t_ms = 1000;
        screen::render_page({a}, Page::Air, in);
        in.t_ms = 1350;
        screen::render_page({b}, Page::Air, in);
        CHECK(a != b);  // animated
    }
    for (int s = 0; s <= static_cast<int>(Sky::Thunder); ++s) {
        in.weather.sky = static_cast<Sky>(s);
        in.t_ms = 2000;
        screen::render_page({a}, Page::Weather, in);
        in.t_ms = 2500;
        screen::render_page({b}, Page::Weather, in);
        CHECK(a != b);
    }
    screen::render_page({a}, Page::Clock, in);
    CHECK(count_color(a, gfx::rgb565_rt(255, 255, 255)) > 400);
}
