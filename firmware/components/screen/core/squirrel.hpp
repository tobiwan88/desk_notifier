// screen: the air page. A cute squirrel mascot shows the indoor air quality with its mood. Pure C++.
#pragma once

#include <cstdint>

#include "screen/core/font.hpp"
#include "sensor/core/airq.hpp"

namespace screen {

// Sleepy: sensor warming up or missing. The other four follow the air-quality levels.
enum class Mood : std::uint8_t { Sleepy, VeryGood, Good, Okay, Bad };

[[nodiscard]] Mood mood_for(sensor::Level l) noexcept;
[[nodiscard]] const char* mood_label(Mood m) noexcept;  // "VERY GOOD", "GOOD", "OKAY", "BAD", ...

struct AirData {
    bool sensor_ok = false;
    sensor::Reading reading{};
    sensor::Quality quality{};
    bool alert = false;      // poor air and not snoozed: the background pulses
    std::uint32_t t_ms = 0;  // monotonic, drives the animation
};

// Draws the squirrel. (cx, cy) is the centre of its body, `scale` 1.0 is about 120 px tall.
void draw_squirrel(const Canvas& cv, float cx, float cy, float scale, Mood m, std::uint32_t t_ms) noexcept;

void render_air(const Canvas& cv, const AirData& d) noexcept;

}  // namespace screen
