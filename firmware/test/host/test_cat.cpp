#include <iterator>
#include <string>
#include <vector>

#include "cat_eyes.h"
#include "doctest.h"
#include "eyes/core/cat.hpp"
extern "C" {
#include "cat_eyes_internal.h"  // g_ce: tells the test which eye currently carries effect shapes
}

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

TEST_CASE("effects are real animations: named, finite, and they end on their own") {
    static constexpr const char* kFx[] = {"food", "fly", "paper_plane", "bubble", "spiral", "slots", "euro", "fish", "rain", "snow",
                                          "leaf", "day_night", "paw", "spider", "loading", "pacman", "matrix", "disco", "pumpkin"};
    static_assert(std::size(kFx) == CAT_ANIM_FX_LAST - CAT_ANIM_FX_FIRST + 1);
    for (std::size_t i = 0; i < std::size(kFx); ++i) {
        const int id = cat_eyes_anim_from_name(kFx[i]);
        CHECK(id == CAT_ANIM_FX_FIRST + static_cast<int>(i));
        CatModel m; m.start(static_cast<std::uint32_t>(i + 11), CAT_STYLE_REALISTIC, 0, false);
        m.tick(0);
        engine::EyeCmd c; c.play = static_cast<std::uint8_t>(id); m.apply(c, 0);
        m.tick(33);
        CHECK(cat_eyes_current_anim() == id);
        CHECK(CatModel::info().anim == kFx[i]);
        std::uint32_t t = 33;
        while (cat_eyes_current_anim() != CAT_ANIM_NONE && t < 6000) { t += 33; m.tick(t); }
        CHECK(t > 1500);   // a set piece, not a blink
        CHECK(t < 5000);   // ...but it must end
    }
}

// While an eye shows effect shapes or a tint the pushed rows must cover every pixel that changed: the panel model is
// the previous full frame plus only the pushed strips, and has to equal the new full frame exactly (shapes that
// moved, vanished or were clipped, whole-eye tints switching on and off). Everywhere else the component deliberately
// skips sub-pixel drift (a 1 px iris shift below the hash quantum), so only those frames are held to exact equality.
TEST_CASE("effects: pushed rows reproduce the full render exactly") {
    constexpr int kW = 240, kLines = 10;
    std::vector<gfx::Rgb565be> panel(kW * kW), full(kW * kW);
    long bad_pixels = 0, fx_frames = 0;
    // realistic: both eyes; halloween (its own moon/bats layer underneath): left eye only. Coarse 100 ms steps keep the gate fast.
    for (int style : {CAT_STYLE_REALISTIC, CAT_STYLE_HALLOWEEN})
        for (int id = CAT_ANIM_FX_FIRST; id <= CAT_ANIM_FX_LAST; ++id)
            for (int e = 0; e < (style == CAT_STYLE_REALISTIC ? 2 : 1); ++e) {
                CatModel m; m.start(static_cast<std::uint32_t>(id * 31 + style), static_cast<std::uint8_t>(style), 1, false);
                CatView v(e == 0 ? engine::Eye::Left : engine::Eye::Right);
                m.tick(0); (void)v.next(0);
                CHECK(static_cast<bool>(v.fill(gfx::Rect{0, 0, kW, kW}, panel)));
                engine::EyeCmd c; c.play = static_cast<std::uint8_t>(id); m.apply(c, 0);
                bool prev_fx = false;
                for (std::uint32_t t = 100; t < 4800; t += 100) {
                    m.tick(t);
                    const bool fx_now = g_ce.f[e].n_fx > 0 || g_ce.f[e].tint_shift != 0;
                    const auto p = v.next(t);
                    for (const auto& r : p.dirty)
                        for (const gfx::Rect s : gfx::Strips(r, kLines, p.trim))
                            CHECK(static_cast<bool>(v.fill(s, std::span(panel).subspan(static_cast<std::size_t>(s.y * kW), static_cast<std::size_t>(s.area())))));
                    CHECK(static_cast<bool>(v.fill(gfx::Rect{0, 0, kW, kW}, full)));
                    if (fx_now || prev_fx) {
                        ++fx_frames;
                        for (int i = 0; i < kW * kW; ++i) bad_pixels += panel[static_cast<std::size_t>(i)] != full[static_cast<std::size_t>(i)];
                    }
                    panel = full;
                    prev_fx = fx_now;
                }
            }
    CHECK(fx_frames > 1000);
    CHECK(bad_pixels == 0);
}

// Left-eye glitch investigation: the eye-0 path (dirty rows -> 10-line strips as the render task pushes them)
// must behave exactly like eye 1. A panel model per eye is updated only with the pushed strips and compared
// against a full render every frame: rows inside the pushed range must match exactly (no row offset or
// misplaced strip), and what stays stale outside it (pupil drift below the hash quantum) is small for both eyes.
TEST_CASE("dirty strips reproduce the full render equally for both eyes") {
    constexpr int kW = 240, kLines = 10;
    std::vector<gfx::Rgb565be> panel[2] = {std::vector<gfx::Rgb565be>(kW * kW), std::vector<gfx::Rgb565be>(kW * kW)};
    std::vector<gfx::Rgb565be> full(kW * kW);
    CatModel m; m.start(77, CAT_STYLE_REALISTIC, 0, true);
    CatView views[2] = {CatView(engine::Eye::Left), CatView(engine::Eye::Right)};
    long stale[2] = {0, 0}, wrong_in_pushed[2] = {0, 0}, pushes[2] = {0, 0};
    std::uint32_t t = 1000;
    for (int f = 0; f < 600; ++f, t += 33 + static_cast<std::uint32_t>((f * 7) % 23)) {
        if (f % 60 == 30) { engine::EyeCmd c; c.play = static_cast<std::uint8_t>((f / 60) % (CAT_ANIM_COUNT - 1)); m.apply(c, t); }
        m.tick(t);
        for (int e = 0; e < 2; ++e) {
            const auto p = views[e].next(t);
            std::vector<bool> pushed(kW, false);
            for (const auto& r : p.dirty) {
                ++pushes[e];
                for (const gfx::Rect s : gfx::Strips(r, kLines, p.trim)) {
                    CHECK(s.x == 0); CHECK(s.w == kW);
                    const auto dst = std::span(panel[e]).subspan(static_cast<std::size_t>(s.y * kW), static_cast<std::size_t>(s.area()));
                    CHECK(static_cast<bool>(views[e].fill(s, dst)));
                    for (int y = s.y; y < s.y + s.h; ++y) pushed[static_cast<std::size_t>(y)] = true;
                }
            }
            CHECK(static_cast<bool>(views[e].fill(gfx::Rect{0, 0, kW, kW}, full)));
            for (int i = 0; i < kW * kW; ++i) {
                if (panel[e][static_cast<std::size_t>(i)] == full[static_cast<std::size_t>(i)]) continue;
                ++(pushed[static_cast<std::size_t>(i / kW)] ? wrong_in_pushed[e] : stale[e]);
            }
            panel[e] = full;  // resync so each frame is judged on its own
        }
    }
    CHECK(wrong_in_pushed[0] == 0);
    CHECK(wrong_in_pushed[1] == 0);
    CHECK(pushes[0] > 100);
    CHECK(stale[0] < 600 * 40);
    CHECK(stale[1] < 600 * 40);
}
