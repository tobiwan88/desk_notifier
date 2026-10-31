#include <set>
#include <vector>

#include "doctest.h"
#include "screen/core/day.hpp"
#include "screen/core/mascot_view.hpp"
#include "screen/core/pages.hpp"
#include "screen/core/routine.hpp"

using screen::Act;
using screen::Mascot;

namespace {
constexpr int kSun = 0, kMon = 1, kThu = 4, kFri = 5, kSat = 6;
Act at(Mascot m, int wd, int h, int mi, bool dry = true) {
    const bool off = wd == kSun || wd == kSat;
    return screen::routine_for(m, screen::default_profile(m), true, wd, h, mi, off, dry);
}
}  // namespace

TEST_CASE("the bunny: apprentice gardener and gamer") {
    const Mascot b = Mascot::Bunny;
    CHECK(at(b, kMon, 5, 44) == Act::Sleep);
    CHECK(at(b, kMon, 5, 45) == Act::Alarm);
    CHECK(at(b, kMon, 6, 15) == Act::GetReady);
    CHECK(at(b, kMon, 7, 0) == Act::Gardening);
    CHECK(at(b, kMon, 12, 10) == Act::LunchBreak);
    CHECK(at(b, kMon, 12, 30) == Act::Gardening);
    CHECK(at(b, kMon, 16, 0) == Act::HomeFromWork);
    CHECK(at(b, kMon, 17, 0) == Act::Gaming);
    CHECK(at(b, kMon, 19, 10) == Act::Dinner);
    CHECK(at(b, kMon, 22, 59) == Act::Gaming);
    CHECK(at(b, kMon, 23, 0) == Act::Sleep);
    // Friday and Saturday night out, home at one
    CHECK(at(b, kFri, 21, 0) == Act::OutWithFriends);
    CHECK(at(b, kSat, 0, 30) == Act::OutWithFriends);
    CHECK(at(b, kSat, 1, 0) == Act::Sleep);
    // the weekend: asleep until 10:30, breakfast, then games
    CHECK(at(b, kSat, 10, 29) == Act::Sleep);
    CHECK(at(b, kSat, 10, 30) == Act::Breakfast);
    CHECK(at(b, kSat, 11, 30) == Act::Gaming);
    CHECK(at(b, kSun, 19, 0) == Act::Dinner);
    CHECK(at(b, kSun, 21, 30) == Act::Gaming);  // no night out on Sunday
}

TEST_CASE("the stag: active mornings, backstage on show nights") {
    const Mascot s = Mascot::Stag;
    CHECK(at(s, kMon, 6, 59) == Act::Sleep);
    CHECK(at(s, kMon, 7, 0) == Act::Morning);
    CHECK(at(s, kMon, 9, 0) == Act::BikeRide);
    CHECK(at(s, kMon, 9, 0, false) == Act::Workshop);  // rain: the workshop instead
    CHECK(at(s, kMon, 12, 0) == Act::Lunch);
    CHECK(at(s, kMon, 14, 0) == Act::Garden);
    CHECK(at(s, kMon, 18, 30) == Act::Dinner);
    CHECK(at(s, kMon, 21, 0) == Act::Tv);
    CHECK(at(s, kMon, 22, 30) == Act::Sleep);
    // show nights Thursday to Saturday
    CHECK(at(s, kThu, 17, 0) == Act::Dinner);
    CHECK(at(s, kThu, 17, 30) == Act::Backstage);
    CHECK(at(s, kThu, 22, 45) == Act::Backstage);
    CHECK(at(s, kThu, 23, 0) == Act::Sleep);
    CHECK(at(s, kSat, 20, 0) == Act::Backstage);
    // a free Sunday: a longer ride, lunch an hour later, coffee and cake
    CHECK(at(s, kSun, 12, 30) == Act::BikeRide);
    CHECK(at(s, kSun, 13, 0) == Act::Lunch);
    CHECK(at(s, kSun, 15, 0) == Act::CoffeeCake);
    // the show nights come from the profile
    screen::Profile p = screen::default_profile(s);
    p.show_days = 1u << kMon;
    CHECK(screen::routine_for(s, p, true, kMon, 20, 0, false) == Act::Backstage);
    CHECK(screen::routine_for(s, p, true, kThu, 20, 0, false) == Act::Tv);
}

TEST_CASE("the lucky cat: the dog sets the day") {
    const Mascot c = Mascot::Cat;
    CHECK(at(c, kMon, 6, 29) == Act::Sleep);
    CHECK(at(c, kMon, 6, 30) == Act::WakeUp);
    CHECK(at(c, kMon, 7, 0) == Act::DogWalk);
    CHECK(at(c, kMon, 8, 0) == Act::Breakfast);
    CHECK(at(c, kMon, 10, 0) == Act::Errands);
    CHECK(at(c, kMon, 12, 0) == Act::Lunch);
    CHECK(at(c, kMon, 13, 30) == Act::Rest);
    CHECK(at(c, kMon, 15, 0) == Act::CoffeeCake);
    CHECK(at(c, kMon, 17, 10) == Act::DogWalk);
    CHECK(at(c, kMon, 18, 0) == Act::Supper);
    CHECK(at(c, kMon, 20, 0) == Act::Cosy);
    CHECK(at(c, kMon, 22, 0) == Act::Sleep);
    CHECK(at(c, kSat, 7, 0) == Act::WakeUp);  // half an hour later at the weekend
    CHECK(at(c, kSun, 9, 0) == Act::DogWalk);  // the long Sunday walk
    screen::Profile p = screen::default_profile(c);
    p.evening_walk = false;
    CHECK(screen::routine_for(c, p, true, kMon, 17, 10, false) == Act::Errands);
}

