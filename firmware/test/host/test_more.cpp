// The moon, countdowns, howling nights, the window-sill plant, rain memory, the golden acorn, gags and puns.
#include <cstring>
#include <set>
#include <vector>

#include "doctest.h"
#include "screen/core/day.hpp"
#include "screen/core/fun.hpp"
#include "screen/core/memory.hpp"
#include "screen/core/pages.hpp"

using screen::fun::Gag;
using screen::fun::Visitor;
using screen::fun::bit;

namespace {
screen::Inputs at(int y, int m, int d, int h, int mi = 0) {
    screen::Inputs in;
    in.time_valid = true; in.year = y; in.month = m; in.day = d; in.hour = h; in.minute = mi;
    in.weekday = screen::cal::weekday(y, m, d);
    in.sensor_ok = true; in.reading = {21, 45, 1013, 90000, true}; in.quality = {80, sensor::Level::Good};
    in.weather_ok = true; in.weather = {12, true, 3, weather::Sky::Cloudy, 14, 6, 20};
    in.schedule.birthday = 202; in.schedule.anniversary = 1023;
    std::strcpy(in.schedule.birthday_name, "ALEX");
    return in;
}
}  // namespace

TEST_CASE("moon phase and full moons") {
    using namespace screen::cal;
    CHECK(moon_phase(2026, 10, 26, 12) == doctest::Approx(0.5).epsilon(0.08));
    const float nm = moon_phase(2026, 10, 10, 12);
    CHECK((nm < 0.06f || nm > 0.94f));
    CHECK(moon_phase(2026, 10, 18, 12) == doctest::Approx(0.25).epsilon(0.2));
    CHECK(full_moon(2026, 10, 26, 20));
    CHECK_FALSE(full_moon(2026, 10, 18, 20));
    for (int d = 1; d <= 28; ++d) {
        const float p = moon_phase(2027, 2, d, 0);
        CHECK(p >= 0.0f); CHECK(p < 1.0f);
    }
}

TEST_CASE("countdowns to the personal days and Christmas") {
    using screen::Occasion;
    CHECK(screen::cal::days_until(2026, 10, 13, 1023) == 10);
    CHECK(screen::cal::days_until(2026, 10, 23, 1023) == 0);
    CHECK(screen::cal::days_until(2026, 12, 31, 101) == 1);
    CHECK(screen::cal::days_until(2026, 10, 24, 1023) == 364);
    auto sc = screen::scene_for(at(2027, 1, 30, 10));
    CHECK(sc.countdown == 3); CHECK(sc.countdown_to == Occasion::Birthday);
    sc = screen::scene_for(at(2026, 10, 18, 10));
    CHECK(sc.countdown == 5); CHECK(sc.countdown_to == Occasion::Anniversary);
    sc = screen::scene_for(at(2026, 12, 20, 10));
    CHECK(sc.countdown == 4); CHECK(sc.countdown_to == Occasion::Christmas);
    sc = screen::scene_for(at(2026, 10, 13, 10));
    CHECK(sc.countdown == 0);  // 10 days: too early
    CHECK(screen::scene_for(at(2026, 10, 23, 10)).countdown == 0);  // on the day itself it is the occasion
}

TEST_CASE("howling nights") {
    auto in = at(2026, 10, 26, 22);  // full moon, reading
    CHECK(screen::scene_for(in).howl_night);
    in = at(2026, 10, 18, 22);
    CHECK_FALSE(screen::scene_for(in).howl_night);
    in = at(2026, 10, 31, 20, 30);   // Halloween
    CHECK(screen::scene_for(in).howl_night);
    in = at(2026, 10, 26, 2);        // asleep
    CHECK_FALSE(screen::scene_for(in).howl_night);
    int loud = 0;
    for (std::uint32_t t = 0; t < 600000; t += 100) {
        const float h = screen::fun::howl(t, 5);
        CHECK(h >= 0.0f); CHECK(h <= 1.0f);
        loud += h > 0.5f;
    }
    CHECK(loud > 0);
}

namespace {
void live_day(screen::Plant& p, int day, int good, int bad, int warm = 0) {
    for (int i = 0; i < good; ++i) screen::plant_minute(p, day, true, sensor::Level::Good);
    for (int i = 0; i < bad; ++i) screen::plant_minute(p, day, true, sensor::Level::Poor);
    for (int i = 0; i < warm; ++i) screen::plant_minute(p, day, true, sensor::Level::WarmingUp);
}
}  // namespace

