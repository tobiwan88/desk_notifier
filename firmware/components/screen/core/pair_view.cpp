#include "screen/core/pair_view.hpp"

#include "screen/core/draw.hpp"

namespace screen {

void render_pair_code(const Canvas& cv, std::string_view code) noexcept {
    draw::dim(cv, 50);
    draw::label(cv, 62, 3, draw::hex(0xFFFFFF), "PAIR CODE");
    draw::label(cv, 102, 5, draw::hex(0xFFD54A), code);
    draw::label(cv, 160, 2, draw::hex(0xBFD7EA), "TYPE IT ON YOUR PHONE");
}

}  // namespace screen
