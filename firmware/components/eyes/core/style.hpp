// eyes: style data, mood presets, patches (plan §3.3). Pure C++.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

#include "gfx/core/gfx.hpp"

namespace eyes {

enum class Mood : std::uint8_t { Neutral, Happy, Angry, Sleepy, Scared, Curious, Asleep, Count };
enum class PupilShape : std::uint8_t { Round, Slit };

struct EyeStyle {
    gfx::Rgb565be bg;
    struct { bool on; std::int16_t w, h, radius; gfx::Rgb565be color; } sclera;
    struct { bool on; std::int16_t r, ring_w; gfx::Rgb565be color, ring; } iris;
    struct { bool on; std::int16_t r; PupilShape shape; std::int16_t slit_w; gfx::Rgb565be color; } pupil;
    struct { bool on; std::int16_t r, dx, dy; gfx::Rgb565be color; } highlight;
    struct { gfx::Rgb565be color; std::int16_t curve; } lid;
    struct { std::int16_t max_x, max_y; } look;
};

using namespace gfx::literals;

inline constexpr EyeStyle kRealistic = {
    .bg = "#000000"_rgb,
    .sclera = {true, 208, 208, 104, "#f2f2f2"_rgb},
    .iris = {true, 52, 4, "#2a7fd4"_rgb, "#0b2f55"_rgb},
    .pupil = {true, 24, PupilShape::Round, 0, "#000000"_rgb},
    .highlight = {true, 10, -18, -20, "#ffffff"_rgb},
    .lid = {"#000000"_rgb, 24},
    .look = {44, 36},
};
inline constexpr EyeStyle kRobo = {
    .bg = "#000000"_rgb,
    .sclera = {true, 150, 170, 36, "#20e0ff"_rgb},
    .iris = {false, 0, 0, 0, 0},
    .pupil = {false, 0, PupilShape::Round, 0, 0},
    .highlight = {false, 0, 0, 0, 0},
    .lid = {"#000000"_rgb, 0},
    .look = {30, 24},
};
inline constexpr std::array<const EyeStyle*, 2> kPresets = {&kRealistic, &kRobo};

// tilt: + = outer corner up. Curious uses per-eye lid/scale (see layout()).
struct MoodShape { std::uint8_t lid_top, lid_bot; std::int8_t tilt; std::uint8_t pupil, scale; };
inline constexpr std::array<MoodShape, std::to_underlying(Mood::Count)> kMoods = {{
    {30, 0, 0, 140, 100},     // Neutral
    {20, 110, 0, 150, 100},   // Happy
    {95, 0, 40, 110, 96},     // Angry
    {140, 30, -10, 130, 100}, // Sleepy
    {0, 0, -20, 80, 108},     // Scared
    {0, 0, 0, 160, 100},      // Curious (right eye overridden in layout)
    {255, 0, 0, 140, 100},    // Asleep
}};

struct StylePatch {
    std::optional<std::uint8_t> preset;  // index into kPresets, applied first
    std::optional<gfx::Rgb565be> bg;
    std::optional<bool> sclera_on;
    std::optional<std::int16_t> sclera_w, sclera_h, sclera_radius;
    std::optional<gfx::Rgb565be> sclera_color;
    std::optional<bool> iris_on;
    std::optional<std::int16_t> iris_r, iris_ring_w;
    std::optional<gfx::Rgb565be> iris_color, iris_ring;
    std::optional<bool> pupil_on;
    std::optional<std::int16_t> pupil_r, pupil_slit_w;
    std::optional<PupilShape> pupil_shape;
    std::optional<gfx::Rgb565be> pupil_color;
    std::optional<bool> hl_on;
    std::optional<std::int16_t> hl_r, hl_dx, hl_dy;
    std::optional<gfx::Rgb565be> hl_color;
    std::optional<gfx::Rgb565be> lid_color;
    std::optional<std::int16_t> lid_curve;
};

namespace detail {
template <class T> constexpr void set(T& dst, const std::optional<T>& v) noexcept { if (v) dst = *v; }
}

constexpr void apply(EyeStyle& s, const StylePatch& p) noexcept {
    using detail::set;
    if (p.preset && *p.preset < kPresets.size()) s = *kPresets[*p.preset];
    set(s.bg, p.bg);
    set(s.sclera.on, p.sclera_on); set(s.sclera.w, p.sclera_w); set(s.sclera.h, p.sclera_h);
    set(s.sclera.radius, p.sclera_radius); set(s.sclera.color, p.sclera_color);
    set(s.iris.on, p.iris_on); set(s.iris.r, p.iris_r); set(s.iris.ring_w, p.iris_ring_w);
    set(s.iris.color, p.iris_color); set(s.iris.ring, p.iris_ring);
    set(s.pupil.on, p.pupil_on); set(s.pupil.r, p.pupil_r); set(s.pupil.slit_w, p.pupil_slit_w);
    set(s.pupil.shape, p.pupil_shape); set(s.pupil.color, p.pupil_color);
    set(s.highlight.on, p.hl_on); set(s.highlight.r, p.hl_r); set(s.highlight.dx, p.hl_dx);
    set(s.highlight.dy, p.hl_dy); set(s.highlight.color, p.hl_color);
    set(s.lid.color, p.lid_color); set(s.lid.curve, p.lid_curve);
}

}  // namespace eyes
