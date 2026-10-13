// screen: animated weather sky (sun, moon, clouds, rain, snow, fog, thunder), shared by the weather page and
// the background of the air page. Pure C++.
#pragma once

#include <cstdint>

#include "screen/core/font.hpp"
#include "weather/core/forecast.hpp"

namespace screen::sky {

struct Palette { std::uint32_t top, bottom; };  // 0xRRGGBB gradient

// Sky gradient for the weather, including the lightning flash of a thunderstorm at time t.
[[nodiscard]] Palette colors(weather::Sky s, bool day, std::uint32_t t) noexcept;
[[nodiscard]] bool lightning(weather::Sky s, std::uint32_t t) noexcept;

// Fills the gradient `pal`, then draws the weather picture centred at (cx, cy) at `scale` (1.0 = weather page).
// Rain and snow fall over the whole screen.
// With `precip` false the rain and snow are left out, to draw them later with precipitation() (e.g. in front
// of a landscape).
void draw(const Canvas& cv, weather::Sky s, bool day, std::uint32_t t, float cx, float cy, float scale,
          const Palette& pal, bool precip = true, float moon_phase = -1) noexcept;  // moon_phase 0..1, -1 = crescent
// Rain, snow, or drifting mist for fog, over the whole screen.
void precipitation(const Canvas& cv, weather::Sky s, std::uint32_t t) noexcept;
// How gloomy the light is under this sky, 0 (sunny) .. 256, for darkening a landscape.
[[nodiscard]] int gloom(weather::Sky s) noexcept;

}  // namespace screen::sky
