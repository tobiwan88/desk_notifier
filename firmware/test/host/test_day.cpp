#include <array>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "doctest.h"
#include "screen/core/day.hpp"
#include "screen/core/pages.hpp"

using screen::Activity;

namespace {
constexpr int kSun = 0, kMon = 1, kFri = 5, kSat = 6;
Activity at(int wd, int h, int m, const screen::Schedule& s = {}) { return screen::activity_for(true, wd, h, m, s); }
}  // namespace

TEST_CASE("the squirrel's weekday follows the clock") {
    CHECK(at(kMon, 3, 0) == Activity::Sleep);
    CHECK(at(kMon, 6, 29) == Activity::Sleep);
    CHECK(at(kMon, 6, 30) == Activity::Coffee);
    CHECK(at(kMon, 8, 59) == Activity::Coffee);
    CHECK(at(kMon, 9, 0) == Activity::Work);
    CHECK(at(kMon, 11, 59) == Activity::Work);
    CHECK(at(kMon, 12, 0) == Activity::Lunch);
    CHECK(at(kMon, 12, 59) == Activity::Lunch);
    CHECK(at(kMon, 13, 0) == Activity::Work);
    CHECK(at(kMon, 17, 59) == Activity::Work);
    CHECK(at(kFri, 18, 0) == Activity::CloseLaptop);
    CHECK(at(kFri, 18, 29) == Activity::CloseLaptop);
    CHECK(at(kFri, 18, 30) == Activity::Read);
    CHECK(at(kMon, 22, 59) == Activity::Read);
    CHECK(at(kMon, 23, 0) == Activity::Sleep);
}

TEST_CASE("weekends: sleep in, long coffee, no work") {
    CHECK(at(kSat, 7, 30) == Activity::Sleep);
    CHECK(at(kSat, 8, 0) == Activity::Coffee);
    CHECK(at(kSun, 9, 59) == Activity::Coffee);
    CHECK(at(kSun, 10, 0) == Activity::Read);
    CHECK(at(kSun, 12, 30) == Activity::Lunch);
    CHECK(at(kSat, 15, 0) == Activity::Read);
    CHECK(at(kSat, 18, 10) == Activity::Read);  // nothing to close
    CHECK(at(kSun, 23, 30) == Activity::Sleep);
}

TEST_CASE("the schedule is configurable, also past midnight") {
    screen::Schedule s;
    s.wake = 800; s.work = 1000; s.lunch = 1330; s.end = 1900; s.bed = 30;  // a night owl
    CHECK(at(kMon, 7, 59, s) == Activity::Sleep);
    CHECK(at(kMon, 8, 0, s) == Activity::Coffee);
    CHECK(at(kMon, 12, 0, s) == Activity::Work);
    CHECK(at(kMon, 13, 30, s) == Activity::Lunch);
    CHECK(at(kMon, 23, 50, s) == Activity::Read);
    CHECK(at(kMon, 0, 15, s) == Activity::Read);
    CHECK(at(kMon, 0, 30, s) == Activity::Sleep);
}

TEST_CASE("no time yet: asleep; every activity is reachable and named") {
    CHECK(screen::activity_for(false, kMon, 10, 0) == Activity::Sleep);
    std::set<int> seen;
    for (int wd = 0; wd < 7; ++wd)
        for (int m = 0; m < 1440; m += 5) seen.insert(static_cast<int>(at(wd, m / 60, m % 60)));
    CHECK(static_cast<int>(seen.size()) == screen::kActivityCount);
    std::set<std::string> names;
    for (int i = 0; i < screen::kActivityCount; ++i) {
        const auto a = static_cast<Activity>(i);
        CHECK(std::strlen(screen::activity_label(a)) > 0);
        names.insert(screen::activity_name(a));
    }
    CHECK(static_cast<int>(names.size()) == screen::kActivityCount);
}

