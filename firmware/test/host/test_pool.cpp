#include <set>

#include "doctest.h"
#include "engine/core/parse.hpp"
#include "engine/core/pool.hpp"
#include "engine/core/scene.hpp"

using namespace engine;

namespace {
struct FakeClip {
    static constexpr std::string_view kName = "clip";
    int frames_left = 0;
    static inline int started = 0;
    std::expected<void, Err> start(std::string_view n, bool) noexcept {
        if (n == "missing") return std::unexpected(Err::NotFound);
        frames_left = 2; ++started; return {};
    }
    FramePlan next(std::uint32_t) noexcept { FramePlan p; p.trim = false; if (frames_left-- <= 0) p.finished = true; return p; }
    std::expected<void, Err> fill(gfx::Rect, std::span<gfx::Rgb565be>) noexcept { return {}; }
    void stop() noexcept {}
};
Pool only_clips(std::initializer_list<const char*> names) {
    Pool p; p.anim_mask = 0;
    for (auto n : names) (void)p.clips.push_back(StaticString<31>(n));
    return p;
}
}  // namespace

TEST_CASE("pick honours the mask and never repeats") {
    Pool p; p.anim_mask = (1u << 0) | (1u << 10) | (1u << 9);  // wink, glance, happy
    std::uint32_t rng = 7; int last = -1; std::set<int> seen;
    for (int i = 0; i < 300; ++i) {
        Pick k = pick(p, rng, last, true);
        CHECK(k.kind == Pick::Kind::Anim);
        CHECK(k.index != last);
        CHECK((p.anim_mask >> k.index & 1));
        seen.insert(k.index); last = pick_key(k);
    }
    CHECK(seen.size() == 3);
}

TEST_CASE("pick: special only with a style that has it, empty pool gives None, single item repeats") {
    Pool special_only; special_only.anim_mask = 1u << 11;
    std::uint32_t rng = 3;
    CHECK(pick(special_only, rng, -1, false).kind == Pick::Kind::None);
    CHECK(pick(special_only, rng, -1, true).index == 11);
    CHECK(pick(special_only, rng, 11, true).index == 11);  // nothing else to pick
    Pool none; none.anim_mask = 0;
    CHECK(pick(none, rng, -1, true).kind == Pick::Kind::None);
}

TEST_CASE("pick includes clips with their key") {
    Pool p = only_clips({"fire", "rain"});
    std::uint32_t rng = 11; std::set<int> seen;
    for (int i = 0; i < 50; ++i) { Pick k = pick(p, rng, -1, true); CHECK(k.kind == Pick::Kind::Clip); seen.insert(k.index); }
    CHECK(seen.size() == 2);
}

TEST_CASE("parse_pool: names, clips, interval") {
    auto r = parse_pool(R"({"enabled":["wink","happy","fire","fire","rain"],"interval":[3,8.5]})");
    CHECK(static_cast<bool>(r)); if (!r) return;
    CHECK(r->anim_mask == ((1u << 0) | (1u << 9)));
    CHECK(r->clips.size() == 2);
    CHECK(r->min_ds == 30); CHECK(r->max_ds == 85);
    auto only_iv = parse_pool(R"({"interval":[1,2]})"); CHECK(static_cast<bool>(only_iv)); CHECK(!only_iv->has_enabled);
    auto empty = parse_pool(R"({"enabled":[]})"); CHECK(static_cast<bool>(empty)); CHECK(empty->anim_mask == 0);
}

TEST_CASE("parse_pool rejects bad input") {
    for (const char* bad : {"{}", "[]", "x", R"({"enabled":"wink"})", R"({"enabled":[1]})", R"({"enabled":["BAD NAME"]})",
                            R"({"interval":[5]})", R"({"interval":[0.5,3]})", R"({"interval":[8,3]})", R"({"interval":[1,500]})",
                            R"({"foo":1})"}) {
        auto r = parse_pool(bad);
        CHECK(!r);
    }
}

TEST_CASE("pool JSON round trip") {
    Pool p; p.anim_mask = 0x0421; (void)p.clips.push_back(StaticString<31>("fire")); p.min_ds = 40; p.max_ds = 90;
    auto patch = parse_pool(pool_to_json(p));
    CHECK(static_cast<bool>(patch)); if (!patch) return;
    Pool q; apply(q, *patch);
    CHECK(q == p);
}

TEST_CASE("scene: random mode plays only enabled clips, after the pause; off means off") {
    FakeClip::started = 0;
    Scene<FakeClip> s(5);
    EyeCmd on; on.auto_idle = true; s.apply(Command{on}, 0);
    Pool p = only_clips({"fire"}); p.min_ds = 10; p.max_ds = 10;
    s.set_pool(p);
    for (std::uint32_t t = 0; t < 4000; t += 33) {
        s.tick(t);
        for (int e = 0; e < 2; ++e) (void)s.next(static_cast<Eye>(e), t);
    }
    CHECK(FakeClip::started >= 2);  // started on both displays at least once
    CHECK(Scene<FakeClip>::eye_info().auto_idle);

    FakeClip::started = 0;
    EyeCmd off; off.auto_idle = false; s.apply(Command{off}, 5000);
    for (std::uint32_t t = 5000; t < 12000; t += 33) { s.tick(t); for (int e = 0; e < 2; ++e) (void)s.next(static_cast<Eye>(e), t); }
    CHECK(FakeClip::started == 0);
}

TEST_CASE("scene: play random picks from the pool immediately; missing clip is ignored") {
    FakeClip::started = 0;
    Scene<FakeClip> s(9);
    s.set_pool(only_clips({"fire"}));
    EyeCmd r; r.play = kPlayRandom; s.apply(Command{r}, 0);
    CHECK(s.current(Eye::Left) == "clip");
    Scene<FakeClip> m(9);
    m.set_pool(only_clips({"missing"}));
    m.apply(Command{r}, 0);
    CHECK(m.current(Eye::Left) == "eyes");
}
