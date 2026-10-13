// screen: little surprises. Which scene shows stays deterministic (clock, calendar, climate); inside a scene the
// squirrel blinks, glances, twitches and waves at random moments, and now and then a visitor drops by.
// "Random" is a hash of the time slot and a seed (set from the hardware RNG at boot), so it needs no state and the
// host tests can replay it. Pure C++.
#pragma once

#include <cstdint>

#include "screen/core/font.hpp"

namespace screen {

enum class Page : std::uint8_t;
struct Inputs;

namespace fun {

[[nodiscard]] std::uint32_t mix(std::uint32_t x) noexcept;  // integer hash

// Small idle movements of the squirrel; draw_squirrel() applies them where they fit the pose.
struct Quirks {
    bool blink = false;
    float look = 0;  // eyes to the side, -1 (left) .. 1 (right)
    float ear = 0;   // right ear twitch 0..1
    float tail = 0;  // tail flick -1..1
    float hop = 0;   // a little hop 0..1
    float wave = 0;  // waves hello with the right paw 0..1
    float toss = 0;  // throws the acorn up and catches it: progress 0..1 (very good air only)
};
[[nodiscard]] Quirks quirks(std::uint32_t t_ms, std::uint32_t seed) noexcept;

enum class Visitor : std::uint8_t { None, Bird, Butterfly, Ladybug, Moth, ShootingStar };
inline constexpr int kVisitorCount = 6;
constexpr unsigned bit(Visitor v) noexcept { return 1u << static_cast<unsigned>(v); }

struct Visit {
    Visitor who = Visitor::None;
    float k = 0;          // progress through the visit, 0..1
    std::uint32_t r = 0;  // random bits for the path and colours
};
// Roughly every other minute someone comes by for about 12 s, picked from `allowed` (bits of Visitor).
[[nodiscard]] Visit visit(std::uint32_t t_ms, std::uint32_t seed, unsigned allowed) noexcept;
// Who may come by on this page now (birds in dry daylight, moths at the reading lamp, stars on clear nights...).
[[nodiscard]] unsigned allowed_visitors(Page p, const Inputs& in) noexcept;

// ---- drawing the visitors (screen pixels) ----------------------------------------------------------------------
// A bird hops in from the right, along a sill from x0 to x1 at height y (its feet), pecks, and flies off.
void bird_on_sill(const Canvas& cv, float x0, float x1, float y, const Visit& v, std::uint32_t t) noexcept;
// A bird lands on the squirrel's head (feet at hx, hy), chirps, and flies off. s = size.
void bird_on_head(const Canvas& cv, float hx, float hy, float s, const Visit& v, std::uint32_t t) noexcept;
// A butterfly flutters across the box.
void butterfly(const Canvas& cv, float x0, float y0, float x1, float y1, const Visit& v, std::uint32_t t) noexcept;
// A ladybug walks along y from one side of [x0, x1] to the other, stopping now and then.
void ladybug(const Canvas& cv, float x0, float x1, float y, const Visit& v, std::uint32_t t) noexcept;
// A moth circles a lamp at (cx, cy).
void moth(const Canvas& cv, float cx, float cy, const Visit& v, std::uint32_t t) noexcept;
// A shooting star streaks through the box (only for a moment of the visit), drawn only inside the box.
void shooting_star(const Canvas& cv, float x0, float y0, float x1, float y1, const Visit& v) noexcept;

}  // namespace fun
}  // namespace screen