TEST_CASE("the window-sill plant grows with good air and wilts with bad") {
    screen::Plant p;
    CHECK(p.leaves == 1);
    int day = 20261001;
    live_day(p, day, 600, 100);
    CHECK(p.leaves == 1);                        // the day is not over yet
    CHECK(screen::plant_minute(p, ++day, true, sensor::Level::Good));  // a new day closes the old one
    CHECK(p.leaves == 2); CHECK(p.grew);
    for (int i = 0; i < 12; ++i) live_day(p, ++day, 600, 100);
    screen::plant_minute(p, ++day, true, sensor::Level::Good);
    CHECK(p.leaves == screen::kPlantMaxLeaves);
    CHECK_FALSE(p.flower);
    for (int i = 0; i < 20; ++i) live_day(p, ++day, 700, 0);
    screen::plant_minute(p, ++day, false, sensor::Level::Good);
    CHECK(p.flower);                             // a month of good days
    CHECK_FALSE(p.grew);                         // already full
    live_day(p, day, 30, 0);                     // too few readings: changes nothing
    screen::plant_minute(p, ++day, false, sensor::Level::Good);
    CHECK(p.flower); CHECK(p.leaves == screen::kPlantMaxLeaves);
    for (int i = 0; i < 4; ++i) live_day(p, ++day, 300, 400);
    screen::plant_minute(p, ++day, false, sensor::Level::Good);
    CHECK(p.droop == screen::kPlantMaxDroop);
    CHECK(p.leaves == screen::kPlantMaxLeaves - 2);  // lost a leaf on each day at the worst droop
    CHECK_FALSE(p.flower);
    live_day(p, day, 700, 0);
    screen::plant_minute(p, ++day, false, sensor::Level::Good);
    CHECK(p.droop == screen::kPlantMaxDroop - 1);    // perks up again
}

TEST_CASE("the plant survives a reboot, and bad saves give a fresh plant") {
    screen::Plant p;
    p.leaves = 7; p.droop = 1; p.streak = 12; p.flower = false; p.grew = true; p.day = 20261013; p.good = 300; p.bad = 20; p.total = 400;
    char buf[64];
    screen::plant_save(p, buf, sizeof buf);
    const auto q = screen::plant_load(buf);
    CHECK(q.leaves == 7); CHECK(q.droop == 1); CHECK(q.streak == 12); CHECK(q.grew); CHECK(q.day == 20261013);
    CHECK(q.good == 300); CHECK(q.bad == 20); CHECK(q.total == 400);
    for (const char* bad : {"", "nonsense", "p1 99 0 0 0 0 0 0 0 0", "p1 3 0 0 0 0 0 50 50 10", "p2 1 2 3", static_cast<const char*>(nullptr)})
        CHECK(screen::plant_load(bad).leaves == 1);
}

TEST_CASE("rain memory") {
    screen::RainWatch r;
    CHECK(r.since_min(1000) == -1);
    r.update(0, true, true);
    CHECK(r.since_min(0) == -1);                 // still raining
    r.update(60000, true, false);
    CHECK(r.since_min(60000 + 10 * 60000) == 11);
    r.update(70000, false, true);                // no weather: ignored
    CHECK(r.since_min(80000) == 1);              // still counted from the last wet reading
    CHECK(r.since_min(3u * 3600u * 1000u + 1000u) == -1);  // three hours later it is forgotten
}

TEST_CASE("rainbows after rain, in daylight, with the sun out") {
    auto in = at(2026, 10, 14, 15);
    in.weather.now_code = 2;  // partly cloudy now
    CHECK(screen::fun::rainbow_alpha(in) == 0);
    in.since_rain_min = 10;
    CHECK(screen::fun::rainbow_alpha(in) > 100);
    in.since_rain_min = 0;
    CHECK(screen::fun::rainbow_alpha(in) > 0);
    in.since_rain_min = 95;
    CHECK(screen::fun::rainbow_alpha(in) == 0);
    in.since_rain_min = 10; in.weather.now_code = 61;  // raining again
    CHECK(screen::fun::rainbow_alpha(in) == 0);
    in.weather.now_code = 2; in.weather.is_day = false;
    CHECK(screen::fun::rainbow_alpha(in) == 0);
}

