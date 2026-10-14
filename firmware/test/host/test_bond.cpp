#include <cstring>

#include "doctest.h"
#include "screen/core/bond.hpp"

using screen::Action;
using screen::Bond;

TEST_CASE("bond: names round-trip") {
    for (int i = 0; i < screen::kActionCount; ++i) {
        const auto a = static_cast<Action>(i);
        CHECK(screen::action_from_name(screen::action_name(a)) == a);
    }
    CHECK_FALSE(screen::action_from_name("bite").has_value());
    CHECK(std::strcmp(screen::level_name(4), "FAMILY") == 0);
}

TEST_CASE("bond: one visit a day is worth most, more actions a little, spamming nothing") {
    Bond b;
    auto r = screen::interact(b, 1000, Action::Feed, 1);
    CHECK(r.first_today); CHECK(r.gain == screen::kFirstGain); CHECK_FALSE(r.welcome_back);
    r = screen::interact(b, 1000, Action::Feed, 1);
    CHECK(r.gain == 0);                                  // the same action again
    r = screen::interact(b, 1000, Action::Coffee);
    CHECK(r.gain == screen::kMoreGain);
    r = screen::interact(b, 1000, Action::Party);
    CHECK(r.gain == screen::kDayCap - screen::kFirstGain - screen::kMoreGain);  // the day's cap
    r = screen::interact(b, 1000, Action::Pet);
    CHECK(r.gain == 0);
    CHECK(b.score == screen::kDayCap);
    CHECK(b.streak == 1); CHECK(b.total == 5);
}

TEST_CASE("bond: too many treats make a round belly, never a refusal") {
    Bond b;
    for (int i = 0; i < 3; ++i) CHECK_FALSE(screen::interact(b, 5, Action::Feed).too_full);
    const auto r = screen::interact(b, 5, Action::Feed);
    CHECK(r.too_full); CHECK(r.gain == 0);
    CHECK_FALSE(screen::interact(b, 6, Action::Feed).too_full);  // a new day, a new appetite
}

TEST_CASE("bond: daily visits climb the levels; streaks count days in a row") {
    Bond b;
    int day = 20000, ups = 0;
    for (int i = 0; i < 9; ++i, ++day) {
        ups += screen::interact(b, day, Action::Feed).level_up;
        ups += screen::interact(b, day, Action::Pet).level_up;
    }
    CHECK(b.score == 9 * (screen::kFirstGain + screen::kMoreGain));
    CHECK(screen::level(b) == 4);
    CHECK(ups == 4);
    CHECK(b.streak == 9); CHECK(b.best_streak == 9);
    screen::interact(b, day + 1, Action::Pet);  // skipped a day
    CHECK(b.streak == 1); CHECK(b.best_streak == 9);
}

TEST_CASE("bond: days away fade it gently, and a heart level is never lost") {
    Bond b;
    int day = 100;
    for (int i = 0; i < 4; ++i, ++day) { screen::interact(b, day, Action::Feed); screen::interact(b, day, Action::Pet); }
    CHECK(b.score == 44);
    CHECK(screen::level(b) == 2);
    const int last = day - 1;
    screen::settle(b, last + screen::kGraceDays);
    CHECK(b.score == 44);                       // a few days off: nothing
    screen::settle(b, last + screen::kGraceDays + 3);
    CHECK(b.score == 41);                       // then one point a day
    screen::settle(b, last + 100);
    CHECK(b.score == 40);                       // ... but never below the level reached
    CHECK(screen::level(b) == 2);
    const auto r = screen::interact(b, last + 101, Action::Coffee);
    CHECK(r.welcome_back); CHECK(r.first_today);
    CHECK(b.score == 48);
}

TEST_CASE("bond: gifts fill the shelf") {
    Bond b;
    CHECK(screen::interact(b, 1, Action::Gift, 2).new_gift);
    CHECK_FALSE(screen::interact(b, 2, Action::Gift, 2).new_gift);
    CHECK(screen::interact(b, 3, Action::Gift, 99).item == screen::kGiftCount - 1);  // clamped
    CHECK(b.gifts == ((1u << 2) | (1u << (screen::kGiftCount - 1))));
}

