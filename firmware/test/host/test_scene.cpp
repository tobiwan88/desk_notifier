#include "doctest.h"
#include "engine/core/scene.hpp"

using namespace engine;

struct FakeClip {
    static constexpr std::string_view kName = "clip";
    int frames_left = 0; bool stopped = false;
    std::expected<void, Err> start(std::string_view n, bool) noexcept {
        if (n == "missing") return std::unexpected(Err::NotFound);
        frames_left = 2; stopped = false; return {};
    }
    FramePlan next(std::uint32_t) noexcept {
        FramePlan p; p.trim = false;
        if (frames_left-- <= 0) p.finished = true;
        else (void)p.dirty.push_back(gfx::Rect{0, 0, 10, 10});
        return p;
    }
    std::expected<void, Err> fill(gfx::Rect, std::span<gfx::Rgb565be>) noexcept { return {}; }
    void stop() noexcept { stopped = true; }
};
static_assert(ClipSource<FakeClip>);
static_assert(ClipSource<NoClip>);

static PlayCmd play(std::string_view l, std::string_view r, bool then = true) {
    PlayCmd p; p.clip[0].assign(l); p.clip[1].assign(r); p.then_eyes = then; return p;
}

TEST_CASE("play on one display only; other keeps the eyes") {
    Scene<FakeClip> s;
    s.apply(Command{play("wink", "")}, 0);
    CHECK(s.current(Eye::Left) == "clip");
    CHECK(s.current(Eye::Right) == "eyes");
}

TEST_CASE("missing clip leaves the eyes running") {
    Scene<FakeClip> s;
    s.apply(Command{play("missing", "")}, 0);
    CHECK(s.current(Eye::Left) == "eyes");
}

TEST_CASE("clip finishes -> eyes return with a full redraw") {
    Scene<FakeClip> s;
    s.apply(Command{play("a", "a")}, 0);
    s.tick(0);
    CHECK(!s.next(Eye::Left, 0).finished);
    CHECK(!s.next(Eye::Left, 33).finished);
    FramePlan p = s.next(Eye::Left, 66);
    CHECK(s.current(Eye::Left) == "eyes");
    CHECK(p.full_redraw);
}

TEST_CASE("then_eyes=false holds the last frame") {
    Scene<FakeClip> s;
    s.apply(Command{play("a", "", false)}, 0);
    for (int i = 0; i < 4; ++i) (void)s.next(Eye::Left, static_cast<std::uint32_t>(i * 33));
    CHECK(s.current(Eye::Left) == "clip");
}

TEST_CASE("stop and interrupt return to eyes") {
    Scene<FakeClip> s;
    s.apply(Command{play("a", "a")}, 0);
    s.apply(Command{StopCmd{}}, 10);
    CHECK(s.current(Eye::Left) == "eyes"); CHECK(s.current(Eye::Right) == "eyes");
    s.apply(Command{play("a", "")}, 20);
    EyeCmd e; e.interrupt = true; e.mood = eyes::Mood::Happy;
    s.apply(Command{e}, 30);
    CHECK(s.current(Eye::Left) == "eyes");
}

TEST_CASE("eye commands during a clip apply to the shared model") {
    Scene<FakeClip> s;
    s.apply(Command{play("a", "")}, 0);
    EyeCmd e; e.mood = eyes::Mood::Angry; e.transition_ms = 0; e.auto_blink = false; e.auto_look = false;
    s.apply(Command{e}, 5);
    s.tick(10);
    CHECK(s.current(Eye::Left) == "clip");
    CHECK(s.next(Eye::Right, 10).full_redraw);
}
