// screen: the render task. Draws the rotating pages (CONFIG_DESK_FPS frames per second) into a PSRAM frame buffer and pushes it.
#pragma once

#include <cstdint>
#include <expected>

#include "engine/core/types.hpp"
#include "screen/core/pages.hpp"

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
void show_page(Page p) noexcept;  // fade to this page (it then rotates on as usual)
void next_page() noexcept;
Page page() noexcept;             // the page on screen now

}  // namespace screen
