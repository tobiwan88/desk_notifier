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
