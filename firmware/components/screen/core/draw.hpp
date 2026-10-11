// screen: anti-aliased shape drawing on the round canvas. Pure C++.
//
// Every shape is convex and described by a span function: for a row y it returns the [x0, x1) interval the
// shape covers. The rasterizer samples 4 sub-rows per pixel row, fills the fully covered middle directly and
// blends only the edge pixels, so the per-pixel cost stays low on a chip without an FPU. Drawing is clipped
// to the visible circle of the round panel.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "gfx/core/gfx.hpp"
#include "screen/core/font.hpp"

namespace screen::draw {

using gfx::Rgb565be;

// 0xRRGGBB -> panel colour (usable at run time).
constexpr Rgb565be hex(std::uint32_t rgb) noexcept {
    return gfx::rgb565_rt(static_cast<std::uint8_t>(rgb >> 16), static_cast<std::uint8_t>(rgb >> 8),
                          static_cast<std::uint8_t>(rgb));
}

// Linear mix of two 0xRRGGBB colours, t in 0..256.
constexpr std::uint32_t lerp_rgb(std::uint32_t a, std::uint32_t b, int t) noexcept {
    t = std::clamp(t, 0, 256);
    auto ch = [&](int sh) {
        const int ca = static_cast<int>((a >> sh) & 0xFF), cb = static_cast<int>((b >> sh) & 0xFF);
        return static_cast<std::uint32_t>(ca + (cb - ca) * t / 256) << sh;
    };
    return ch(16) | ch(8) | ch(0);
}

// dst -> src by a (0..256). Works on the byte-swapped panel format.
inline Rgb565be mix(Rgb565be dst, Rgb565be src, int a) noexcept {
    if (a >= 256) return src;
    if (a <= 0) return dst;
    const int d = static_cast<std::uint16_t>((dst << 8) | (dst >> 8));
    const int s = static_cast<std::uint16_t>((src << 8) | (src >> 8));
    const int r = ((d >> 11) & 31) + ((((s >> 11) & 31) - ((d >> 11) & 31)) * a >> 8);
    const int g = ((d >> 5) & 63) + ((((s >> 5) & 63) - ((d >> 5) & 63)) * a >> 8);
    const int b = (d & 31) + (((s & 31) - (d & 31)) * a >> 8);
    const auto v = static_cast<std::uint16_t>((r << 11) | (g << 5) | b);
    return static_cast<Rgb565be>((v << 8) | (v >> 8));
}

struct Span { float x0 = 0, x1 = 0; };
struct Pt { float x, y; };

// Generic convex rasterizer. `alpha` 0..256 is the shape opacity.
template <class SpanFn>
void fill(const Canvas& cv, float top, float bottom, Rgb565be c, int alpha, const SpanFn& span_at) noexcept {
    if (alpha <= 0) return;
    constexpr int kSub = 4;
    const int y0 = std::max(0, static_cast<int>(std::floor(top)));
    const int y1 = std::min(gfx::kScreen - 1, static_cast<int>(std::ceil(bottom)));
    for (int y = y0; y <= y1; ++y) {
        const gfx::Chord ch = gfx::kChord[static_cast<std::size_t>(y)];
        if (ch.x1 <= ch.x0) continue;
        Span s[kSub];
        float lo = 1e9f, hi = -1e9f, in_lo = -1e9f, in_hi = 1e9f;
        bool any = false, all = true;
        for (int k = 0; k < kSub; ++k) {
            s[k] = span_at(static_cast<float>(y) + (static_cast<float>(k) + 0.5f) / kSub);
            if (s[k].x1 > s[k].x0) {
                any = true;
                lo = std::min(lo, s[k].x0); hi = std::max(hi, s[k].x1);
                in_lo = std::max(in_lo, s[k].x0); in_hi = std::min(in_hi, s[k].x1);
            } else {
                all = false;
            }
        }
        if (!any) continue;
        const int px0 = std::max(static_cast<int>(std::floor(lo)), static_cast<int>(ch.x0));
        const int px1 = std::min(static_cast<int>(std::ceil(hi)), static_cast<int>(ch.x1));
        int f0 = 1 << 20, f1 = -(1 << 20);
        if (all && in_hi > in_lo) { f0 = static_cast<int>(std::ceil(in_lo)); f1 = static_cast<int>(std::floor(in_hi)); }
        // edge coverage in 24.8 fixed point: integer math per pixel (the C61 has no FPU)
        std::int32_t sx0[kSub], sx1[kSub];
        for (int k = 0; k < kSub; ++k) {
            const bool on = s[k].x1 > s[k].x0;
            sx0[k] = on ? static_cast<std::int32_t>(std::max(s[k].x0, -1.0f) * 256.0f) : 0;
            sx1[k] = on ? static_cast<std::int32_t>(std::min(s[k].x1, 241.0f) * 256.0f) : 0;
        }
        Rgb565be* row = cv.px.data() + static_cast<std::size_t>(y) * gfx::kScreen;
        for (int x = px0; x < px1; ++x) {
            if (x >= f0 && x < f1) {
                row[x] = alpha >= 256 ? c : mix(row[x], c, alpha);
                continue;
            }
            const std::int32_t l = x << 8, r = l + 256;
            std::int32_t cov = 0;
            for (int k = 0; k < kSub; ++k) {
                const std::int32_t o = std::min(r, sx1[k]) - std::max(l, sx0[k]);
                if (o > 0) cov += o;
            }
            const int a = static_cast<int>((cov * alpha) >> 10);  // cov max 4 * 256
            if (a > 0) row[x] = mix(row[x], c, a);
        }
    }
}

// ---- span functions ----------------------------------------------------------------------------------------

struct CircleSpan {
    float cx, cy, r;
    Span operator()(float y) const noexcept {
        const float dy = y - cy, h = r * r - dy * dy;
        if (h <= 0) return {};
        const float w = std::sqrt(h);
        return {cx - w, cx + w};
    }
};

// Ellipse with radii rx, ry rotated by `angle` radians (clockwise on screen, since y points down).
struct EllipseSpan {
    float cx, cy, a, bk, ck, half_h;
    EllipseSpan(float x, float y, float rx, float ry, float angle) noexcept : cx(x), cy(y) {
        const float c = std::cos(angle), s = std::sin(angle);
        const float irx = 1.0f / (rx * rx), iry = 1.0f / (ry * ry);
        a = c * c * irx + s * s * iry;
        bk = 2.0f * c * s * (irx - iry);
        ck = s * s * irx + c * c * iry;
        half_h = std::sqrt(rx * rx * s * s + ry * ry * c * c);
    }
    Span operator()(float y) const noexcept {
        const float Y = y - cy, b = bk * Y, cc = ck * Y * Y - 1.0f;
        const float disc = b * b - 4.0f * a * cc;
        if (disc <= 0) return {};
        const float r = std::sqrt(disc), inv = 0.5f / a;
        return {cx + (-b - r) * inv, cx + (-b + r) * inv};
    }
};

// Convex polygon (any winding), up to 16 points.
struct PolySpan {
    Pt p[16];
    int n = 0;
    float top = 1e9f, bottom = -1e9f;
    PolySpan(const Pt* pts, int count) noexcept : n(std::min(count, 16)) {
        for (int i = 0; i < n; ++i) { p[i] = pts[i]; top = std::min(top, pts[i].y); bottom = std::max(bottom, pts[i].y); }
    }
    Span operator()(float y) const noexcept {
        float lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < n; ++i) {
            const Pt a = p[i], b = p[(i + 1) % n];
            if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
                const float x = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y);
                lo = std::min(lo, x); hi = std::max(hi, x);
            }
        }
        return hi > lo ? Span{lo, hi} : Span{};
    }
};

