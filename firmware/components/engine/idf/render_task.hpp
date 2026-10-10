// engine: render task, command queue, stats (plan §3.6). ESP-IDF only.
#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <string_view>

#include "engine/core/command.hpp"

namespace engine {

struct Stats {
    std::uint16_t fps_x10;
    std::uint32_t frame_us_avg, frame_us_max;
    std::array<std::string_view, kEyes> source;
};

[[nodiscard]] std::expected<void, Err> start_render_task() noexcept;
[[nodiscard]] std::expected<void, Err> post(const Command&, std::uint32_t timeout_ms) noexcept;
Stats stats() noexcept;

}  // namespace engine