TEST_CASE("bond: survives a reboot, broken saves start fresh") {
    Bond b;
    for (int d = 1; d <= 6; ++d) screen::interact(b, d, Action::Feed), screen::interact(b, d, Action::Gift, d % 6);
    char buf[96];
    screen::bond_save(b, buf, sizeof buf);
    const Bond c = screen::bond_load(buf);
    CHECK(c.score == b.score); CHECK(c.best == b.best); CHECK(c.streak == b.streak); CHECK(c.last_day == b.last_day);
    CHECK(c.gifts == b.gifts); CHECK(c.total == b.total); CHECK(c.done == b.done);
    for (const char* bad : {"", "b1 1 2", "b1 500 0 0 0 0 0 0 0 0 0 0 0", "b2 1 0 0 0 0 0 0 0 0 0 0 0", static_cast<const char*>(nullptr)})
        CHECK(screen::bond_load(bad).score == 0);
}

#include <vector>

#include "screen/core/day.hpp"
#include "screen/core/fun.hpp"
#include "screen/core/pages.hpp"

namespace {
screen::Inputs friend_at(int y, int m, int d, int h, int lvl) {
    screen::Inputs in;
    in.time_valid = true; in.year = y; in.month = m; in.day = d; in.hour = h;
    in.weekday = screen::cal::weekday(y, m, d);
    in.sensor_ok = true; in.reading = {21, 45, 1013, 90000, true}; in.quality = {80, sensor::Level::Good};
    in.weather_ok = true; in.weather = {15, true, 2, weather::Sky::PartlyCloudy, 17, 8, 10};
    in.bond.score = lvl * 20 + 5; in.bond.best = lvl;
    return in;
}
}  // namespace

TEST_CASE("bond: the family perks come with the fifth heart") {
    using screen::fun::Visitor;
    using screen::fun::bit;
    const auto air = [](const screen::Inputs& in) { return screen::fun::allowed_visitors(screen::Page::Air, in); };
    CHECK((air(friend_at(2026, 10, 14, 11, 3)) & (bit(Visitor::Fox) | bit(Visitor::Friend))) == 0u);
    CHECK((air(friend_at(2026, 10, 14, 11, 4)) & bit(Visitor::Fox)) != 0u);
    CHECK((air(friend_at(2026, 10, 14, 11, 4)) & bit(Visitor::Friend)) == 0u);  // a Wednesday
    CHECK((air(friend_at(2026, 10, 17, 11, 4)) & bit(Visitor::Friend)) != 0u);  // Saturday
    int plain = 0, lucky = 0, hellos = 0;
    for (std::uint32_t t = 0; t < 40u * 24u * 3600u * 1000u; t += 5000) {
        plain += screen::fun::golden(t, 9) >= 0;
        lucky += screen::fun::golden(t, 9, true) >= 0;
    }
    for (std::uint32_t t = 0; t < 600000; t += 100) hellos += screen::fun::hello(t, 9) > 0.5f;
    CHECK(lucky > plain);  // best friends find more golden acorns
    CHECK(hellos > 0);
}

TEST_CASE("bond: every reaction renders through its whole run, at every level") {
    // Runs under ASan/UBSan.
    std::vector<gfx::Rgb565be> a(240 * 240), b(240 * 240);
    for (int act = 0; act < screen::kActionCount; ++act)
        for (int lvl = 0; lvl < screen::kLevels; ++lvl) {
            auto in = friend_at(2026, 10, 14, 11, lvl);
            in.react.active = true;
            in.react.r.action = static_cast<Action>(act);
            in.react.r.item = act % 3;
            in.react.r.level = lvl;
            in.react.r.too_full = lvl == 1;
            in.react.r.welcome_back = lvl == 2;
            in.react.r.level_up = lvl == 3;
            std::strcpy(in.pet_name, lvl >= 2 ? "NUTTY" : "");
            for (int i = 0; i <= 10; ++i) {
                in.react.k = static_cast<float>(i) / 10.0f;
                in.t_ms = 1000u + static_cast<std::uint32_t>(i) * 1100u;
                screen::render_page({a}, screen::Page::Air, in);
            }
            auto quiet = in;
            quiet.react.active = false;
            screen::render_page({b}, screen::Page::Air, quiet);
            CHECK(a != b);  // the reaction shows
        }
    auto shelf = friend_at(2026, 10, 14, 10, 4);
    for (unsigned g = 0; g < (1u << screen::kGiftCount); g += 7) {
        shelf.bond.gifts = g;
        screen::render_page({a}, screen::Page::Day, shelf);
    }
}