// Thick line with round caps.
struct CapsuleSpan {
    CircleSpan c0, c1;
    PolySpan band;
    static PolySpan make_band(float x0, float y0, float x1, float y1, float r) noexcept {
        float dx = x1 - x0, dy = y1 - y0;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-3f) { const Pt z[1] = {{x0, y0}}; return PolySpan(z, 1); }
        const float nx = -dy / len * r, ny = dx / len * r;
        const Pt q[4] = {{x0 + nx, y0 + ny}, {x1 + nx, y1 + ny}, {x1 - nx, y1 - ny}, {x0 - nx, y0 - ny}};
        return PolySpan(q, 4);
    }
    CapsuleSpan(float x0, float y0, float x1, float y1, float r) noexcept
        : c0{x0, y0, r}, c1{x1, y1, r}, band(make_band(x0, y0, x1, y1, r)) {}
    Span operator()(float y) const noexcept {
        const Span parts[3] = {c0(y), c1(y), band.n >= 3 ? band(y) : Span{}};
        float lo = 1e9f, hi = -1e9f;
        for (const Span& s : parts)
            if (s.x1 > s.x0) { lo = std::min(lo, s.x0); hi = std::max(hi, s.x1); }
        return hi > lo ? Span{lo, hi} : Span{};
    }
};

// ---- shapes ---------------------------------------------------------------------------------------------------

