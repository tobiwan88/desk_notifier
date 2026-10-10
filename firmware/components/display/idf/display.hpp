// display: two GC9A01 panels on one SPI bus with a shared DMA strip pool (plan §3.4).
// Only the render task may call acquire/push/wait_idle/fill.
#pragma once

#include <cstdint>
#include <expected>
#include <span>

#include "engine/core/types.hpp"
#include "gfx/core/gfx.hpp"

namespace display {

struct StripBuf {
    std::span<gfx::Rgb565be> px;
    std::uint8_t idx;
};

[[nodiscard]] std::expected<void, engine::Err> init() noexcept;  // bus, 2 panel IOs, one reset pulse, blank, then DISPON
void set_backlight(std::uint8_t pct) noexcept;                   // no-op unless cfg::kHasBacklightPwm
[[nodiscard]] std::expected<StripBuf, engine::Err> acquire(std::uint32_t timeout_ms) noexcept;
[[nodiscard]] std::expected<void, engine::Err> push(engine::Eye, StripBuf&&, gfx::Rect) noexcept;
[[nodiscard]] std::expected<void, engine::Err> wait_idle(std::uint32_t timeout_ms) noexcept;  // all strips returned
[[nodiscard]] std::expected<void, engine::Err> fill(engine::Eye, gfx::Rgb565be) noexcept;

// Diagnostics (render task only; the console posts requests through engine::lcd_request).
// swap: Eye::Left is drawn on the CS_R panel and vice versa (does a glitch follow the panel or the content?).
void set_swap(bool) noexcept;
bool swapped() noexcept;
// Tear both panels down and bring them up again (reset + init), optionally at another SPI clock (0 = keep).
[[nodiscard]] std::expected<void, engine::Err> reinit(int spi_hz) noexcept;
int spi_hz() noexcept;
// Full-screen row-address test pattern (10-row bands, coloured marker rows every 40 rows, red top / green bottom row).
[[nodiscard]] std::expected<void, engine::Err> pattern(engine::Eye) noexcept;

}  // namespace display
