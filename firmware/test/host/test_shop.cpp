#include <algorithm>
#include <cstring>
#include <set>
#include <string>

#include "doctest.h"
#include "screen/core/bond.hpp"
#include "screen/core/day.hpp"
#include "screen/core/shop.hpp"

using screen::Bond;
using screen::Season;
using screen::Shelf;

namespace {
Bond family(int coins = 0) {
    Bond b;
    b.score = 85; b.best = 4; b.coins = coins; b.coins_total = coins;
    return b;
}
int day_of(int y, int m, int d) { return screen::cal::day_number(y, m, d); }
}  // namespace

TEST_CASE("shop: the catalogue is sane") {
    std::set<std::string> ids;
    for (int i = 0; i < screen::kShopItemCount; ++i) {
        const auto& it = screen::shop_item(i);
        ids.insert(it.id);
        CHECK(it.price > 0);
        CHECK(std::strlen(it.name) <= 15);  // fits the caption
        CHECK(screen::shop_find(it.id) == i);
        if (it.shelf == Shelf::Decor) CHECK(it.spot != screen::Spot::None);
    }
    CHECK(static_cast<int>(ids.size()) == screen::kShopItemCount);
    CHECK(screen::shop_find("unicorn") == -1);
}

TEST_CASE("shop: seasons") {
    CHECK(screen::in_season(Season::Halloween, 2026, 10, 17));
    CHECK_FALSE(screen::in_season(Season::Halloween, 2026, 11, 3));
    CHECK(screen::in_season(Season::Christmas, 2026, 12, 24));
    CHECK(screen::in_season(Season::NewYear, 2026, 12, 31));
    CHECK(screen::in_season(Season::NewYear, 2027, 1, 6));
    CHECK_FALSE(screen::in_season(Season::NewYear, 2027, 1, 7));
    CHECK(screen::in_season(Season::Easter, 2027, 3, 20));  // Easter Sunday is 28 March 2027
    CHECK_FALSE(screen::in_season(Season::Easter, 2027, 4, 10));
    CHECK(screen::in_season(Season::Oktoberfest, 2026, 9, 25));
    CHECK(screen::in_season(Season::Summer, 2027, 7, 15));
    CHECK_FALSE(screen::in_season(Season::Summer, 2027, 9, 15));
}

TEST_CASE("shop: six a day, mixed by the date, seasonal ones only in season and first") {
    const Bond b = family();
    int a[6], c[6];
    const int na = screen::daily_offer(b, day_of(2026, 10, 17), 2026, 10, 17, a);
    CHECK(na == screen::kOfferSize);
    CHECK(screen::shop_item(a[0]).season == Season::Halloween);  // the pumpkin hat, it is October
    for (int i = 1; i < na; ++i) CHECK(screen::shop_item(a[i]).season == Season::Any);
    const int nc = screen::daily_offer(b, day_of(2026, 10, 17), 2026, 10, 17, c);
    CHECK(std::equal(a, a + na, c, c + nc));  // the same all day
    int changed = 0;
    for (int d = 18; d <= 24; ++d) {
        screen::daily_offer(b, day_of(2026, 10, d), 2026, 10, d, c);
        changed += !std::equal(a, a + na, c);
    }
    CHECK(changed >= 6);  // a new mix every day
    std::set<int> seen;
    for (int d = 0; d < 60; ++d) {
        const int n = screen::daily_offer(b, day_of(2027, 3, 1) + d, 2027, 3, 1, c);
        seen.insert(c, c + n);
    }
    for (int i : seen) CHECK(screen::shop_item(i).season == Season::Any);  // nothing seasonal in March (date fixed)
    CHECK(seen.size() >= 15);
}

TEST_CASE("shop: buying") {
    Bond b = family(500);
    const int day = day_of(2026, 10, 17);
    int offer[6];
    screen::daily_offer(b, day, 2026, 10, 17, offer);
    const int price = screen::shop_item(offer[1]).price;
    auto r = screen::buy(b, offer[1], day, 2026, 10, 17);
    CHECK(r.error == screen::BuyError::None);
    CHECK(screen::owns(b, offer[1]));
    CHECK(b.coins == 500 - price);
    CHECK(screen::buy(b, offer[1], day, 2026, 10, 17).error == screen::BuyError::Owned);
    int later[6];
    const int n = screen::daily_offer(b, day, 2026, 10, 17, later);
    CHECK(std::find(later, later + n, offer[1]) == later + n);  // gone from the shop
    int not_today = -1;
    for (int i = 0; i < screen::kShopItemCount && not_today < 0; ++i)
        if (!screen::owns(b, i) && std::find(later, later + n, i) == later + n) not_today = i;
    CHECK(screen::buy(b, not_today, day, 2026, 10, 17).error == screen::BuyError::NotToday);
    Bond poor = family(5);
    screen::daily_offer(poor, day, 2026, 10, 17, offer);
    r = screen::buy(poor, offer[0], day, 2026, 10, 17);
    CHECK(r.error == screen::BuyError::Coins);
    CHECK(r.need == screen::shop_item(offer[0]).price - 5);
    CHECK(poor.coins == 5);
    Bond shy;
    shy.coins = 500;
    CHECK(screen::buy(shy, offer[0], day, 2026, 10, 17).error == screen::BuyError::Closed);
}

