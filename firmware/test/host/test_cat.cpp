#include <string>
#include <vector>

#include "cat_eyes.h"
#include "doctest.h"
#include "eyes/core/cat.hpp"

using namespace eyes;

TEST_CASE("names resolve") {
    CHECK(cat_eyes_style_from_name("realistic") == CAT_STYLE_REALISTIC);
    CHECK(cat_eyes_style_from_name("line") == CAT_STYLE_LINE_ART);
    CHECK(cat_eyes_style_from_name("nope") < 0);
    CHECK(cat_eyes_iris_from_name("odd") == CAT_IRIS_ODD);
    CHECK(cat_eyes_anim_from_name("wink") == CAT_ANIM_WINK);
    CHECK(cat_eyes_anim_from_name("heart_eyes") == CAT_ANIM_SPECIAL);
}

TEST_CASE("first frame is a full redraw; idle frames are mostly clean") {
    CatModel m; m.start(5, 0, 0, false);
    CatView v(engine::Eye::Left);
    m.tick(1000);
    auto p = v.next(1000);
    CHECK(p.full_redraw);
    CHECK(!p.trim);
    CHECK(p.dirty.size() == 1);
    int clean = 0;
    for (int i = 1; i <= 30; ++i) { m.tick(1000 + i * 33); if (v.next(0).dirty.empty()) ++clean; }
    CHECK(clean > 0);
}

TEST_CASE("every style x animation renders for 3 s without errors, dirty rows stay in range") {
    std::vector<gfx::Rgb565be> buf(240 * 20);
    for (int style = 0; style < CAT_STYLE_COUNT; ++style) {
        for (int anim = 0; anim < CAT_ANIM_COUNT; ++anim) {
            CatModel m; m.start(static_cast<std::uint32_t>(style * 100 + anim + 1), static_cast<std::uint8_t>(style), 5, false);
            CatView views[2] = {CatView(engine::Eye::Left), CatView(engine::Eye::Right)};
            engine::EyeCmd c; c.play = static_cast<std::uint8_t>(anim);
            m.tick(0); m.apply(c, 0);
            for (std::uint32_t t = 33; t < 3000; t += 100) {
                m.tick(t);
                for (auto& v : views) {
                    auto p = v.next(t);
                    for (const auto& r : p.dirty) {
                        CHECK(r.x == 0); CHECK(r.w == 240); CHECK(r.y >= 0); CHECK(r.y + r.h <= 240);
                        for (int y = r.y; y < r.y + r.h; y += 20) {
                            gfx::Rect s{0, static_cast<std::int16_t>(y), 240, static_cast<std::int16_t>(std::min(20, r.y + r.h - y))};
                            CHECK(static_cast<bool>(v.fill(s, buf)));
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("commands: style, iris, auto, random, look, blink") {
    CatModel m; m.start(9, 0, 0, true);
    engine::EyeCmd c; c.style = CAT_STYLE_ANIME; c.iris = CAT_IRIS_GREEN; c.auto_idle = false; c.save = true;
    m.apply(c, 0);
    auto i = CatModel::info();
    CHECK(i.style == CAT_STYLE_ANIME); CHECK(i.iris == CAT_IRIS_GREEN); CHECK(!i.auto_idle); CHECK(i.style_name == "anime");
    CHECK(m.save_pending()); m.saved(); CHECK(!m.save_pending());
    engine::EyeCmd r; r.play = engine::kPlayRandom; m.tick(0); m.apply(r, 0); m.tick(33);
    CHECK(CatModel::info().anim != "idle");
    engine::EyeCmd l; l.look_q8 = std::array<std::int16_t, 2>{128, -128}; l.hold_ms = 500; m.apply(l, 40);
    engine::EyeCmd b; b.play = engine::kPlayBlink; m.apply(b, 50);
}

TEST_CASE("invalidate forces the whole eye") {
    CatModel m; m.start(2, 0, 0, false);
    CatView v(engine::Eye::Right);
    m.tick(0); (void)v.next(0);
    m.tick(33); (void)v.next(33);
    v.invalidate(); m.tick(66);
    auto p = v.next(66);
    CHECK(p.full_redraw);
}

TEST_CASE("catalogue matches the component") {
    for (std::size_t i = 0; i < kStyleNames.size(); ++i) CHECK(cat_eyes_style_from_name(std::string(kStyleNames[i]).c_str()) == static_cast<int>(i));
    for (std::size_t i = 0; i < kIrisNames.size(); ++i) CHECK(cat_eyes_iris_from_name(std::string(kIrisNames[i]).c_str()) == static_cast<int>(i));
    for (std::size_t i = 0; i < kAnimNames.size(); ++i) CHECK(cat_eyes_anim_from_name(std::string(kAnimNames[i]).c_str()) == static_cast<int>(i));
    CHECK(kStyleNames.size() == CAT_STYLE_COUNT); CHECK(kIrisNames.size() == CAT_IRIS_COUNT); CHECK(kAnimNames.size() == CAT_ANIM_COUNT);
    for (std::size_t i = 0; i < kSpecialNames.size(); ++i)
        if (!kSpecialNames[i].empty()) CHECK(cat_eyes_anim_from_name(std::string(kSpecialNames[i]).c_str()) == CAT_ANIM_SPECIAL);
}