TEST_CASE("day page: every activity x weather x day/night x air renders, animates and differs") {
    // Runs under ASan/UBSan: catches out-of-bounds drawing in any combination.
    std::vector<gfx::Rgb565be> a(240 * 240), b(240 * 240);
    std::vector<std::vector<gfx::Rgb565be>> per_activity;
    static constexpr int kTimes[][2] = {{3, 0}, {7, 0}, {10, 0}, {12, 30}, {18, 10}, {20, 0}};  // one per activity on a Monday
    for (const auto& tm : kTimes) {
        screen::Inputs in;
        in.time_valid = true; in.weekday = kMon; in.hour = tm[0]; in.minute = tm[1];
        in.sensor_ok = true; in.reading = {22, 45, 1013, 90000, true}; in.quality = {70, sensor::Level::Good};
        for (int sky = 0; sky <= static_cast<int>(weather::Sky::Thunder); ++sky)
            for (const bool day : {true, false})
                for (int lvl = 0; lvl < 5; ++lvl) {
                    in.weather_ok = true;
                    in.weather = {10, day, 0, static_cast<weather::Sky>(sky), 12, 5, 30};
                    in.quality = {50, static_cast<sensor::Level>(lvl)};
                    for (const std::uint32_t t : {0u, 2345u, 987654u}) {
                        in.t_ms = t;
                        screen::render_page({a}, screen::Page::Day, in);
                    }
                }
        in.weather_ok = false;
        in.quality = {70, sensor::Level::Good};
        in.t_ms = 1000;
        screen::render_page({a}, screen::Page::Day, in);
        in.t_ms = 4700;
        screen::render_page({b}, screen::Page::Day, in);
        CHECK(a != b);  // animated
        per_activity.push_back(a);
    }
    for (std::size_t i = 1; i < per_activity.size(); ++i) CHECK(per_activity[i] != per_activity[i - 1]);
}

// ---- the calendar ----------------------------------------------------------------------------------------------

TEST_CASE("calendar: weekdays, Easter, last Sundays") {
    using namespace screen::cal;
    CHECK(weekday(2026, 10, 12) == 1);  // a Monday
    CHECK(weekday(1970, 1, 1) == 4);
    CHECK(weekday(2000, 2, 29) == 2);
    CHECK(easter_day_of_year(2026) == day_of_year(2026, 4, 5));
    CHECK(easter_day_of_year(2025) == day_of_year(2025, 4, 20));
    CHECK(easter_day_of_year(2024) == day_of_year(2024, 3, 31));
    CHECK(easter_day_of_year(2027) == day_of_year(2027, 3, 28));
    CHECK(last_sunday(2026, 10) == 25);
    CHECK(last_sunday(2026, 3) == 29);
    CHECK(last_sunday(2027, 3) == 28);
}

TEST_CASE("calendar: Bavarian public holidays") {
    using screen::cal::public_holiday;
    for (const auto& d : {std::array{2026, 1, 1}, {2026, 1, 6}, {2026, 4, 3}, {2026, 4, 6}, {2026, 5, 1}, {2026, 5, 14},
                          {2026, 5, 25}, {2026, 6, 4}, {2026, 8, 15}, {2026, 10, 3}, {2026, 11, 1}, {2026, 12, 25}, {2026, 12, 26}})
        CHECK(public_holiday(d[0], d[1], d[2]));
    CHECK_FALSE(public_holiday(2026, 10, 12));
    CHECK_FALSE(public_holiday(2026, 12, 24));
    // a holiday on a weekday is planned like a weekend: no laptop
    screen::Inputs in;
    in.time_valid = true; in.year = 2026; in.month = 5; in.day = 14; in.weekday = 4; in.hour = 10;
    CHECK(screen::scene_for(in).day_off);
    CHECK(screen::scene_for(in).activity != Activity::Work);
}

TEST_CASE("calendar: Oktoberfest and Advent") {
    using namespace screen::cal;
    CHECK_FALSE(oktoberfest(2026, 9, 18));
    CHECK(oktoberfest(2026, 9, 19));
    CHECK(oktoberfest(2026, 10, 4));
    CHECK_FALSE(oktoberfest(2026, 10, 5));
    CHECK(oktoberfest(2023, 9, 16));  // ends on 3 October that year
    CHECK(oktoberfest(2023, 10, 3));
    CHECK_FALSE(oktoberfest(2023, 10, 4));
    CHECK(advent_candles(2026, 11, 28) == 0);
    CHECK(advent_candles(2026, 11, 29) == 1);
    CHECK(advent_candles(2026, 12, 6) == 2);
    CHECK(advent_candles(2026, 12, 13) == 3);
    CHECK(advent_candles(2026, 12, 20) == 4);
    CHECK(advent_candles(2026, 12, 23) == 4);
    CHECK(advent_candles(2026, 12, 24) == 0);
    CHECK(advent_candles(2027, 11, 28) == 1);  // 24 December 2027 is a Friday
}