TEST_CASE("shop: the squirrel dresses itself") {
    Bond b = family(5000);
    int day = day_of(2026, 10, 17);
    CHECK(screen::look_for(b, day, 0, 2026, 10, 17).hat == -1);  // nothing owned
    // buy two things a day for two weeks
    for (int d = 0; d < 14; ++d) {
        int offer[6];
        const int n = screen::daily_offer(b, day + d, 2026, 10, 17 + d, offer);
        for (int i = 0; i < std::min(n, 2); ++i) screen::buy(b, offer[i], day + d, 2026, 10, 17 + d);
    }
    const int last = b.bought_day;  // the day of the latest purchase
    const int last_d = 17 + (last - day);
    const auto today = screen::look_for(b, last, 0, 2026, 10, last_d);
    CHECK(today.hat >= 0);
    CHECK(screen::look_for(b, last, 0, 2026, 10, last_d).hat == today.hat);  // the same pick all morning
    if (screen::shop_item(b.bought).shelf == Shelf::Hat) CHECK(today.hat == b.bought);  // a new hat is shown off on its day
    if (screen::shop_item(b.bought).shelf == Shelf::Place) CHECK(today.place == b.bought);
    if (screen::shop_item(b.bought).shelf == Shelf::Friend) CHECK(today.buddy == b.bought);
    std::set<int> hats, places;
    for (int d = 1; d < 40; ++d)
        for (int half = 0; half < 2; ++half) {
            const auto l = screen::look_for(b, last + d, half, 2026, 11, 20);
            hats.insert(l.hat);
            places.insert(l.place);
            if (l.hat >= 0) {
                CHECK(screen::owns(b, l.hat));
                CHECK(screen::shop_item(l.hat).season == Season::Any);  // no pumpkin hat in late November
            }
        }
    CHECK(hats.size() >= 4);    // variety
    CHECK(places.size() >= 2);
    const int pumpkin = screen::shop_find("pumpkin");
    if (screen::owns(b, pumpkin)) CHECK(screen::look_for(b, day_of(2027, 10, 30), 0, 2027, 10, 30).hat == pumpkin);  // every Halloween
}

TEST_CASE("coins: a trickle first, more with five hearts") {
    Bond b;
    auto r = screen::interact(b, 100, screen::Action::Feed);
    CHECK(r.coins == screen::kCoinsTrickle);
    CHECK(screen::interact(b, 100, screen::Action::Pet).coins == 0);  // once a day
    Bond f = family();
    f.last_day = 199; f.streak = 6;
    r = screen::interact(f, 200, screen::Action::Coffee, 0, true);
    CHECK(r.coins == screen::kCoinsDaily + screen::kCoinsWeek + screen::kCoinsSpecial);  // a week in a row, a special day
    CHECK(f.coins == r.coins);
    CHECK(f.coins_total == r.coins);
}

TEST_CASE("coins: saved with the friendship, old saves still load") {
    Bond b = family(77);
    b.owned_lo = 0x80000005u; b.owned_hi = 0x3u; b.bought = 2; b.bought_day = 20000;
    char buf[160];
    screen::bond_save(b, buf, sizeof buf);
    const Bond c = screen::bond_load(buf);
    CHECK(c.coins == 77); CHECK(c.owned_lo == 0x80000005u); CHECK(c.owned_hi == 0x3u); CHECK(c.bought == 2); CHECK(c.bought_day == 20000);
    CHECK(c.score == 85);
    const Bond old = screen::bond_load("b1 44 2 3 5 20000 20000 20000 3 1 11 5 30");
    CHECK(old.score == 44); CHECK(old.coins == 0); CHECK(old.bought == -1); CHECK(old.gifts == 5u);
}

#include <vector>

#include "screen/core/pages.hpp"

TEST_CASE("shop: every item renders where it belongs, and every unboxing") {
    // Runs under ASan/UBSan.
    std::vector<gfx::Rgb565be> with(240 * 240), without(240 * 240);
    for (int i = 0; i < screen::kShopItemCount; ++i) {
        const auto& it = screen::shop_item(i);
        // a date in the item's season
        int y = 2026, m = 10, d = 17;
        switch (it.season) {
            case Season::Christmas: m = 12; d = 15; break;
            case Season::NewYear: m = 12; d = 30; break;
            case Season::Easter: y = 2027; m = 3; d = 25; break;
            case Season::Oktoberfest: m = 9; d = 25; break;
            case Season::Summer: y = 2027; m = 7; d = 15; break;
            default: break;
        }
        screen::Inputs in;
        in.time_valid = true; in.year = y; in.month = m; in.day = d; in.hour = 11; in.weekday = screen::cal::weekday(y, m, d);
        in.sensor_ok = true; in.reading = {21, 45, 1013, 90000, true}; in.quality = {80, sensor::Level::Good};
        in.weather_ok = true; in.weather = {15, true, 2, weather::Sky::PartlyCloudy, 17, 8, 10};
        in.bond.score = 90; in.bond.best = 4;
        const auto page = it.shelf == Shelf::Decor ? screen::Page::Day : screen::Page::Air;
        in.t_ms = 5000;
        screen::render_page({without}, page, in);
        if (i < 32) in.bond.owned_lo |= 1u << i; else in.bond.owned_hi |= 1u << (i - 32);
        in.bond.bought = i; in.bond.bought_day = screen::cal::day_number(y, m, d);
        for (const std::uint32_t t : {5000u, 6234u, 99999u}) { in.t_ms = t; screen::render_page({with}, page, in); }
        in.t_ms = 5000;
        screen::render_page({with}, page, in);
        CHECK_MESSAGE(with != without, it.id);  // it shows
        in.react.active = true; in.react.r.unbox = true; in.react.r.item = i; in.react.r.level = 4;
        for (int s = 0; s <= 10; ++s) {
            in.react.k = static_cast<float>(s) / 10.0f;
            in.t_ms = 1000u + static_cast<std::uint32_t>(s) * 1200u;
            screen::render_page({with}, screen::Page::Air, in);
            screen::render_page({with}, screen::Page::Clock, in);
        }
    }
}
