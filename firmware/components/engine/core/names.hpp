// engine: clip/picture name rule shared by storage and the web API.
#pragma once

#include <string_view>

namespace engine {

inline constexpr std::size_t kMaxBody = 1024;

[[nodiscard]] constexpr bool clip_name_valid(std::string_view n) noexcept {
    if (n.empty() || n.size() > 31) return false;
    for (char c : n)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}

}  // namespace engine