TEST_CASE("the golden acorn is rare") {
    int found = 0;
    bool was = false;
    for (std::uint32_t t = 0; t < 30u * 24u * 3600u * 1000u; t += 2000) {
        const bool now = screen::fun::golden(t, 77) >= 0;
        found += now && !was;
        was = now;
    }
    CHECK(found >= 5);   // about once every day and a half ...
    CHECK(found <= 60);  // ... not every hour
}

TEST_CASE("seasonal visitors") {
    using screen::Page;
    using screen::fun::allowed_visitors;
    auto summer = at(2027, 7, 10, 14);
    summer.weather = {24, true, 0, weather::Sky::Clear, 27, 15, 0};
    CHECK((allowed_visitors(Page::Air, summer) & bit(Visitor::Bee)) != 0);
    auto spring = at(2027, 4, 14, 11);
    spring.weather = {16, true, 0, weather::Sky::Clear, 18, 6, 0};
    CHECK((allowed_visitors(Page::Air, spring) & bit(Visitor::Petals)) != 0);
    CHECK((allowed_visitors(Page::Air, spring) & (bit(Visitor::Bee) | bit(Visitor::Leaves) | bit(Visitor::Hedgehog))) == 0u);
    auto wet = at(2026, 10, 14, 15);
    wet.since_rain_min = 20; wet.weather.now_code = 3;
    CHECK((allowed_visitors(Page::Air, wet) & bit(Visitor::Snail)) != 0);
    CHECK((allowed_visitors(Page::Day, wet) & bit(Visitor::Snail)) != 0);
    wet.weather.now_code = 61;  // still raining: snails wait
    CHECK((allowed_visitors(Page::Air, wet) & bit(Visitor::Snail)) == 0u);
}

TEST_CASE("gags fit the moment, the time-window ones only in their window") {
    auto gags = [](const screen::Inputs& in) { return screen::allowed_gags(screen::scene_for(in), in); };
    CHECK(((gags(at(2026, 10, 13, 14, 0)) & bit(Gag::NodOff)) != 0u));          // Tuesday after lunch
    CHECK_FALSE(((gags(at(2026, 10, 13, 11, 0)) & bit(Gag::NodOff)) != 0u));
    CHECK(((gags(at(2026, 10, 16, 15, 0)) & bit(Gag::ClockWatch)) != 0u));      // Friday afternoon
    CHECK_FALSE(((gags(at(2026, 10, 15, 15, 0)) & bit(Gag::ClockWatch)) != 0u));
    CHECK(((gags(at(2026, 10, 12, 9, 0)) & bit(Gag::Sigh)) != 0u));             // Monday morning
    CHECK_FALSE(((gags(at(2026, 10, 13, 9, 0)) & bit(Gag::Sigh)) != 0u));
    CHECK(((gags(at(2026, 10, 13, 12, 20)) & bit(Gag::Cheeks)) != 0u));         // lunch
    CHECK(((gags(at(2026, 10, 13, 11, 0)) & bit(Gag::Pen)) != 0u));
    auto airing = at(2026, 10, 13, 10, 0);
    airing.quality = {40, sensor::Level::Stale};
    airing.weather = {18, true, 2, weather::Sky::PartlyCloudy, 20, 9, 10};
    CHECK(((gags(airing) & bit(Gag::Stretch)) != 0u));
    CHECK(gags(at(2026, 10, 13, 3, 0)) == 0u);                         // asleep
    const unsigned allowed = bit(Gag::Plane) | bit(Gag::Acorn) | bit(Gag::NodOff);
    std::set<int> seen;
    for (std::uint32_t t = 0; t < 2u * 3600u * 1000u; t += 500) {
        const auto g = screen::fun::gag(t, 3, allowed);
        if (g.gag != Gag::None) seen.insert(static_cast<int>(g.gag));
    }
    CHECK(seen == std::set<int>{static_cast<int>(Gag::NodOff)});        // in its window the mood comes first
}

TEST_CASE("puns are short enough for the caption") {
    int some = 0;
    for (std::uint32_t t = 0; t < 4u * 3600u * 1000u; t += 1000)
        for (int a = 0; a < screen::kActivityCount; ++a)
            if (const char* p = screen::fun::pun(t, 11, a)) {
                ++some;
                CHECK(std::strlen(p) <= 15);
                CHECK(screen::text_width(p, 2) <= 184);
            }
    CHECK(some > 0);
}

