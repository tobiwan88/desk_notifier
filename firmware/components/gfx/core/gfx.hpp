// gfx: geometry, circle trimming, RGB565 helpers, fixed-point and LUTs.
// Pure C++ (no ESP-IDF includes). Frozen contract: plan §3.1.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>

namespace gfx {

inline constexpr int kScreen = 240;

struct Rect {
    std::int16_t x{}, y{}, w{}, h{};
    constexpr bool empty() const noexcept { return w <= 0 || h <= 0; }
    constexpr std::int32_t area() const noexcept { return empty() ? 0 : std::int32_t{w} * h; }
    friend constexpr bool operator==(const Rect&, const Rect&) noexcept = default;
};

constexpr Rect unite(Rect a, Rect b) noexcept {
    if (a.empty()) return b;
    if (b.empty()) return a;
    const int x0 = a.x < b.x ? a.x : b.x;
    const int y0 = a.y < b.y ? a.y : b.y;
    const int x1 = (a.x + a.w) > (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    const int y1 = (a.y + a.h) > (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    return {static_cast<std::int16_t>(x0), static_cast<std::int16_t>(y0),
            static_cast<std::int16_t>(x1 - x0), static_cast<std::int16_t>(y1 - y0)};
}

constexpr Rect intersect(Rect a, Rect b) noexcept {
    const int x0 = a.x > b.x ? a.x : b.x;
    const int y0 = a.y > b.y ? a.y : b.y;
    const int x1 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    const int y1 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    if (x1 <= x0 || y1 <= y0) return {};
    return {static_cast<std::int16_t>(x0), static_cast<std::int16_t>(y0),
            static_cast<std::int16_t>(x1 - x0), static_cast<std::int16_t>(y1 - y0)};
}

constexpr Rect clip_screen(Rect r) noexcept { return intersect(r, Rect{0, 0, kScreen, kScreen}); }

// ---- circle chords --------------------------------------------------------
// Visible area of the round panel. A pixel is inside when its centre lies within
// radius^2 = 14470 (~120.3 px) of the panel centre (119.5, 119.5). Calibrated so that
// 20-line trimmed strips cover exactly 48 880 px (plan §6.3).
inline constexpr std::int32_t kCircleR2x4 = 4 * 14470;  // in half-pixel units

struct Chord { std::int16_t x0, x1; };  // [x0, x1) per row; x0 == x1 == 0 when empty

constexpr std::int32_t isqrt(std::int32_t v) noexcept {
    if (v <= 0) return 0;
    std::int32_t lo = 0, hi = 46341;
    while (lo < hi) {
        const std::int32_t mid = (lo + hi + 1) / 2;
        if (mid * mid <= v) lo = mid; else hi = mid - 1;
    }
    return lo;
}

constexpr std::array<Chord, kScreen> make_chords() noexcept {
    std::array<Chord, kScreen> t{};
    for (int y = 0; y < kScreen; ++y) {
        const std::int32_t dy = 2 * y - (kScreen - 1);
        const std::int32_t rem = kCircleR2x4 - dy * dy;
        if (rem < 0) { t[y] = {0, 0}; continue; }
        // pixel centre offset |2x-239| <= sqrt(rem)
        const std::int32_t s = isqrt(rem);
        int x0 = 0, x1 = 0;
        // smallest x with |2x-239| <= s
        x0 = ((kScreen - 1) - s + 1) / 2;
        if (x0 < 0) x0 = 0;
        x1 = kScreen - x0;
        // exactness fixup (isqrt floors; rem may allow one more pixel only if exact square)
        t[y] = {static_cast<std::int16_t>(x0), static_cast<std::int16_t>(x1)};
    }
    return t;
}
inline constexpr std::array<Chord, kScreen> kChord = make_chords();

// Bounding x-span of the circle over rows [y, y+h).
constexpr Chord span_of_rows(int y, int h) noexcept {
    int x0 = kScreen, x1 = 0;
    for (int r = y; r < y + h; ++r) {
        if (r < 0 || r >= kScreen) continue;
        const Chord c = kChord[r];
        if (c.x1 <= c.x0) continue;
        if (c.x0 < x0) x0 = c.x0;
        if (c.x1 > x1) x1 = c.x1;
    }
    if (x1 <= x0) return {0, 0};
    return {static_cast<std::int16_t>(x0), static_cast<std::int16_t>(x1)};
}

constexpr std::int32_t trimmed_pixels(int strip_lines) noexcept {
    std::int32_t n = 0;
    for (int y = 0; y < kScreen; y += strip_lines) {
        const int h = (y + strip_lines > kScreen) ? kScreen - y : strip_lines;
        const Chord c = span_of_rows(y, h);
        n += std::int32_t{c.x1 - c.x0} * h;
    }
    return n;
}

constexpr std::int32_t circle_pixels() noexcept {
    std::int32_t n = 0;
    for (const Chord c : kChord) n += c.x1 - c.x0;
    return n;
}

// Splits `r` into strips of <= `lines` rows; with `trim`, each strip is narrowed to the
// circle's span over its rows (and strips fully outside are skipped).
class Strips {
public:
    constexpr Strips(Rect r, int lines, bool trim) noexcept : r_(r), lines_(lines > 0 ? lines : 1), trim_(trim) {}

    class It {
    public:
        constexpr It(const Strips* s, int y) noexcept : s_(s), y_(y) { skip(); }
        constexpr Rect operator*() const noexcept { return cur_; }
        constexpr It& operator++() noexcept { y_ += s_->lines_; skip(); return *this; }
        constexpr bool operator==(std::default_sentinel_t) const noexcept { return done_; }
    private:
        constexpr void skip() noexcept {
            done_ = true;
            const int end = s_->r_.y + s_->r_.h;
            while (y_ < end) {
                const int h = (y_ + s_->lines_ > end) ? end - y_ : s_->lines_;
                Rect c{s_->r_.x, static_cast<std::int16_t>(y_), s_->r_.w, static_cast<std::int16_t>(h)};
                if (s_->trim_) {
                    const Chord sp = span_of_rows(y_, h);
                    c = intersect(c, Rect{sp.x0, static_cast<std::int16_t>(y_),
                                          static_cast<std::int16_t>(sp.x1 - sp.x0), static_cast<std::int16_t>(h)});
                }
                if (!c.empty()) { cur_ = c; done_ = false; return; }
                y_ += s_->lines_;
            }
        }
        const Strips* s_;
        int y_;
        Rect cur_{};
        bool done_ = true;
    };
    constexpr It begin() const noexcept { return It(this, r_.y); }
    constexpr std::default_sentinel_t end() const noexcept { return {}; }

private:
    Rect r_;
    int lines_;
    bool trim_;
};

// ---- colour ----------------------------------------------------------------
using Rgb565be = std::uint16_t;  // wire order: high byte first, stored byte-swapped on little-endian

constexpr Rgb565be rgb565_rt(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
    const std::uint16_t v = static_cast<std::uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    return static_cast<Rgb565be>((v << 8) | (v >> 8));
}
consteval Rgb565be rgb565(std::uint32_t rgb888) noexcept {
    return rgb565_rt(static_cast<std::uint8_t>(rgb888 >> 16), static_cast<std::uint8_t>(rgb888 >> 8),
                     static_cast<std::uint8_t>(rgb888));
}

namespace detail {
constexpr std::uint16_t swap16(std::uint16_t v) noexcept { return static_cast<std::uint16_t>((v << 8) | (v >> 8)); }
constexpr int hex_digit(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
}  // namespace detail

namespace literals {
// "#ff8800"_rgb ; malformed literals fail to compile (non-constant throw-free trick: divide by zero).
consteval Rgb565be operator""_rgb(const char* s, std::size_t n) noexcept {
    std::uint32_t v = 0;
    if (n != 7 || s[0] != '#') { int z = 0; return static_cast<Rgb565be>(1 / z); }
    for (std::size_t i = 1; i < 7; ++i) {
        const int d = detail::hex_digit(s[i]);
        if (d < 0) { int z = 0; return static_cast<Rgb565be>(1 / z); }
        v = (v << 4) | static_cast<std::uint32_t>(d);
    }
    return rgb565(v);
}
}  // namespace literals

// alpha 0 -> a, 255 -> b. Operates on logical (non-swapped) 565 internally.
constexpr Rgb565be blend(Rgb565be a, Rgb565be b, std::uint8_t alpha) noexcept {
    const std::uint32_t pa = detail::swap16(a), pb = detail::swap16(b);
    const std::uint32_t ia = 255u - alpha, ib = alpha;
    const std::uint32_t r = (((pa >> 11) & 31) * ia + ((pb >> 11) & 31) * ib + 127) / 255;
    const std::uint32_t g = (((pa >> 5) & 63) * ia + ((pb >> 5) & 63) * ib + 127) / 255;
    const std::uint32_t bl = ((pa & 31) * ia + (pb & 31) * ib + 127) / 255;
    return detail::swap16(static_cast<std::uint16_t>((r << 11) | (g << 5) | bl));
}

// ---- fixed point -----------------------------------------------------------
template <int Frac>
struct Fixed {
    static_assert(Frac > 0 && Frac < 30);
    std::int32_t raw{};
    static constexpr Fixed from_int(std::int32_t i) noexcept { return {i << Frac}; }
    static constexpr Fixed from_raw(std::int32_t r) noexcept { return {r}; }
    constexpr std::int32_t to_int() const noexcept { return raw >> Frac; }
    friend constexpr Fixed operator+(Fixed a, Fixed b) noexcept { return {a.raw + b.raw}; }
    friend constexpr Fixed operator-(Fixed a, Fixed b) noexcept { return {a.raw - b.raw}; }
    friend constexpr Fixed operator*(Fixed a, Fixed b) noexcept {
        return {static_cast<std::int32_t>((std::int64_t{a.raw} * b.raw) >> Frac)};
    }
    friend constexpr bool operator==(Fixed, Fixed) noexcept = default;
    friend constexpr auto operator<=>(Fixed, Fixed) noexcept = default;
};
// t in 0..256 (256 = 1.0)
template <int Frac>
constexpr Fixed<Frac> lerp(Fixed<Frac> a, Fixed<Frac> b, std::uint16_t t) noexcept {
    return {a.raw + static_cast<std::int32_t>((std::int64_t{b.raw - a.raw} * t) >> 8)};
}

// ---- LUTs --------------------------------------------------------------------
// smoothstep 3t^2 - 2t^3 over t = i/256, scaled to 0..255
constexpr std::array<std::uint8_t, 257> make_ease_lut() noexcept {
    std::array<std::uint8_t, 257> t{};
    for (std::int64_t i = 0; i <= 256; ++i) {
        const std::int64_t v = (3 * i * i * 256 - 2 * i * i * i) ;  // scale 256^3
        t[i] = static_cast<std::uint8_t>((v * 255 + (std::int64_t{1} << 23)) >> 24);
    }
    return t;
}
// fractional edge coverage (0..15 sixteenths) -> alpha 0..255
constexpr std::array<std::uint8_t, 16> make_edge_lut() noexcept {
    std::array<std::uint8_t, 16> t{};
    for (int i = 0; i < 16; ++i) t[i] = static_cast<std::uint8_t>((i * 255 + 7) / 15);
    return t;
}
inline constexpr std::array<std::uint8_t, 257> kEaseInOut = make_ease_lut();
inline constexpr std::array<std::uint8_t, 16> kEdgeAlpha = make_edge_lut();

}  // namespace gfx