TEST_CASE("the penguin: retired, in her garden") {
    const Mascot g = Mascot::Penguin;
    CHECK(at(g, kMon, 6, 30) == Act::Morning);
    CHECK(at(g, kMon, 9, 0) == Act::Gardening);
    CHECK(at(g, kMon, 11, 0) == Act::Knitting);
    CHECK(at(g, kMon, 12, 0) == Act::Lunch);
    CHECK(at(g, kMon, 13, 0) == Act::Nap);
    CHECK(at(g, kMon, 14, 45) == Act::Knitting);
    CHECK(at(g, kMon, 15, 0) == Act::CoffeeCake);
    CHECK(at(g, kMon, 16, 30) == Act::Birds);
    CHECK(at(g, kMon, 18, 30) == Act::Supper);
    CHECK(at(g, kMon, 20, 0) == Act::Tv);
    CHECK(at(g, kMon, 21, 30) == Act::Sleep);
    CHECK(at(g, kSun, 10, 0) == Act::Church);
    CHECK(at(g, kSun, 12, 0) == Act::Roast);
}

TEST_CASE("every mascot's week uses only acts it has a label for, and sleeps without a time") {
    for (int mi = 1; mi < screen::kMascotCount; ++mi) {
        const auto m = static_cast<Mascot>(mi);
        CHECK(screen::routine_for(m, screen::default_profile(m), false, kMon, 12, 0, false) == Act::Sleep);
        std::set<int> seen;
        for (int wd = 0; wd < 7; ++wd)
            for (int q = 0; q < 24 * 60; q += 5) seen.insert(static_cast<int>(at(m, wd, q / 60, q % 60)));
        CHECK(seen.size() >= 9);
        for (const int a : seen) {
            CHECK(std::string(screen::act_label(static_cast<Act>(a))).size() > 0);
            CHECK(screen::text_width(screen::act_label(static_cast<Act>(a)), 2) <= 156);  // fits the round caption
        }
    }
}

TEST_CASE("the lucky cat's coat: calico most days, the same all day") {
    int counts[3] = {};
    for (int d = 0; d < 700; ++d) ++counts[static_cast<int>(screen::cat_coat(20000 + d))];
    CHECK(counts[0] > 400);  // about 5 in 7
    CHECK(counts[1] > 50);
    CHECK(counts[2] > 50);
    CHECK(screen::cat_coat(20500) == screen::cat_coat(20500));
}

TEST_CASE("the new mascots dress for the weather") {
    weather::Forecast f;
    f.temp_c = 3;
    auto w = screen::wear_for(true, f);
    CHECK(w.hat); CHECK(w.scarf); CHECK(!w.summer);
    f.temp_c = 15;
    w = screen::wear_for(true, f);
    CHECK(!w.hat); CHECK(!w.summer);
    f.temp_c = 27;
    w = screen::wear_for(true, f);
    CHECK(w.summer); CHECK(!w.hat);
    CHECK(!screen::wear_for(false, f).summer);  // no forecast, no outfit
}

TEST_CASE("the new mascots' pages render and move, the squirrel's stay the same") {
    std::vector<gfx::Rgb565be> a(240 * 240), b(240 * 240), sq(240 * 240), sq2(240 * 240);
    screen::Inputs in;
    in.time_valid = true; in.year = 2026; in.month = 10; in.day = 12; in.weekday = kMon; in.hour = 9; in.minute = 41;
    in.sensor_ok = true; in.reading = {22, 45, 1013, 90000, true};
    in.quality = {62, sensor::Level::Fair};
    in.weather_ok = true; in.weather.temp_c = 12; in.weather.is_day = true;
    in.t_ms = 1000;
    screen::render_page({sq}, screen::Page::Day, in);
    for (int mi = 1; mi < screen::kMascotCount; ++mi) {
        in.mascot = static_cast<Mascot>(mi);
        in.profile = screen::default_profile(in.mascot);
        for (const auto page : {screen::Page::Clock, screen::Page::Air, screen::Page::Day}) {
            for (int h = 0; h < 24; h += 3) {
                in.hour = h;
                in.t_ms = 1000;
                screen::render_page({a}, page, in);
                in.t_ms = 2700;
                screen::render_page({b}, page, in);
                CHECK(a != b);
            }
        }
        in.hour = 9;
        in.t_ms = 1000;
        screen::render_page({a}, screen::Page::Day, in);
        CHECK(a != sq);  // not the squirrel's room
    }
    in.mascot = Mascot::Squirrel;
    screen::render_page({sq2}, screen::Page::Day, in);
    CHECK(sq == sq2);
}

TEST_CASE("add-ons: no visitors and no plant when switched off") {
    screen::Inputs in;
    in.time_valid = true; in.hour = 11; in.month = 7; in.weather_ok = true; in.weather.is_day = true;
    in.weather.temp_c = 22; in.weather.sky = weather::Sky::Clear;
    CHECK(screen::fun::allowed_visitors(screen::Page::Air, in) != 0);
    in.addons &= ~screen::kAddonVisitors;
    CHECK(screen::fun::allowed_visitors(screen::Page::Air, in) == 0);
    CHECK(screen::fun::allowed_visitors(screen::Page::Day, in) == 0);
}