TEST_CASE("special days, the personal ones first") {
    using screen::Occasion;
    screen::Schedule s;
    s.birthday = 202; s.anniversary = 1023;
    CHECK(screen::occasion_for(2027, 2, 2, s) == Occasion::Birthday);
    CHECK(screen::occasion_for(2026, 10, 23, s) == Occasion::Anniversary);
    CHECK(screen::occasion_for(2026, 10, 23) == Occasion::None);  // nothing configured
    CHECK(screen::occasion_for(2026, 12, 24, s) == Occasion::Christmas);
    CHECK(screen::occasion_for(2026, 12, 31, s) == Occasion::NewYearsEve);
    CHECK(screen::occasion_for(2027, 1, 1, s) == Occasion::NewYear);
    CHECK(screen::occasion_for(2026, 4, 5, s) == Occasion::Easter);
    CHECK(screen::occasion_for(2026, 4, 6, s) == Occasion::Easter);
    CHECK(screen::occasion_for(2026, 12, 6, s) == Occasion::Nikolaus);
    CHECK(screen::occasion_for(2026, 11, 11, s) == Occasion::StMartin);
    CHECK(screen::occasion_for(2026, 10, 31, s) == Occasion::Halloween);
    CHECK(screen::occasion_for(2026, 10, 25, s) == Occasion::ClockChange);
    CHECK(screen::occasion_for(2027, 3, 28, s) == Occasion::Easter);  // Easter beats the clock change in 2027
    CHECK(screen::occasion_for(2026, 3, 29, s) == Occasion::ClockChange);
    CHECK(screen::occasion_for(2026, 9, 25, s) == Occasion::Oktoberfest);
    CHECK(screen::occasion_for(2027, 1, 21, s) == Occasion::SquirrelDay);
    CHECK(screen::occasion_for(2027, 2, 14, s) == Occasion::Valentine);
    CHECK(screen::occasion_for(2027, 4, 1, s) == Occasion::AprilFool);
    CHECK(screen::occasion_for(2026, 10, 12, s) == Occasion::None);
}

// ---- situations ------------------------------------------------------------------------------------------------

namespace {
screen::Inputs day_in(int y, int m, int d, int h, int mi) {
    screen::Inputs in;
    in.time_valid = true; in.year = y; in.month = m; in.day = d; in.hour = h; in.minute = mi;
    in.weekday = screen::cal::weekday(y, m, d);
    in.sensor_ok = true; in.reading = {21, 45, 1013, 90000, true}; in.quality = {80, sensor::Level::Good};
    in.weather_ok = true; in.weather = {12, true, 3, weather::Sky::Cloudy, 14, 6, 20};
    return in;
}
screen::Situation sit(const screen::Inputs& in) { return screen::scene_for(in).situation; }
}  // namespace

TEST_CASE("situations: the indoor climate") {
    using screen::Situation;
    auto in = day_in(2026, 10, 12, 10, 0);  // Monday, working
    CHECK(sit(in) == Situation::None);
    in.reading.temp_c = 26; CHECK(sit(in) == Situation::HotRoom);
    in.reading.temp_c = 17.9f; CHECK(sit(in) == Situation::ColdRoom);
    in.reading.temp_c = 21; in.reading.rh = 66; CHECK(sit(in) == Situation::Humid);
    in.reading.rh = 34; CHECK(sit(in) == Situation::DryAir);
    in.reading.rh = 45; in.quality = {40, sensor::Level::Stale};
    CHECK(sit(in) == Situation::AirOut);         // 12 °C and cloudy outside: nice enough to open the window
    in.weather.sky = weather::Sky::Rain; CHECK(sit(in) == Situation::Yawn);  // not in the rain: it yawns instead
    in.air_drop = true; CHECK(sit(in) == Situation::Smell);
    in.sensor_ok = false; CHECK(sit(in) == Situation::None);
}

