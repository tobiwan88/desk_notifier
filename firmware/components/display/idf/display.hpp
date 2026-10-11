// display: one GC9A01 panel with a DMA strip pool.
// Only the screen task may call acquire/push/wait_idle/fill.
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

[[nodiscard]] std::expected<void, engine::Err> init() noexcept;  // bus, panel IO, reset pulse, blank, then DISPON
void set_backlight(std::uint8_t pct) noexcept;                   // no-op unless cfg::kHasBacklightPwm
[[nodiscard]] std::expected<StripBuf, engine::Err> acquire(std::uint32_t timeout_ms) noexcept;
[[nodiscard]] std::expected<void, engine::Err> push(StripBuf&&, gfx::Rect) noexcept;
[[nodiscard]] std::expected<void, engine::Err> wait_idle(std::uint32_t timeout_ms) noexcept;  // all strips returned
[[nodiscard]] std::expected<void, engine::Err> fill(gfx::Rgb565be) noexcept;
// Tear the panel down and bring it up again, optionally at another SPI clock (0 = keep).
[[nodiscard]] std::expected<void, engine::Err> reinit(int spi_hz) noexcept;
int spi_hz() noexcept;
// Pushes a full 240x240 frame (row-major, big-endian RGB565) in strips.
[[nodiscard]] std::expected<void, engine::Err> blit(std::span<const gfx::Rgb565be> frame) noexcept;

}  // namespace display
