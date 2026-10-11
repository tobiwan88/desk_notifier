// screen: the home view (clock, temperature, humidity, air quality) on a slowly drifting background. Pure C++.
#pragma once

#include <cstdint>

#include "screen/core/font.hpp"
#include "sensor/core/airq.hpp"

namespace screen {

struct HomeData {
    bool time_valid = false;
    int hour = 0, minute = 0, second = 0;
    bool sensor_ok = false;
    sensor::Reading reading{};
    sensor::Quality quality{};
    bool alert = false;        // show the "fresh air" alert styling (set by the caller, honours snooze)
    std::uint32_t t_ms = 0;    // monotonic, drives the background drift
};

// Colour at hue h (0..1535, one wheel) with value v (0..255).
[[nodiscard]] gfx::Rgb565be hue_color(int h, int v) noexcept;

void render_home(const Canvas& cv, const HomeData& d) noexcept;

}  // namespace screen
