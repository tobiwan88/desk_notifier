// screen: the Bluetooth pairing code, drawn over whatever page is showing. Pure C++.
#pragma once

#include <string_view>

#include "screen/core/font.hpp"

namespace screen {

// Dims the page and writes the 6-digit passkey the phone asks for.
void render_pair_code(const Canvas& cv, std::string_view code) noexcept;

}  // namespace screen
