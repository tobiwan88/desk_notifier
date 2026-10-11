// screen: the air page. A cute squirrel mascot shows the indoor air quality with its mood, in front of the
// outdoor weather, dressed for the outdoor temperature. Pure C++.
#pragma once

#include <cstdint>

#include "screen/core/font.hpp"
#include "sensor/core/airq.hpp"
#include "weather/core/forecast.hpp"

namespace screen {

// Sleepy: sensor warming up or missing. The other four follow the air-quality levels.
enum class Mood : std::uint8_t { Sleepy, VeryGood, Good, Okay, Bad };

[[nodiscard]] Mood mood_for(sensor::Level l) noexcept;
[[nodiscard]] const char* mood_label(Mood m) noexcept;  // "VERY GOOD", "GOOD", "OKAY", "BAD", ...

// What the squirrel wears for the outdoor temperature: a knitted hat and scarf below kColdC, an ice cream
// from kHotC on.
struct Outfit {
    bool hat = false;
    bool ice_cream = false;
};
inline constexpr float kColdC = 8.0f, kHotC = 25.0f;
[[nodiscard]] Outfit outfit_for(bool weather_ok, float outdoor_c) noexcept;

struct AirData {
    bool sensor_ok = false;
    sensor::Reading reading{};
    sensor::Quality quality{};
    bool alert = false;      // poor air and not snoozed: the background pulses
    bool weather_ok = false; // the sky behind the squirrel shows the outdoor weather
    weather::Forecast weather{};
    std::uint32_t t_ms = 0;  // monotonic, drives the animation
};

// Draws the squirrel. (cx, cy) is the centre of its body, `scale` 1.0 is about 120 px tall.
void draw_squirrel(const Canvas& cv, float cx, float cy, float scale, Mood m, std::uint32_t t_ms, Outfit o = {}) noexcept;

void render_air(const Canvas& cv, const AirData& d) noexcept;

}  // namespace screen
