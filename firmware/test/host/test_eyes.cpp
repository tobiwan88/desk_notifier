#include <cstdio>
#include <vector>

#include "doctest.h"
#include "eyes/core/model.hpp"

using namespace eyes;

static_assert(kMoods.size() == 7);
static_assert(std::is_trivially_copyable_v<engine::Command>);

static std::vector<gfx::Rgb565be> frame(const EyeGeom& g) {
    std::vector<gfx::Rgb565be> f(240 * 240);
    render(g, {0, 0, 240, 240}, f);
    return f;
}

TEST_CASE("neutral realistic eye renders sclera, iris, pupil") {
    State s;
    auto g = layout(kRealistic, s, engine::Eye::Left);
    auto f = frame(g);
    CHECK(f[120 * 240 + 120 + 40] != g.bg);   // sclera near centre (iris r=52 -> x+40 is iris)
    CHECK(f[120 * 240 + 120 - 90] == g.sclera_c);
    CHECK(f[120 * 240 + 120 + 5] == g.pupil_c);
    CHECK(f[2 * 240 + 2] == g.bg);
}

TEST_CASE("closed lid blanks the eye") {
    State s; s.lid_top = 255;
    auto f = frame(layout(kRealistic, s, engine::Eye::Left));
    for (auto px : f) CHECK(px == kRealistic.bg);
}

TEST_CASE("diff: identical geometry has no dirty rects, look change is small") {
    State a, b; b.look_x = 100;
    auto ga = layout(kRealistic, a, engine::Eye::Left), gb = layout(kRealistic, b, engine::Eye::Left);
    CHECK(diff(ga, ga).empty());
    auto d = diff(ga, gb);
    CHECK(d.size() == 1); if (d.size() != 1) return;
    CHECK(d[0].area() < 240 * 240 / 2);
}

TEST_CASE("diff covers every changed pixel (blink step and look)") {
    for (int step = 1; step < 6; ++step) {
        State a, b; b.blink_l = static_cast<std::uint16_t>(step * 40); b.look_x = static_cast<std::int16_t>(step * 30);
        auto ga = layout(kRealistic, a, engine::Eye::Left), gb = layout(kRealistic, b, engine::Eye::Left);
        auto fa = frame(ga), fb = frame(gb);
        auto d = diff(ga, gb);
        for (int y = 0; y < 240; ++y) for (int x = 0; x < 240; ++x) {
            if (fa[y * 240 + x] == fb[y * 240 + x]) continue;
            bool covered = false;
            for (auto r : d) covered |= x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
            CHECK(covered);
            if (!covered) return;
        }
    }
}

TEST_CASE("robo preset and slit pupil render without overrun") {
    State s;
    EyeStyle st = kRobo;
    auto f = frame(layout(st, s, engine::Eye::Right));
    CHECK(f[120 * 240 + 120] == layout(st, s, engine::Eye::Right).sclera_c);
    st = kRealistic; st.pupil.shape = PupilShape::Slit; st.pupil.slit_w = 10;
    (void)frame(layout(st, s, engine::Eye::Left));
}

TEST_CASE("render of sub-rect equals crop of full render") {
    State s; s.look_x = 80; s.blink_r = 100;
    auto g = layout(kRealistic, s, engine::Eye::Right);
    auto full = frame(g);
    gfx::Rect r{50, 70, 77, 33};
    std::vector<gfx::Rgb565be> part(static_cast<std::size_t>(r.area()));
    render(g, r, part);
    for (int y = 0; y < r.h; ++y) for (int x = 0; x < r.w; ++x)
        CHECK(part[y * r.w + x] == full[(r.y + y) * 240 + r.x + x]);
}

TEST_CASE("model: mood transition reaches target, blink returns to open") {
    EyesModel m; m.start(7, kRealistic);
    engine::EyeCmd c; c.mood = Mood::Angry; c.auto_blink = false; c.auto_look = false; c.transition_ms = 200;
    m.apply(c, 1000);
    m.tick(1100); CHECK(m.state().lid_top > 30); CHECK(m.state().lid_top < 95);
    m.tick(1300); CHECK(m.state().lid_top == 95); CHECK(m.state().tilt == 40);
    engine::EyeCmd b; b.blink = engine::EyeSel::Left; m.apply(b, 2000);
    m.tick(2075); CHECK(m.state().blink_l >= 250); CHECK(m.state().blink_r == 0);
    m.tick(2200); CHECK(m.state().blink_l == 0);
}

TEST_CASE("EyeView: first frame full, idle frame empty") {
    EyesModel m; m.start(3, kRealistic);
    engine::EyeCmd c; c.auto_blink = false; c.auto_look = false; m.apply(c, 0);
    EyeView v(m, engine::Eye::Left);
    m.tick(10);
    auto p1 = v.next(10);
    CHECK(p1.full_redraw);
    m.tick(43);
    auto p2 = v.next(43);
    CHECK(p2.dirty.empty());
    CHECK(p2.delay_ms == 100);
}

TEST_CASE("brightness scales colours") {
    CHECK(scale_brightness(gfx::rgb565(0xFFFFFF), 0) == 0);
    CHECK(scale_brightness(gfx::rgb565(0xFFFFFF), 100) == 0xFFFF);
}
