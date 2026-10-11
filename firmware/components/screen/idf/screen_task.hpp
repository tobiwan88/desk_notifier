// screen: the render task. Draws the home view once a second into a PSRAM frame buffer and pushes it.
#pragma once

#include <cstdint>
#include <expected>

#include "engine/core/types.hpp"

namespace screen {

struct Stats {
    std::uint32_t frame_us_avg = 0, frame_us_max = 0;
    std::uint32_t frames = 0;
    bool alert = false;
    bool snoozed = false;
};

[[nodiscard]] std::expected<void, engine::Err> start() noexcept;
void ack() noexcept;   // button or POST /api/ack: snoozes the fresh-air alert
Stats stats() noexcept;

}  // namespace screen
