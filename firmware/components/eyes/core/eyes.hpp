// eyes: layout (style+state -> resolved geometry), diff and span renderer (plan §3.3).
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "engine/core/command.hpp"
#include "engine/core/static.hpp"
#include "eyes/core/style.hpp"
#include "gfx/core/gfx.hpp"

namespace eyes {

struct State {
    Mood mood = Mood::Neutral;
    std::int16_t look_x = 0, look_y = 0;            // q8: -256..256
    std::uint16_t blink_l = 0, blink_r = 0;         // 0 open .. 256 closed
    std::uint8_t pupil = 140;                       // 64..255 (140 = style radius)
    std::uint8_t brightness = 100;                  // 0..100
    // per-state resolved (already eased) mood parameters:
    std::uint8_t lid_top = 30, lid_bot = 0;
    std::int8_t tilt = 0;
    std::uint8_t scale_l = 100, scale_r = 100;      // percent
    std::uint8_t lid_top_r = 30;                    // right eye top lid (Curious asymmetry)
};

// Everything the renderer needs for one eye, with brightness already applied to colours.
struct EyeGeom {
    gfx::Rgb565be bg{};
    // sclera: rounded rect
    bool sclera_on{}; std::int16_t scx{}, scy{}, sw{}, sh{}, sr{}; gfx::Rgb565be sclera_c{};
    // iris
    bool iris_on{}; std::int16_t icx{}, icy{}, ir{}, iring{}; gfx::Rgb565be iris_c{}, ring_c{};
    // pupil
    bool pupil_on{}; std::int16_t pr{}, pslit{}; bool slit{}; gfx::Rgb565be pupil_c{};
    // highlight
    bool hl_on{}; std::int16_t hx{}, hy{}, hr{}; gfx::Rgb565be hl_c{};
    // lids: eye box (top..bottom) and edges at the corners, curve (px), tilt (px at outer side)
    gfx::Rgb565be lid_c{}; std::int16_t box_l{}, box_r{}, box_t{}, box_b{};
    std::int16_t top_edge{}, bot_edge{}, curve{}, tilt_px{};
    bool top_closed{}, bot_closed{};
    friend constexpr bool operator==(const EyeGeom&, const EyeGeom&) noexcept = default;
};

[[nodiscard]] EyeGeom layout(const EyeStyle&, const State&, engine::Eye) noexcept;
[[nodiscard]] engine::StaticVec<gfx::Rect, 3> diff(const EyeGeom& a, const EyeGeom& b) noexcept;
void render(const EyeGeom&, gfx::Rect r, std::span<gfx::Rgb565be> out) noexcept;

[[nodiscard]] gfx::Rgb565be scale_brightness(gfx::Rgb565be c, std::uint8_t pct) noexcept;

}  // namespace eyes