TEST_CASE("situations: evenings, mornings and nights") {
    using screen::Situation;
    auto in = day_in(2026, 7, 15, 20, 30);  // Wednesday evening, reading
    in.reading.temp_c = 27; in.weather = {19, false, 0, weather::Sky::Clear, 30, 17, 0};
    CHECK(sit(in) == Situation::CoolDown);
    in.weather.temp_c = 26; CHECK(sit(in) == Situation::HotRoom);  // not cooler outside
    in = day_in(2026, 10, 14, 20, 30);
    in.weather.sky = weather::Sky::Rain; CHECK(sit(in) == Situation::Cocoa);
    in.weather.sky = weather::Sky::Thunder; CHECK(sit(in) == Situation::Storm);
    in.hour = 2; CHECK(sit(in) == Situation::Storm);  // also wakes it at night
    in.weather.sky = weather::Sky::Rain; CHECK(sit(in) == Situation::None);  // asleep: nothing else
    in = day_in(2026, 10, 13, 7, 30);  // Tuesday coffee
    in.weather.precip_pct = 60; CHECK(sit(in) == Situation::Umbrella);
    in = day_in(2026, 10, 17, 15, 0);  // Saturday afternoon
    in.weather = {20, true, 0, weather::Sky::Clear, 22, 10, 0}; CHECK(sit(in) == Situation::Watering);
}

TEST_CASE("New Year's Eve: up late with a sparkler, then a long lie-in") {
    using screen::Situation;
    auto in = day_in(2026, 12, 31, 21, 59);
    CHECK(sit(in) == Situation::None);
    in.hour = 22; in.minute = 0;
    CHECK(sit(in) == Situation::Party);
    in.hour = 23; in.minute = 50;
    CHECK(screen::scene_for(in).activity == Activity::Read);  // not asleep
    in = day_in(2027, 1, 1, 0, 20);
    CHECK(sit(in) == Situation::Party);
    in.minute = 30;
    CHECK(screen::scene_for(in).activity == Activity::Sleep);
    in.hour = 9; in.minute = 0;
    CHECK(screen::scene_for(in).activity == Activity::Sleep);  // sleeps in
    in.hour = 10;
    CHECK(screen::scene_for(in).activity == Activity::Coffee);
}

TEST_CASE("Friday evening and Monday morning") {
    auto in = day_in(2026, 10, 16, 18, 10);
    CHECK(screen::scene_for(in).friday_party);
    in.hour = 17;
    CHECK_FALSE(screen::scene_for(in).friday_party);
    in = day_in(2026, 10, 12, 7, 45);
    CHECK(screen::scene_for(in).monday);
    in = day_in(2026, 10, 13, 7, 45);
    CHECK_FALSE(screen::scene_for(in).monday);
}

TEST_CASE("air trend: a sudden drop, not a slow one, not while warming up") {
    screen::AirTrend tr;
    std::uint32_t t = 0;
    for (int i = 0; i < 10; ++i, t += 60000) tr.add(t, 80, true);
    CHECK_FALSE(tr.sudden_drop());
    tr.add(t, 64, true);
    CHECK(tr.sudden_drop());
    for (int i = 0; i < 31; ++i, t += 60000) tr.add(t, 64, true);  // half an hour later it is the new normal
    CHECK_FALSE(tr.sudden_drop());
    screen::AirTrend fresh;
    fresh.add(0, 80, true);
    fresh.add(1000, 40, true);
    CHECK_FALSE(fresh.sudden_drop());  // too little history
    tr.add(t, 0, false);
    CHECK_FALSE(tr.sudden_drop());
}

