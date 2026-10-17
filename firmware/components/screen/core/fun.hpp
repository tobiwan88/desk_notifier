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
    float howl = 0;  // howls at the full moon: mouth open 0..1 (set by the pages on howling nights)
};
[[nodiscard]] Quirks quirks(std::uint32_t t_ms, std::uint32_t seed) noexcept;

enum class Visitor : std::uint8_t {
    None, Bird, Butterfly, Ladybug, Moth, ShootingStar,
    Owl, Hedgehog, Snail, Bee, Leaves, Petals,  // the seasonal ones
    Fox, Friend,                                // friendship level 5 (family): a fox, a squirrel friend on days off
};
inline constexpr int kVisitorCount = 14;
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

// The golden acorn: very rare (about once in a day and a half), 20 s long. Progress 0..1, or -1.
inline constexpr std::uint32_t kGoldenMs = 20000;
// `lucky` (friendship level 4 and up) makes it twice as likely.
[[nodiscard]] float golden(std::uint32_t t_ms, std::uint32_t seed, bool lucky = false) noexcept;
// From friendship level 2 the squirrel waves hello more often: an extra wave 0..1.
[[nodiscard]] float hello(std::uint32_t t_ms, std::uint32_t seed) noexcept;
// At friendship level 5 it now and then has a present for you (on the day page): progress 0..1, or -1.
[[nodiscard]] float present_for_you(std::uint32_t t_ms, std::uint32_t seed) noexcept;
// From five hearts the squirrel now and then finds a coin in the grass (air page): progress 0..1, or -1.
inline constexpr std::uint32_t kCoinFindMs = 6000;
[[nodiscard]] float coin_found(std::uint32_t t_ms, std::uint32_t seed) noexcept;
// On a howling night (full moon, Halloween) the squirrel howls now and then: mouth 0..1, 0 when quiet.
[[nodiscard]] float howl(std::uint32_t t_ms, std::uint32_t seed) noexcept;

// Short gags on the day page. The first four can happen any time they fit; the last four only in their time
// window (after lunch, Friday afternoon, Monday morning, with the window open), and then more often.
enum class Gag : std::uint8_t { None, Plane, Acorn, Pen, Cheeks, NodOff, ClockWatch, Sigh, Stretch };
inline constexpr int kGagCount = 9;
constexpr unsigned bit(Gag g) noexcept { return 1u << static_cast<unsigned>(g); }
inline constexpr unsigned kWindowGags = bit(Gag::NodOff) | bit(Gag::ClockWatch) | bit(Gag::Sigh) | bit(Gag::Stretch);
struct GagNow { Gag gag = Gag::None; float k = 0; std::uint32_t r = 0; };
[[nodiscard]] GagNow gag(std::uint32_t t_ms, std::uint32_t seed, unsigned allowed) noexcept;

// Now and then a squirrel pun replaces the caption for a few seconds. `activity` is screen::Activity as an int.
[[nodiscard]] const char* pun(std::uint32_t t_ms, std::uint32_t seed, int activity) noexcept;

// How strongly a rainbow shows (0..256): in daylight, when the sun is out again within 90 minutes after rain.
[[nodiscard]] int rainbow_alpha(const Inputs& in) noexcept;

// Sound arcs from the mouth (mx, my) and "AWOO" at (tx, ty) while the squirrel howls, k = howl 0..1.
void howl_text(const Canvas& cv, float mx, float my, float tx, float ty, float k, std::uint32_t t) noexcept;

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
