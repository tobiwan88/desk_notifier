// sensor: BME688 over I2C (Bosch BME68x SensorAPI, forced mode with the gas heater), sampled by its own task.
#pragma once

#include <cstdint>
#include <expected>
#include <string>

#include "engine/core/types.hpp"
#include "sensor/core/airq.hpp"
#include "sensor/core/vocindex.hpp"

namespace sensor {

struct Snapshot {
    bool ok = false;              // at least one good sample and the last one is recent
    Reading reading{};
    Quality quality{};
    int voc_index = 0;            // Sensirion-style VOC index 1..500 (100 = typical for this room), 0 while masked or unavailable
    bool voc_learning = true;     // first ~1.5 h without a restored state
    Restore voc_restored = Restore::None;
    std::uint32_t age_ms = 0;
    int errors = 0;               // consecutive failures since the last good sample
};

[[nodiscard]] std::expected<void, engine::Err> start() noexcept;  // I2C bus + probe + sampling task
Snapshot latest() noexcept;
std::string i2c_scan() noexcept;  // "0x76,0x77" - addresses that answered the last bus scan

}  // namespace sensor
