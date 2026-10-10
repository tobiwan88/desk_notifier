// engine: render task, command queue, stats (plan §3.6). ESP-IDF only.
#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <string_view>

#include "engine/core/command.hpp"
#include "eyes/core/cat.hpp"

namespace engine {

struct Stats {
    std::uint16_t fps_x10;
    std::uint32_t frame_us_avg, frame_us_max, fill_us_avg;
    std::array<std::string_view, kEyes> source;
};

[[nodiscard]] std::expected<void, Err> start_render_task() noexcept;
[[nodiscard]] std::expected<void, Err> post(const Command&, std::uint32_t timeout_ms) noexcept;
Stats stats() noexcept;
unsigned render_stack_free_bytes() noexcept;  // minimum free stack of the render task seen so far
eyes::CatInfo eye_info() noexcept;  // style / iris / auto / running animation (names are static strings)

// Display diagnostics, executed by the render task between frames (result is logged):
//   Swap  arg 0/1: draw Eye::Left on the CS_R panel and vice versa
//   Init  arg = SPI Hz (0 = keep): reset + init both panels again
//   Test  arg = seconds (0 = stop): push a row-address test pattern to both panels every frame
enum class LcdOp : std::uint8_t { Swap, Init, Test };
[[nodiscard]] bool lcd_request(LcdOp, int arg) noexcept;

}  // namespace engine