inline void circle(const Canvas& cv, float cx, float cy, float r, Rgb565be c, int alpha = 256) noexcept {
    if (r <= 0) return;
    fill(cv, cy - r, cy + r, c, alpha, CircleSpan{cx, cy, r});
}

inline void ellipse(const Canvas& cv, float cx, float cy, float rx, float ry, Rgb565be c, float angle = 0,
                    int alpha = 256) noexcept {
    if (rx <= 0 || ry <= 0) return;
    const EllipseSpan e(cx, cy, rx, ry, angle);
    fill(cv, cy - e.half_h, cy + e.half_h, c, alpha, e);
}

inline void capsule(const Canvas& cv, float x0, float y0, float x1, float y1, float r, Rgb565be c,
                    int alpha = 256) noexcept {
    fill(cv, std::min(y0, y1) - r, std::max(y0, y1) + r, c, alpha, CapsuleSpan(x0, y0, x1, y1, r));
}

inline void polygon(const Canvas& cv, const Pt* p, int n, Rgb565be c, int alpha = 256) noexcept {
    const PolySpan ps(p, n);
    fill(cv, ps.top, ps.bottom, c, alpha, ps);
}

// Arc of radius R from angle a0 to a1 (radians, 0 = right, pi/2 = down), stroke half-width w.
inline void arc(const Canvas& cv, float cx, float cy, float R, float a0, float a1, float w, Rgb565be c,
                int alpha = 256, int segments = 6) noexcept {
    float px = cx + R * std::cos(a0), py = cy + R * std::sin(a0);
    for (int i = 1; i <= segments; ++i) {
        const float a = a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(segments);
        const float x = cx + R * std::cos(a), y = cy + R * std::sin(a);
        capsule(cv, px, py, x, y, w, c, alpha);
        px = x; py = y;
    }
}

// Four-pointed sparkle star.
inline void sparkle(const Canvas& cv, float cx, float cy, float r, Rgb565be c, int alpha = 256) noexcept {
    if (r < 1) return;
    const Pt v[4] = {{cx, cy - r}, {cx + r * 0.22f, cy}, {cx, cy + r}, {cx - r * 0.22f, cy}};
    const Pt h[4] = {{cx - r, cy}, {cx, cy - r * 0.22f}, {cx + r, cy}, {cx, cy + r * 0.22f}};
    polygon(cv, v, 4, c, alpha);
    polygon(cv, h, 4, c, alpha);
}

// Vertical gradient over the visible circle, rows [y0, y1).
inline void vgradient(const Canvas& cv, std::uint32_t top, std::uint32_t bottom, int y0 = 0,
                      int y1 = gfx::kScreen) noexcept {
    y0 = std::max(y0, 0); y1 = std::min(y1, gfx::kScreen);
    for (int y = y0; y < y1; ++y) {
        const gfx::Chord ch = gfx::kChord[static_cast<std::size_t>(y)];
        const Rgb565be c = hex(lerp_rgb(top, bottom, (y - y0) * 256 / std::max(1, y1 - y0 - 1)));
        Rgb565be* row = cv.px.data() + static_cast<std::size_t>(y) * gfx::kScreen;
        std::fill(row + ch.x0, row + ch.x1, c);
    }
}

// Scales the brightness of the visible circle, a in 0..256 (used for page fades).
inline void dim(const Canvas& cv, int a) noexcept {
    if (a >= 256) return;
    a = std::max(a, 0);
    for (int y = 0; y < gfx::kScreen; ++y) {
        const gfx::Chord ch = gfx::kChord[static_cast<std::size_t>(y)];
        Rgb565be* row = cv.px.data() + static_cast<std::size_t>(y) * gfx::kScreen;
        for (int x = ch.x0; x < ch.x1; ++x) {
            const int v = static_cast<std::uint16_t>((row[x] << 8) | (row[x] >> 8));
            const int r = ((v >> 11) & 31) * a >> 8, g = ((v >> 5) & 63) * a >> 8, b = (v & 31) * a >> 8;
            const auto o = static_cast<std::uint16_t>((r << 11) | (g << 5) | b);
            row[x] = static_cast<Rgb565be>((o << 8) | (o >> 8));
        }
    }
}

// Centred text with a soft 1-scale drop shadow, for overlays on busy pictures.
inline void label(const Canvas& cv, int y, int scale, Rgb565be color, std::string_view s,
                  Rgb565be shadow = hex(0x1a1a2e)) noexcept {
    const int x = (gfx::kScreen - text_width(s, scale)) / 2;
    const int off = std::max(1, scale / 2);
    draw_text(cv, x + off, y + off, scale, shadow, s);
    draw_text(cv, x, y, scale, color, s);
}

}  // namespace screen::draw
