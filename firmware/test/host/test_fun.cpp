#include <set>
#include <vector>

#include "doctest.h"
#include "screen/core/day.hpp"
#include "screen/core/fun.hpp"
#include "screen/core/pages.hpp"

using screen::fun::Visitor;
using screen::fun::bit;

TEST_CASE("quirks: replayable, seed-dependent, and each one shows up now and then") {
    constexpr std::uint32_t kSeed = 1234;
    int blink = 0, look = 0, ear = 0, tail = 0, hop = 0, wave = 0, toss = 0, frames = 0, differ = 0;
    for (std::uint32_t t = 0; t < 20u * 60u * 1000u; t += 50, ++frames) {
        const auto q = screen::fun::quirks(t, kSeed);
        const auto again = screen::fun::quirks(t, kSeed);
        CHECK(q.look == again.look);
        CHECK(q.blink == again.blink);
        CHECK(q.look >= -1.0f); CHECK(q.look <= 1.0f);
        CHECK(q.toss >= 0.0f); CHECK(q.toss < 1.0f);
        blink += q.blink; look += q.look != 0; ear += q.ear > 0; tail += q.tail != 0; hop += q.hop > 0;
        wave += q.wave > 0; toss += q.toss > 0;
        differ += screen::fun::quirks(t, kSeed + 1).blink != q.blink;
    }
    CHECK(blink > frames / 100);      // blinks often ...
    CHECK(blink < frames / 10);       // ... but briefly
    CHECK(look > 0); CHECK(ear > 0); CHECK(tail > 0); CHECK(hop > 0); CHECK(wave > 0); CHECK(toss > 0);
    CHECK(differ > 0);                // another seed, other moments
}

TEST_CASE("visits: only allowed visitors, now and then, each one eventually") {
    constexpr std::uint32_t kSeed = 99;
    CHECK(screen::fun::visit(123456, kSeed, 0).who == Visitor::None);
    const unsigned allowed = bit(Visitor::Bird) | bit(Visitor::Moth) | bit(Visitor::ShootingStar);
    std::set<int> seen;
    int busy = 0, frames = 0;
    for (std::uint32_t t = 0; t < 4u * 3600u * 1000u; t += 500, ++frames) {
        const auto v = screen::fun::visit(t, kSeed, allowed);
        if (v.who == Visitor::None) continue;
        ++busy;
        seen.insert(static_cast<int>(v.who));
        CHECK((allowed & bit(v.who)) != 0);
        CHECK(v.k >= 0.0f); CHECK(v.k < 1.0f);
    }
    CHECK(seen.size() == 3);
    CHECK(busy > frames / 20);  // about one visit every other minute ...
    CHECK(busy < frames / 4);   // ... not a crowd
}

namespace {
screen::Inputs base(int h, weather::Forecast w) {
    screen::Inputs in;
    in.time_valid = true; in.year = 2026; in.month = 10; in.day = 14; in.weekday = 3; in.hour = h;
    in.sensor_ok = true; in.reading = {21, 45, 1013, 90000, true}; in.quality = {80, sensor::Level::Good};
    in.weather_ok = true; in.weather = w;
    return in;
}
const weather::Forecast kSunny{19, true, 0, weather::Sky::Clear, 22, 10, 0}, kRain{9, true, 63, weather::Sky::Rain, 11, 6, 80},
    kNight{8, false, 0, weather::Sky::Clear, 13, 4, 0}, kCloudyNight{8, false, 3, weather::Sky::Cloudy, 13, 4, 0};
}  // namespace

TEST_CASE("who may visit") {
    using screen::Page;
    using screen::fun::allowed_visitors;
    CHECK(allowed_visitors(Page::Air, base(11, kSunny)) == (bit(Visitor::Bird) | bit(Visitor::Butterfly) | bit(Visitor::Leaves)));  // October
    CHECK(allowed_visitors(Page::Air, base(11, kRain)) == 0u);
    CHECK(allowed_visitors(Page::Air, base(23, kNight)) == (bit(Visitor::ShootingStar) | bit(Visitor::Hedgehog)));
    auto windy = base(11, kSunny); windy.weather.wind_kmh = 40;
    CHECK(allowed_visitors(Page::Air, windy) == 0u);
    CHECK(allowed_visitors(Page::Clock, base(11, kSunny)) == 0u);
    CHECK(allowed_visitors(Page::Day, base(10, kRain)) == bit(Visitor::Ladybug));           // working, rain outside
    CHECK(allowed_visitors(Page::Day, base(21, kCloudyNight)) == (bit(Visitor::Ladybug) | bit(Visitor::Moth) | bit(Visitor::Owl)));  // reading lamp
    CHECK(allowed_visitors(Page::Day, base(3, kNight)) == (bit(Visitor::ShootingStar) | bit(Visitor::Owl)));  // asleep: only the window
    auto no_time = base(10, kSunny); no_time.time_valid = false;
    CHECK(allowed_visitors(Page::Day, no_time) == 0u);
}

TEST_CASE("every visitor renders on its page through its whole visit") {
    // Runs under ASan/UBSan.
    struct Case { screen::Page page; int hour; weather::Forecast w; Visitor who; };
    const Case cases[] = {
        {screen::Page::Air, 11, kSunny, Visitor::Bird},       {screen::Page::Air, 11, kSunny, Visitor::Butterfly},
        {screen::Page::Air, 23, kNight, Visitor::ShootingStar}, {screen::Page::Day, 10, kSunny, Visitor::Bird},
        {screen::Page::Day, 10, kRain, Visitor::Ladybug},     {screen::Page::Day, 21, kCloudyNight, Visitor::Moth},
        {screen::Page::Day, 23, kNight, Visitor::ShootingStar}, {screen::Page::Day, 11, kSunny, Visitor::Butterfly},
    };
    std::vector<gfx::Rgb565be> fb(240 * 240), none(240 * 240);
    for (const Case& c : cases) {
        auto in = base(c.hour, c.w);
        if (c.page == screen::Page::Day && c.hour == 11) in.day = 17, in.weekday = 6;  // a Saturday: reading, warm
        const unsigned allowed = screen::fun::allowed_visitors(c.page, in);
        CHECK((allowed & bit(c.who)) != 0);
        if (!(allowed & bit(c.who))) continue;
        std::uint32_t start = 0;
        for (std::uint32_t t = 0; t < 36000000u; t += 100) {
            const auto v = screen::fun::visit(t, in.seed, allowed);
            if (v.who == c.who && v.k < 0.01f) { start = t; break; }
        }
        CHECK(start > 0);
        if (start == 0) continue;
        bool changed = false;
        for (int i = 0; i < 24; ++i) {
            in.t_ms = start + static_cast<std::uint32_t>(i) * 500u;
            screen::render_page({fb}, c.page, in);
            auto quiet = in;
            quiet.weather_ok = false;  // no visitors without weather ... except the ladybug and the moth
            if (c.who != Visitor::Ladybug && c.who != Visitor::Moth) {
                screen::render_page({none}, c.page, quiet);
                changed |= fb != none;
            } else {
                changed = true;
            }
        }
        CHECK(changed);
    }
}

TEST_CASE("on the full hour the clock squirrel pops up") {
    std::vector<gfx::Rgb565be> a(240 * 240), b(240 * 240);
    screen::Inputs in = base(10, kSunny);
    in.t_ms = 5000; in.minute = 0; in.second = 5;
    screen::render_page({a}, screen::Page::Clock, in);
    in.minute = 1;
    screen::render_page({b}, screen::Page::Clock, in);
    CHECK(a != b);
}
