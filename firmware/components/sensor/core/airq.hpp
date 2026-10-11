// sensor: air-quality score from BME688 readings (temperature, humidity, gas resistance). Pure C++.
// This is a simple gas-baseline score, not Bosch BSEC IAQ: the baseline is the highest gas resistance seen
// (slowly decaying), so it needs a few minutes of warm-up and a clean-air period to be meaningful.
#pragma once

#include <cstdint>

namespace sensor {

struct Reading {
    float temp_c = 0, rh = 0, hpa = 0, gas_ohm = 0;
    bool gas_valid = false;  // heater stable and gas measurement valid
};

enum class Level : std::uint8_t { WarmingUp, Good, Fair, Stale, Poor };

struct Quality {
    int score = 0;  // 0 (bad) .. 100 (good)
    Level level = Level::WarmingUp;
};

[[nodiscard]] constexpr const char* level_name(Level l) noexcept {
    switch (l) {
        case Level::WarmingUp: return "WARMING UP";
        case Level::Good: return "AIR GOOD";
        case Level::Fair: return "AIR OK";
        case Level::Stale: return "AIR STALE";
        case Level::Poor: return "FRESH AIR!";
    }
    return "";
}

class Tracker {
public:
    static constexpr int kWarmupSamples = 60;  // ~3 min at one sample per 3 s

    Quality update(const Reading& r) noexcept;
    float baseline_ohm() const noexcept { return baseline_; }
    int samples() const noexcept { return n_; }

private:
    float baseline_ = 0;
    int n_ = 0;
};

// 25 points for humidity (ideal 40 % RH), 75 points for gas resistance relative to the baseline.
[[nodiscard]] int humidity_points(float rh) noexcept;
[[nodiscard]] Level level_for(int score) noexcept;

}  // namespace sensor
