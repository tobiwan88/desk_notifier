#include "sensor/core/airq.hpp"

#include <algorithm>

namespace sensor {

int humidity_points(float rh) noexcept {
    rh = std::clamp(rh, 0.0f, 100.0f);
    const float p = rh <= 40.0f ? 25.0f * rh / 40.0f : 25.0f * (100.0f - rh) / 60.0f;
    return static_cast<int>(p + 0.5f);
}

Level level_for(int score) noexcept {
    if (score >= 70) return Level::Good;
    if (score >= 50) return Level::Fair;
    if (score >= 35) return Level::Stale;
    return Level::Poor;
}

Quality Tracker::update(const Reading& r) noexcept {
    if (!r.gas_valid || r.gas_ohm <= 0) return {0, Level::WarmingUp};
    if (n_ == 0 || r.gas_ohm > baseline_) baseline_ = r.gas_ohm;
    else baseline_ -= (baseline_ - r.gas_ohm) * 0.002f;  // forget old highs slowly
    if (n_ < 1'000'000) ++n_;
    const float ratio = std::clamp(r.gas_ohm / baseline_, 0.0f, 1.0f);
    const int score = std::clamp(humidity_points(r.rh) + static_cast<int>(75.0f * ratio + 0.5f), 0, 100);
    if (n_ < kWarmupSamples) return {score, Level::WarmingUp};
    return {score, level_for(score)};
}

}  // namespace sensor
