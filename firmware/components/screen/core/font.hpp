// screen: 5x7 bitmap font and text drawing on an RGB565-BE canvas. Pure C++.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "gfx/core/gfx.hpp"

namespace screen {

struct Canvas {
    std::span<gfx::Rgb565be> px;  // kScreen x kScreen, row-major
    void set(int x, int y, gfx::Rgb565be c) const noexcept {
        if (x >= 0 && y >= 0 && x < gfx::kScreen && y < gfx::kScreen) px[static_cast<std::size_t>(y) * gfx::kScreen + x] = c;
    }
    void rect(int x, int y, int w, int h, gfx::Rgb565be c) const noexcept {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i) set(x + i, y + j, c);
    }
};

// Glyph rows, bit 4 = leftmost column. '^' is the degree sign, '#' a heart.
constexpr std::array<std::uint8_t, 7> glyph(char c) noexcept {
    switch (c) {
        case '0': return {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110};
        case '1': return {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110};
        case '2': return {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111};
        case '3': return {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110};
        case '4': return {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010};
        case '5': return {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110};
        case '6': return {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110};
        case '7': return {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000};
        case '8': return {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110};
        case '9': return {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100};
        case 'A': return {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001};
        case 'B': return {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110};
        case 'C': return {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110};
        case 'D': return {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110};
        case 'E': return {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111};
        case 'F': return {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000};
        case 'G': return {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111};
        case 'H': return {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001};
        case 'I': return {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110};
        case 'J': return {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100};
        case 'K': return {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001};
        case 'L': return {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111};
        case 'M': return {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001};
        case 'N': return {0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001};
        case 'O': return {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
        case 'P': return {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000};
        case 'Q': return {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101};
        case 'R': return {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001};
        case 'S': return {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110};
        case 'T': return {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100};
        case 'U': return {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
        case 'V': return {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100};
        case 'W': return {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001};
        case 'X': return {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001};
        case 'Y': return {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100};
        case 'Z': return {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111};
        case '.': return {0, 0, 0, 0, 0, 0b01100, 0b01100};
        case ':': return {0, 0b01100, 0b01100, 0, 0b01100, 0b01100, 0};
        case '%': return {0b11001, 0b11010, 0b00010, 0b00100, 0b01000, 0b01011, 0b10011};
        case '-': return {0, 0, 0, 0b11111, 0, 0, 0};
        case '/': return {0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000};
        case '!': return {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0, 0b00100};
        case '\'': return {0b00100, 0b00100, 0b01000, 0, 0, 0, 0};
        case '#': return {0b01010, 0b11111, 0b11111, 0b11111, 0b01110, 0b00100, 0};  // a heart
        case ',': return {0, 0, 0, 0, 0b00110, 0b00100, 0b01000};
        case '?': return {0b01110, 0b10001, 0b00001, 0b00110, 0b00100, 0, 0b00100};
        case '&': return {0b01100, 0b10010, 0b10100, 0b01000, 0b10101, 0b10010, 0b01101};
        case '^': return {0b01100, 0b10010, 0b10010, 0b01100, 0, 0, 0};
        default: return {0, 0, 0, 0, 0, 0, 0};
    }
}

// Advance of one character at `scale` (5 columns + 1 gap).
constexpr int text_width(std::string_view s, int scale) noexcept {
    return s.empty() ? 0 : static_cast<int>(s.size()) * 6 * scale - scale;
}

inline void draw_text(const Canvas& cv, int x, int y, int scale, gfx::Rgb565be color, std::string_view s) noexcept {
    for (char ch : s) {
        const auto g = glyph(ch);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if ((g[static_cast<std::size_t>(row)] >> (4 - col)) & 1) cv.rect(x + col * scale, y + row * scale, scale, scale, color);
        x += 6 * scale;
    }
}

inline void draw_text_centered(const Canvas& cv, int y, int scale, gfx::Rgb565be color, std::string_view s) noexcept {
    draw_text(cv, (gfx::kScreen - text_width(s, scale)) / 2, y, scale, color, s);
}

}  // namespace screen
