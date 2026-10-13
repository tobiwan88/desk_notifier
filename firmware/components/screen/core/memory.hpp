// screen: the little things the desk remembers between frames and reboots: how good the air was day after day
// (the window-sill plant grows or wilts) and when it last rained (snails, rainbows). Pure C++; the render task
// feeds them and saves the plant to NVS.
#pragma once

#include <cstddef>
#include <cstdint>

#include "sensor/core/airq.hpp"

namespace screen {

// The window-sill plant. Each day with mostly good air (60 % of the measured minutes VERY GOOD or GOOD) gives a new
// leaf (up to 9) and perks it up; a day with a lot of stale air (30 % OKAY or BAD) makes it droop, and at the worst
// droop it loses a leaf. Thirty good days in a row and it flowers.
struct Plant {
    int leaves = 1, droop = 0, streak = 0;
    bool flower = false, grew = false;  // grew: a new leaf today
    int day = 0;                         // YYYYMMDD of the day being counted
    int good = 0, bad = 0, total = 0;    // minutes of that day
};
inline constexpr int kPlantMaxLeaves = 9, kPlantMaxDroop = 3, kPlantBloomDays = 30, kPlantMinMinutes = 60;

// Call once a minute with today's date (YYYYMMDD). Returns true when a day was closed (a good moment to save).
bool plant_minute(Plant& p, int yyyymmdd, bool valid, sensor::Level level) noexcept;
// "p1 ..." text for NVS, and back. A broken or unknown text gives a fresh plant.
std::size_t plant_save(const Plant& p, char* buf, std::size_t n) noexcept;
[[nodiscard]] Plant plant_load(const char* s) noexcept;

// Remembers when it last rained (the current weather, not the day's forecast).
class RainWatch {
public:
    void update(std::uint32_t now_ms, bool weather_ok, bool wet_now) noexcept;
    // Minutes since the rain stopped, or -1 while it rains or when it has not rained for 3 hours.
    [[nodiscard]] int since_min(std::uint32_t now_ms) const noexcept;

private:
    bool seen_ = false, raining_ = false;
    std::uint32_t last_wet_ = 0;
};
inline constexpr int kRainMemoryMin = 180;

}  // namespace screen
