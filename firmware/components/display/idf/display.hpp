// display: two GC9A01 panels on one SPI bus (plan §3.4).
#pragma once

#include <cstdint>
#include <expected>

#include "engine/core/types.hpp"
#include "gfx/core/gfx.hpp"

namespace display {

[[nodiscard]] std::expected<void, engine::Err> init() noexcept;  // bus, 2 panel IOs, one reset pulse, panels blanked, then DISPON
void set_backlight(std::uint8_t pct) noexcept;                   // no-op unless cfg::kHasBacklightPwm
[[nodiscard]] std::expected<void, engine::Err> fill(engine::Eye, gfx::Rgb565be) noexcept;

}  // namespace display
