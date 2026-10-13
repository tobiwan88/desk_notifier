// screen: the air page. A cute squirrel mascot shows the indoor air quality with its mood, in front of the
// outdoor weather, dressed for the outdoor temperature. Pure C++.
#pragma once

#include <cstdint>

#include "screen/core/font.hpp"
#include "screen/core/fun.hpp"
#include "sensor/core/airq.hpp"
#include "weather/core/forecast.hpp"

namespace screen {

// Sleepy: sensor warming up or missing. The other four follow the air-quality levels.
enum class Mood : std::uint8_t { Sleepy, VeryGood, Good, Okay, Bad };

[[nodiscard]] Mood mood_for(sensor::Level l) noexcept;
[[nodiscard]] const char* mood_label(Mood m) noexcept;  // "VERY GOOD", "GOOD", "OKAY", "BAD", ...

// What the squirrel wears for the outdoor weather. Items stack: a knitted hat and scarf below kColdC, an ice
// cream from kHotC on, sunglasses on a sunny day, an umbrella in rain (always when windy), and from kWindyKmh on
// it glides under the umbrella instead of standing.
struct Outfit {
    bool hat = false;
    bool ice_cream = false;
    bool sunglasses = false;
    bool umbrella = false;
    bool windy = false;
    bool paws_free = false;  // no arms, acorn, ice cream or umbrella: the caller draws what the paws hold
};
inline constexpr float kColdC = 8.0f, kHotC = 25.0f, kWindyKmh = 30.0f;
[[nodiscard]] Outfit outfit_for(bool weather_ok, const weather::Forecast& w) noexcept;

struct AirData {
    bool sensor_ok = false;
    sensor::Reading reading{};
    sensor::Quality quality{};
    bool alert = false;      // poor air and not snoozed: the background pulses
    bool weather_ok = false; // the sky behind the squirrel shows the outdoor weather
    weather::Forecast weather{};
    std::uint32_t t_ms = 0;  // monotonic, drives the animation
    fun::Quirks quirks{};    // blinks, glances, waves ...
    fun::Visit visit{};      // a bird on the head, a butterfly, a shooting star
};

// Draws the squirrel. (cx, cy) is the centre of its body, `scale` 1.0 is about 120 px tall.
void draw_squirrel(const Canvas& cv, float cx, float cy, float scale, Mood m, std::uint32_t t_ms, Outfit o = {},
                   const fun::Quirks& q = {}) noexcept;

// Where draw_squirrel moves the body this frame (the bob of the mood animation), in design units (multiply by
// scale), so that props drawn by the caller move with it.
struct Bob { float dx, dy; };
[[nodiscard]] Bob squirrel_bob(Mood m, std::uint32_t t_ms) noexcept;

void render_air(const Canvas& cv, const AirData& d) noexcept;

}  // namespace screen