TEST_CASE("day page: every situation and every special day renders") {
    // Runs under ASan/UBSan: catches out-of-bounds drawing in the props.
    std::vector<gfx::Rgb565be> fb(240 * 240), b(240 * 240);
    std::set<int> sits, occs;
    struct Case { int y, m, d, h, mi; float tin, rh; sensor::Level lvl; bool drop; weather::Forecast w; };
    const weather::Forecast mild{18, true, 2, weather::Sky::PartlyCloudy, 20, 9, 10}, rain{9, false, 63, weather::Sky::Rain, 11, 6, 80};
    const Case cases[] = {
        {2026, 12, 31, 23, 0, 21, 45, sensor::Level::Good, false, {-1, false, 0, weather::Sky::Clear, 2, -4, 0}},
        {2026, 10, 12, 10, 0, 21, 45, sensor::Level::Good, true, mild},
        {2026, 10, 14, 21, 0, 21, 45, sensor::Level::Good, false, {16, false, 95, weather::Sky::Thunder, 20, 12, 90}},
        {2026, 10, 12, 10, 0, 21, 45, sensor::Level::Stale, false, mild},
        {2026, 7, 15, 20, 30, 27.5f, 45, sensor::Level::Good, false, {19, false, 0, weather::Sky::Clear, 30, 17, 0}},
        {2026, 10, 12, 10, 0, 21, 45, sensor::Level::Stale, false, {9, true, 63, weather::Sky::Rain, 11, 6, 80}},
        {2026, 10, 12, 14, 0, 27.5f, 45, sensor::Level::Good, false, mild},
        {2026, 10, 12, 10, 0, 16, 45, sensor::Level::Good, false, mild},
        {2026, 10, 12, 12, 30, 21, 72, sensor::Level::Good, false, mild},
        {2026, 10, 12, 10, 0, 21, 28, sensor::Level::Good, false, mild},
        {2026, 10, 13, 7, 30, 21, 45, sensor::Level::Good, false, {10, true, 61, weather::Sky::Cloudy, 12, 7, 80}},
        {2026, 10, 14, 20, 30, 21, 45, sensor::Level::Good, false, rain},
        {2026, 10, 17, 15, 0, 21, 45, sensor::Level::Good, false, {20, true, 0, weather::Sky::Clear, 22, 10, 0}},
        {2027, 2, 2, 10, 0, 21, 45, sensor::Level::Good, false, mild},
        {2026, 10, 23, 19, 30, 21, 45, sensor::Level::Good, false, rain},
        {2026, 12, 25, 9, 0, 21, 45, sensor::Level::Good, false, {-1, true, 73, weather::Sky::Snow, 1, -3, 60}},
        {2026, 12, 31, 16, 0, 21, 45, sensor::Level::Good, false, mild},
        {2027, 1, 1, 11, 0, 21, 45, sensor::Level::Good, false, mild},
        {2027, 3, 28, 10, 0, 21, 45, sensor::Level::Good, false, mild},
        {2026, 12, 6, 9, 0, 21, 45, sensor::Level::Good, false, {0, true, 45, weather::Sky::Fog, 2, -3, 20}},
        {2026, 11, 11, 18, 45, 21, 45, sensor::Level::Good, false, {5, false, 0, weather::Sky::Clear, 9, 2, 0, 40}},
        {2026, 10, 31, 19, 30, 21, 45, sensor::Level::Good, false, mild},
        {2026, 10, 25, 9, 0, 21, 45, sensor::Level::Good, false, mild},
        {2026, 9, 25, 12, 30, 21, 45, sensor::Level::Good, false, mild},
        {2027, 1, 21, 10, 0, 21, 45, sensor::Level::Good, false, {-3, true, 0, weather::Sky::Clear, 0, -8, 0}},
        {2027, 2, 14, 9, 0, 21, 45, sensor::Level::Good, false, mild},
        {2027, 4, 1, 10, 0, 21, 45, sensor::Level::Good, false, mild},
    };
    for (const Case& c : cases) {
        auto in = day_in(c.y, c.m, c.d, c.h, c.mi);
        in.schedule.birthday = 202; in.schedule.anniversary = 1023;
        std::strcpy(in.schedule.birthday_name, "MAYA");
        in.reading.temp_c = c.tin; in.reading.rh = c.rh; in.quality = {50, c.lvl}; in.air_drop = c.drop; in.weather = c.w;
        const auto sc = screen::scene_for(in);
        sits.insert(static_cast<int>(sc.situation));
        occs.insert(static_cast<int>(sc.occasion));
        for (const std::uint32_t t : {0u, 1234u, 4321u, 98765u, 1000000u}) {
            in.t_ms = t;
            screen::render_page({fb}, screen::Page::Day, in);
        }
        in.t_ms = 2000; screen::render_page({b}, screen::Page::Day, in);
        CHECK(fb != b);  // animated
    }
    CHECK(static_cast<int>(sits.size()) == screen::kSituationCount);
    CHECK(static_cast<int>(occs.size()) == screen::kOccasionCount);
}
