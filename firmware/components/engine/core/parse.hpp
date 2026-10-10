// engine: JSON -> Command parsing (plan §5.2, S4.1). Uses cJSON.
#pragma once

#include <expected>
#include <string_view>

#include "engine/core/command.hpp"

namespace engine {

struct ParseError { Err code; StaticString<63> message; };

inline constexpr std::size_t kMaxBody = 1024;

[[nodiscard]] std::expected<EyeCmd, ParseError> parse_eye(std::string_view json) noexcept;
[[nodiscard]] std::expected<PlayCmd, ParseError> parse_play(std::string_view json) noexcept;

}  // namespace engine
