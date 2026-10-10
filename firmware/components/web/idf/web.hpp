#pragma once

#include <expected>

#include "engine/core/types.hpp"

namespace web {
[[nodiscard]] std::expected<void, engine::Err> start() noexcept;  // plan §5.3
}
