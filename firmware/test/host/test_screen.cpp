#include <vector>

#include "doctest.h"
#include "engine/core/errors.hpp"
#include "engine/core/names.hpp"
#include "screen/core/view.hpp"
#include "sensor/core/airq.hpp"

using namespace sensor;

TEST_CASE("clip_name_valid") {
    CHECK(engine::clip_name_valid("a-b_9")); CHECK(!engine::clip_name_valid("")); CHECK(!engine::clip_name_valid("A"));
    CHECK(!engine::clip_name_valid("../x")); CHECK(!engine::clip_name_valid(std::string(32, 'a'))); CHECK(engine::clip_name_valid(std::string(31, 'a')));
}

TEST_CASE("errors and auth helpers") {
    using namespace engine;
    CHECK(describe(Err::Conflict).http == 409);
    CHECK(describe(Err::BadJson).code == "E_BAD_JSON");
    static_assert(ct_equal("abc", "abc") && !ct_equal("abc", "abd") && !ct_equal("abc", "ab"));
    CHECK(bearer_token("Bearer xyz") == "xyz");
    CHECK(bearer_token("Basic xyz").empty());
    CHECK(bearer_token("Bearer ").empty());
}

TEST_CASE("humidity points peak at 40 percent") {
    CHECK(humidity_points(40) == 25);
    CHECK(humidity_points(0) == 0);
    CHECK(humidity_points(100) == 0);
    CHECK(humidity_points(70) == 13);
}

TEST_CASE("tracker warms up, then scores gas against the baseline") {
    Tracker t;
    Reading r{22, 40, 1013, 100000, true};
    Quality q{};
    for (int i = 0; i < Tracker::kWarmupSamples - 1; ++i) q = t.update(r);
    CHECK(q.level == Level::WarmingUp);
    q = t.update(r);
    CHECK(q.level == Level::Good);
    CHECK(q.score == 100);
    r.gas_ohm = 30000;  // gas resistance collapses: stale / bad air
    q = t.update(r);
    CHECK(q.score < 70);
    r.gas_ohm = 5000;
    for (int i = 0; i < 5; ++i) q = t.update(r);
    CHECK(q.level == Level::Poor);
}

TEST_CASE("invalid gas readings do not move the tracker") {
    Tracker t;
    Reading r{22, 40, 1013, 100000, false};
    CHECK(t.update(r).level == Level::WarmingUp);
    CHECK(t.samples() == 0);
}

TEST_CASE("level thresholds") {
    CHECK(level_for(100) == Level::Good); CHECK(level_for(70) == Level::Good);
    CHECK(level_for(69) == Level::Fair); CHECK(level_for(50) == Level::Fair);
    CHECK(level_for(49) == Level::Stale); CHECK(level_for(35) == Level::Stale);
    CHECK(level_for(34) == Level::Poor);
}

TEST_CASE("home view draws text and differs between alert and normal") {
    std::vector<gfx::Rgb565be> a(240 * 240), b(240 * 240);
    screen::HomeData d;
    d.time_valid = true; d.hour = 9; d.minute = 41; d.second = 2;
    d.sensor_ok = true; d.reading = {23.4f, 45, 1012, 90000, true}; d.quality = {82, Level::Good};
    screen::render_home({a}, d);
    d.alert = true; d.quality = {20, Level::Poor};
    screen::render_home({b}, d);
    CHECK(a != b);
    int white = 0;
    for (auto px : a) white += (px == gfx::rgb565_rt(255, 255, 255));
    CHECK(white > 500);  // time + temperature glyph pixels
}

TEST_CASE("text width and hue wheel") {
    CHECK(screen::text_width("AB", 2) == 22);
    CHECK(screen::text_width("", 3) == 0);
    CHECK(screen::hue_color(0, 255) == screen::hue_color(1536, 255));
}
