// engine: shared enums (frozen contract, plan §3.2). Pure C++.
#pragma once

#include <cstddef>
#include <cstdint>

namespace engine {

enum class Err : std::uint8_t { Ok, Unauthorized, BadJson, TooLarge, NotFound, BadName, BadClip,
                                NoSpace, Busy, Conflict, LengthRequired, Io, Ota, DisplayStall };
enum class Eye : std::uint8_t { Left = 0, Right = 1 };
inline constexpr std::size_t kEyes = 2;

}  // namespace engine
