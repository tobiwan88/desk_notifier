// sensor: BME688 over I2C (Bosch BME68x SensorAPI, forced mode with the gas heater), sampled by its own task.
#pragma once

#include <cstdint>
#include <expected>

#include "engine/core/types.hpp"
#include "sensor/core/airq.hpp"

namespace sensor {

struct Snapshot {
    bool ok = false;              // at least one good sample and the last one is recent
    Reading reading{};
    Quality quality{};
    std::uint32_t age_ms = 0;
    int errors = 0;               // consecutive failures since the last good sample
};

[[nodiscard]] std::expected<void, engine::Err> start() noexcept;  // I2C bus + probe + sampling task
Snapshot latest() noexcept;

}  // namespace sensor
