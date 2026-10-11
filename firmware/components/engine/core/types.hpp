// engine: shared enums (frozen contract, plan §3.2). Pure C++.
#pragma once

#include <cstddef>
#include <cstdint>

namespace engine {

enum class Err : std::uint8_t { Ok, Unauthorized, BadJson, TooLarge, NotFound, BadName, BadClip,
                                NoSpace, Busy, Conflict, LengthRequired, Io, Ota, DisplayStall };

}  // namespace engine