TEST_CASE("every new surprise renders through its whole run") {
    // Runs under ASan/UBSan.
    std::vector<gfx::Rgb565be> fb(240 * 240);
    auto run = [&](screen::Page page, screen::Inputs in, auto active) {
        std::uint32_t start = 0;
        for (std::uint32_t t = 0; t < 400000000u; t += 200)
            if (active(t, in)) { start = t; break; }
        CHECK(start > 0);
        if (start == 0) return;
        for (int i = 0; i < 30; ++i) {
            in.t_ms = start + static_cast<std::uint32_t>(i) * 700u;
            screen::render_page({fb}, page, in);
        }
    };
    auto visitor = [](screen::Page page, Visitor who) {
        return [page, who](std::uint32_t t, const screen::Inputs& in) {
            return screen::fun::visit(t, in.seed, screen::fun::allowed_visitors(page, in)).who == who;
        };
    };
    auto night = at(2026, 10, 14, 21, 30);
    night.weather = {9, false, 3, weather::Sky::Cloudy, 12, 6, 20};
    run(screen::Page::Day, night, visitor(screen::Page::Day, Visitor::Owl));
    auto dusk = at(2026, 10, 14, 18);
    dusk.weather = {11, false, 2, weather::Sky::PartlyCloudy, 14, 6, 10};
    run(screen::Page::Air, dusk, visitor(screen::Page::Air, Visitor::Hedgehog));
    auto wet = at(2026, 10, 14, 15);
    wet.since_rain_min = 25; wet.weather = {12, true, 63, weather::Sky::Rain, 14, 8, 70, 5, 3};
    run(screen::Page::Air, wet, visitor(screen::Page::Air, Visitor::Snail));
    run(screen::Page::Day, wet, visitor(screen::Page::Day, Visitor::Snail));
    auto summer = at(2027, 7, 10, 14);
    summer.weather = {24, true, 0, weather::Sky::Clear, 27, 15, 0};
    run(screen::Page::Air, summer, visitor(screen::Page::Air, Visitor::Bee));
    auto autumn = at(2026, 10, 14, 11);
    autumn.weather = {13, true, 2, weather::Sky::PartlyCloudy, 15, 6, 10};
    run(screen::Page::Air, autumn, visitor(screen::Page::Air, Visitor::Leaves));
    auto spring = at(2027, 4, 14, 11);
    spring.weather = {16, true, 0, weather::Sky::Clear, 18, 6, 0};
    run(screen::Page::Air, spring, visitor(screen::Page::Air, Visitor::Petals));
    run(screen::Page::Air, autumn, [](std::uint32_t t, const screen::Inputs& in) { return screen::fun::golden(t, in.seed) >= 0; });
    auto full = at(2026, 10, 26, 22);
    full.weather = {7, false, 0, weather::Sky::Clear, 12, 3, 0};
    auto howling = [](std::uint32_t t, const screen::Inputs& in) { return screen::fun::howl(t, in.seed) > 0; };
    run(screen::Page::Air, full, howling);
    run(screen::Page::Day, full, howling);
    run(screen::Page::Day, at(2026, 10, 31, 20, 30), howling);
    for (int g = 1; g < screen::fun::kGagCount; ++g) {
        static const int kWhen[][3] = {{0, 0, 0}, {13, 10, 0}, {13, 16, 0}, {13, 11, 0}, {13, 12, 20}, {13, 14, 0}, {16, 16, 0}, {12, 9, 15}, {13, 10, 0}};
        auto in = at(2026, 10, kWhen[g][0], kWhen[g][1], kWhen[g][2]);
        if (g == static_cast<int>(Gag::Stretch)) { in.quality = {40, sensor::Level::Stale}; in.weather = {18, true, 2, weather::Sky::PartlyCloudy, 20, 9, 10}; }
        run(screen::Page::Day, in, [g](std::uint32_t t, const screen::Inputs& i) {
            return static_cast<int>(screen::fun::gag(t, i.seed, screen::allowed_gags(screen::scene_for(i), i)).gag) == g;
        });
    }
    auto plant = at(2026, 10, 13, 10);
    for (int leaves = 0; leaves <= 9; ++leaves)
        for (int droop = 0; droop <= 3; ++droop) {
            plant.plant.leaves = leaves; plant.plant.droop = droop; plant.plant.flower = leaves == 9;
            screen::render_page({fb}, screen::Page::Day, plant);
        }
}
