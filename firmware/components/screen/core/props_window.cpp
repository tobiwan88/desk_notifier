// screen: day-page props, see props.hpp. Pure C++.
#include "screen/core/props.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "screen/core/draw.hpp"

namespace screen::props {
namespace {

using draw::hex;
using draw::Pt;

constexpr float kPi = 3.14159265f;

// The glass rectangle. k is 1 for the 46 px wide window of the scene and grows with the window.
struct Glass {
    float x0, y0, x1, y1, w, h, k;
    int xa, xb, ya, yb;  // whole pixels inside the window, [xa, xb) x [ya, yb)
    explicit Glass(const Win& win) noexcept
        : x0(win.x0), y0(win.y0), x1(win.x1), y1(win.y1), w(win.x1 - win.x0), h(win.y1 - win.y0),
          k(std::min(win.x1 - win.x0, win.y1 - win.y0) / 46.0f),
          xa(std::max(0, static_cast<int>(win.x0))), xb(std::min(gfx::kScreen, static_cast<int>(win.x1))),
          ya(std::max(0, static_cast<int>(win.y0))), yb(std::min(gfx::kScreen, static_cast<int>(win.y1))) {}
};

float ph(std::uint32_t t, std::uint32_t period) noexcept {
    return static_cast<float>(t % period) / static_cast<float>(period);
}
float wave(std::uint32_t t, std::uint32_t period) noexcept { return std::sin(2 * kPi * ph(t, period)); }
std::uint32_t tt(std::uint32_t t, int i, std::uint32_t step) noexcept {
    return t + static_cast<std::uint32_t>(i) * step;
}
std::uint32_t hash(std::uint32_t n) noexcept {
    n ^= n >> 16;
    n *= 0x7feb352dU;
    n ^= n >> 15;
    n *= 0x846ca68bU;
    n ^= n >> 16;
    return n;
}
// Deterministic pseudo-random value in 0..1 for (index, salt).
float rr(int i, int salt) noexcept {
    return static_cast<float>(hash(static_cast<std::uint32_t>(i * 31 + salt)) & 0xFFFFU) / 65536.0f;
}
int scale_alpha(int a, int by) noexcept { return a * std::clamp(by, 0, 256) / 256; }
float frac(float g, float start, float span) noexcept { return std::clamp((g - start) / span, 0.0f, 1.0f); }

// Blends colour c over the glass pixels with alpha_at(x, y) in 0..256. Nothing outside the window is touched.
template <class AlphaFn>
void glass_fill(const Canvas& cv, const Glass& g, std::uint32_t c, const AlphaFn& alpha_at) noexcept {
    const auto col = hex(c);
    for (int y = g.ya; y < g.yb; ++y) {
        draw::Rgb565be* row = cv.px.data() + static_cast<std::size_t>(y) * gfx::kScreen;
        for (int x = g.xa; x < g.xb; ++x) {
            const int a = alpha_at(x, y);
            if (a > 0) row[x] = draw::mix(row[x], col, a);
        }
    }
}

void tint(const Canvas& cv, const Glass& g, std::uint32_t c, int alpha) noexcept {
    glass_fill(cv, g, c, [alpha](int, int) { return alpha; });
}

// A round water drop on glass: darker rim, pale body, a glint in the upper left. `a` 0..256 fades it.
void water_drop(const Canvas& cv, float cx, float cy, float r, int a, int glint) noexcept {
    draw::ellipse(cv, cx, cy + 0.1f * r, r * 1.12f, r * 1.22f, hex(0x5E83A4), 0.0f, scale_alpha(230, a));
    draw::ellipse(cv, cx, cy + 0.1f * r, r * 0.92f, r * 1.02f, hex(0xCDEEFD), 0.0f, scale_alpha(190, a));
    draw::circle(cv, cx - 0.35f * r, cy - 0.35f * r, std::max(0.5f, 0.3f * r), hex(0xFFFFFF),
                 scale_alpha(glint, a));
}

// A drop running down the glass from `top` towards `bottom`, p in 0..1 along the way, with a faint trail.
void runner(const Canvas& cv, const Glass& g, float x, float top, float bottom, float p, float r) noexcept {
    const float fade = std::min(1.0f, p * 8.0f) * std::min(1.0f, (1.0f - p) * 6.0f);
    if (fade <= 0.02f) return;
    const float y = top + p * (bottom - top);
    const int a = static_cast<int>(fade * 256);
    draw::capsule(cv, x, top, x, y, 0.45f * g.k, hex(0xDDF2FF), scale_alpha(90, a));
    water_drop(cv, x, y, r, a, 200);
}

// ---- frost ----------------------------------------------------------------------------------------------------

struct Seg { Pt a, b; float r; };

float clamp_angle(float a) noexcept { return std::clamp(a, 0.12f, kPi / 2 - 0.12f); }

// Segments of one corner fern. (ox, oy) is the corner point, (sx, sy) = +-1 points into the glass, `grow` 0..1
// grows the stem first and then the branches one after another. Every point stays inside the corner box.
int fern(Seg* out, float ox, float oy, float sx, float sy, float len, float grow, float k) noexcept {
    const float a0 = kPi / 4;
    auto go = [sx, sy](Pt from, float ang, float l) {
        return Pt{from.x + sx * std::cos(ang) * l, from.y + sy * std::sin(ang) * l};
    };
    int n = 0;
    const Pt o{ox, oy};
    out[n++] = Seg{o, go(o, a0, len * frac(grow, 0.0f, 0.35f)), 0.8f * k};
    for (int i = 0; i < 3; ++i) {
        const float f = 0.22f + 0.24f * static_cast<float>(i);
        const float gb = frac(grow, 0.3f + 0.15f * static_cast<float>(i), 0.3f);
        if (gb <= 0.0f) break;
        const Pt base = go(o, a0, len * f);
        const float lb = len * 0.5f * (1.0f - 0.2f * static_cast<float>(i)) * gb;
        for (int s = -1; s <= 1; s += 2) {
            const float ang = clamp_angle(a0 + 0.5f * static_cast<float>(s));
            const Pt tip = go(base, ang, lb);
            out[n++] = Seg{base, tip, 0.6f * k};
            const Pt mid{(base.x + tip.x) * 0.5f, (base.y + tip.y) * 0.5f};
            const float gs = frac(gb, 0.6f, 0.4f);
            if (gs > 0.0f) out[n++] = Seg{mid, go(mid, clamp_angle(ang - 0.35f * static_cast<float>(s)), lb * 0.5f * gs), 0.45f * k};
        }
    }
    return n;
}

void draw_fern(const Canvas& cv, const Seg* segs, int n, int alpha, float k) noexcept {
    for (int i = 0; i < n; ++i)
        draw::capsule(cv, segs[i].a.x, segs[i].a.y, segs[i].b.x, segs[i].b.y, segs[i].r + 0.5f * k, hex(0xA9CBE6), alpha);
    for (int i = 0; i < n; ++i)
        draw::capsule(cv, segs[i].a.x, segs[i].a.y, segs[i].b.x, segs[i].b.y, segs[i].r, hex(0xFFFFFF), alpha);
}

// ---- leaves ---------------------------------------------------------------------------------------------------

// Outline of a leaf (pointed ellipse) with half-length lh and half-width hw, rotated by th about (cx, cy).
int leaf_outline(Pt* out, float cx, float cy, float lh, float hw, float th) noexcept {
    const float c = std::cos(th), s = std::sin(th);
    constexpr int kN = 10;
    auto pt = [&](float u, float v) { return Pt{cx + u * c - v * s, cy + u * s + v * c}; };
    int n = 0;
    for (int i = 0; i <= kN; ++i) {
        const float u = lh * (-1.0f + 2.0f * static_cast<float>(i) / kN);
        out[n++] = pt(u, hw * std::sqrt(std::max(0.0f, 1.0f - (u * u) / (lh * lh))));
    }
    for (int i = kN; i >= 0; --i) {
        const float u = lh * (-1.0f + 2.0f * static_cast<float>(i) / kN);
        out[n++] = pt(u, -hw * std::sqrt(std::max(0.0f, 1.0f - (u * u) / (lh * lh))));
    }
    return n;
}

void leaf(const Canvas& cv, float cx, float cy, float lh, float hw, float th, std::uint32_t col, int a, float k) noexcept {
    constexpr int kMax = 22;
    Pt pts[kMax];
    const int n = leaf_outline(pts, cx, cy, lh, hw * 1.25f, th);
    draw::polygon(cv, pts, n, hex(draw::lerp_rgb(col, 0x3A1A0A, 90)), a);
    leaf_outline(pts, cx, cy, lh, hw, th);
    draw::polygon(cv, pts, n, hex(col), a);
    const float c = std::cos(th) * 0.8f * lh, s = std::sin(th) * 0.8f * lh;
    draw::capsule(cv, cx - c, cy - s, cx + c, cy + s, 0.35f * k, hex(draw::lerp_rgb(col, 0x3A1A0A, 120)), a);
}

// ---- snowman --------------------------------------------------------------------------------------------------

void ball(const Canvas& cv, float cx, float cy, float r, float k) noexcept {
    draw::circle(cv, cx, cy, r + 0.7f * k, hex(0x7E9EBE));
    draw::circle(cv, cx, cy, r, hex(0xD4E6F7));
    draw::circle(cv, cx - 0.12f * r, cy - 0.12f * r, 0.9f * r, hex(0xFFFFFF));
}

}  // namespace

void glass_rain(const Canvas& cv, const Win& win, std::uint32_t t) noexcept {
    const Glass g(win);
    tint(cv, g, 0x6F8397, 36);
    const float pad = 3.2f * g.k;
    // drops sitting on the glass, each glinting on its own rhythm
    for (int i = 0; i < 9; ++i) {
        const float x = g.x0 + pad + rr(i, 1) * (g.w - 2 * pad);
        const float y = g.y0 + pad + rr(i, 2) * (g.h - 2 * pad);
        const float r = (1.4f + 0.9f * rr(i, 3)) * g.k;
        const int glint = 150 + static_cast<int>(70 * wave(tt(t, i, 300), 2600));
        water_drop(cv, x, y, r, 256, glint);
    }
    // a few running down, leaving a trail
    for (int i = 0; i < 3; ++i) {
        const float x = g.x0 + pad + (0.15f + 0.7f * rr(i, 4)) * (g.w - 2 * pad);
        const float top = g.y0 + pad + rr(i, 5) * 0.25f * (g.h - 2 * pad);
        const auto period = static_cast<std::uint32_t>(4200 + 900 * i);
        runner(cv, g, x, top, g.y1 - pad, ph(tt(t, i, 1700), period), 1.9f * g.k);
    }
}

void glass_frost(const Canvas& cv, const Win& win, std::uint32_t t) noexcept {
    const Glass g(win);
    // grows in the first ~3 s of a 9 s cycle, holds, then fades out and starts again
    const float p = ph(t, 9000);
    const float grow = std::min(1.0f, p * 3.5f);
    const float fade = p > 0.8f ? (1.0f - p) / 0.2f : 1.0f;
    const int alpha = static_cast<int>(fade * 255);
    if (alpha <= 0) return;
    tint(cv, g, 0xE6F4FF, static_cast<int>(fade * 40));
    const float pad = 2.6f * g.k;
    const float len = 0.44f * std::min(g.w, g.h);
    for (int c = 0; c < 4; ++c) {
        const float sx = (c & 1) ? -1.0f : 1.0f;  // 0 top left, 1 top right, 2 bottom left, 3 bottom right
        const float sy = (c & 2) ? -1.0f : 1.0f;
        const float ox = sx > 0 ? g.x0 + pad : g.x1 - pad;
        const float oy = sy > 0 ? g.y0 + pad : g.y1 - pad;
        Seg segs[16];
        const int n = fern(segs, ox, oy, sx, sy, len, std::max(0.0f, grow - 0.05f * static_cast<float>(c)), g.k);
        const int shimmer = static_cast<int>(25 * wave(tt(t, c, 700), 2600));
        draw_fern(cv, segs, n, scale_alpha(alpha, 230 + shimmer), g.k);
    }
}

void glass_fog(const Canvas& cv, const Win& win, std::uint32_t t) noexcept {
    const Glass g(win);
    // the wiped peephole drifts a little and breathes, a bit left of centre
    const float R = 0.22f * std::min(g.w, g.h) * (1.0f + 0.04f * wave(t, 3000));
    const float cx = g.x0 + 0.42f * g.w + 1.2f * g.k * wave(t, 6000);
    const float cy = g.y0 + 0.5f * g.h + 0.8f * g.k * wave(t + 1500, 7000);
    const float soft = 2.2f * g.k;
    const int cx16 = static_cast<int>(cx * 16), cy16 = static_cast<int>(cy * 16);
    // radii in 1/16 px, squared; the ramp between them gives the soft edge
    const int rin = std::max(0, static_cast<int>((R - soft) * 16));
    const int rout = std::max(rin + 1, static_cast<int>((R + soft) * 16));
    const int in2 = rin * rin, out2 = rout * rout;
    glass_fill(cv, g, 0xF0F4F6, [&](int x, int y) {
        const int dx = x * 16 + 8 - cx16, dy = y * 16 + 8 - cy16;
        const int d2 = dx * dx + dy * dy;
        if (d2 <= in2) return 0;
        if (d2 >= out2) return 170;
        return (d2 - in2) * 170 / (out2 - in2);
    });
}

void glass_steam(const Canvas& cv, const Win& win, std::uint32_t t) noexcept {
    const Glass g(win);
    const float top = g.y0 + 0.35f * g.h;  // the mist starts here and thickens downwards
    const int tn = static_cast<int>((t / 400) & 0xFFFFu);
    glass_fill(cv, g, 0xEEF3F6, [&](int x, int y) {
        if (static_cast<float>(y) < top) return 0;
        const int base = static_cast<int>((static_cast<float>(y) - top) * 200.0f / (g.y1 - top));
        const int n = static_cast<int>(hash(static_cast<std::uint32_t>((x / 3) * 73 + (y / 3) * 19 + tn)) & 31U) - 16;
        return std::clamp(base + n, 0, 200);
    });
    // condensation droplets in the misty part
    const float wet_top = g.y0 + 0.5f * g.h, pad = 3.2f * g.k;
    for (int i = 0; i < 5; ++i) {
        const float x = g.x0 + pad + rr(i, 11) * (g.w - 2 * pad);
        const float y = wet_top + rr(i, 12) * (g.y1 - pad - wet_top);
        water_drop(cv, x, y, (1.0f + 0.6f * rr(i, 13)) * g.k, 256, 140);
    }
    // a couple running down
    for (int i = 0; i < 2; ++i) {
        const float x = g.x0 + pad + (0.25f + 0.5f * rr(i, 14)) * (g.w - 2 * pad);
        const auto period = static_cast<std::uint32_t>(5200 + 1000 * i);
        runner(cv, g, x, wet_top, g.y1 - pad, ph(tt(t, i, 2100), period), 1.7f * g.k);
    }
}

void leaves_flying(const Canvas& cv, const Win& win, std::uint32_t t) noexcept {
    const Glass g(win);
    static constexpr std::uint32_t kCols[5] = {0xF28A22, 0xD8452B, 0xF6C23A, 0xE8772E, 0xC93F24};
    const float lh = 4.4f * g.k, hw = 2.1f * g.k;
    const float R = lh * 1.3f + g.k;  // keeps the whole leaf inside the window
    if (g.w <= 2 * R || g.h <= 2 * R) return;
    for (int i = 0; i < 5; ++i) {
        const auto period = static_cast<std::uint32_t>(6000 + 900 * i);
        const float p = ph(tt(t, i, 1700), period);
        const float fade = std::min(1.0f, p * 10.0f) * std::min(1.0f, (1.0f - p) * 10.0f);
        if (fade <= 0.02f) continue;
        const float x = g.x0 + R + p * (g.w - 2 * R);
        const float base_y = g.y0 + R + rr(i, 5) * (g.h - 2 * R);
        const float y = std::clamp(base_y + 1.5f * g.k * std::sin(2 * kPi * p * 3.0f + static_cast<float>(i)),
                                   g.y0 + R, g.y1 - R);
        const float th = rr(i, 6) * 2 * kPi + p * 3.0f * kPi;  // tumbles a second and a half times
        const float squash = 0.35f + 0.65f * std::fabs(std::cos(2 * kPi * p * 2.0f + static_cast<float>(i)));
        leaf(cv, x, y, lh, hw * squash, th, kCols[i], static_cast<int>(fade * 256), g.k);
    }
}

void snowman(const Canvas& cv, const Win& win, std::uint32_t t) noexcept {
    const Glass g(win);
    const float k = g.k, pad = 2.6f * k;
    const float gy = g.y1 - 0.28f * g.h;  // snow ground: top edge bumpy, rows below are snow
    for (int x = g.xa; x < g.xb; ++x) {
        const float fx = static_cast<float>(x);
        const float top = gy + k * (1.1f * std::sin(0.42f * fx + 0.7f) + 0.5f * std::sin(1.3f * fx));
        for (int y = std::max(g.ya, static_cast<int>(top)); y < g.yb; ++y)
            cv.set(x, y, hex(static_cast<float>(y) - top < 1.5f * k ? 0xDCEBF8 : 0xFFFFFF));
    }
    const float cx = g.x0 + 0.3f * g.w;
    const float r0 = 6.2f * k, r1 = 4.6f * k, r2 = 3.3f * k;
    const float b0 = gy + 2.0f * k - r0;
    const float b1 = b0 - (r0 + r1 - 1.2f * k);
    const float b2 = b1 - (r1 + r2 - 1.0f * k);
    // stick arms behind the body, the right one waves
    draw::capsule(cv, cx - r1 * 0.8f, b1 + 0.5f * k, cx - r1 - 5.5f * k, b1 - 3.5f * k, 0.5f * k, hex(0x6B4A2A));
    draw::capsule(cv, cx + r1 * 0.8f, b1 + 0.5f * k, cx + r1 + 5.5f * k,
                  b1 - 3.5f * k + 1.2f * k * wave(t, 2400), 0.5f * k, hex(0x6B4A2A));
    ball(cv, cx, b0, r0, k);
    ball(cv, cx, b1, r1, k);
    ball(cv, cx, b2, r2, k);
    // coal eyes, carrot nose, a row of coal mouth dots
    draw::circle(cv, cx - 1.3f * k, b2 - 0.8f * k, 0.6f * k, hex(0x222831));
    draw::circle(cv, cx + 1.3f * k, b2 - 0.8f * k, 0.6f * k, hex(0x222831));
    const Pt nose[3] = {{cx + 0.9f * k, b2 - 0.7f * k}, {cx + 5.0f * k, b2 + 0.3f * k}, {cx + 0.9f * k, b2 + 0.7f * k}};
    draw::polygon(cv, nose, 3, hex(0xFF8A1F));
    for (int i = -1; i <= 1; ++i)
        draw::circle(cv, cx + 1.1f * k * static_cast<float>(i), b2 + 1.4f * k, 0.35f * k, hex(0x222831));
    // two buttons
    draw::circle(cv, cx, b1 - 0.8f * k, 0.5f * k, hex(0x222831));
    draw::circle(cv, cx, b1 + 1.8f * k, 0.5f * k, hex(0x222831));
    // red scarf at the neck with a tail hanging down the right
    const float neck = b2 + 2.3f * k;
    draw::capsule(cv, cx - r2 * 0.85f, neck, cx + r2 * 0.85f, neck, 0.9f * k, hex(0xD62828));
    const Pt tail[4] = {{cx + 1.5f * k, neck}, {cx + 3.5f * k, neck + 0.8f * k}, {cx + 4.0f * k, neck + 5.0f * k},
                        {cx + 2.5f * k, neck + 4.5f * k}};
    draw::polygon(cv, tail, 4, hex(0xB01E1E));
    // snowflakes drifting down
    for (int i = 0; i < 6; ++i) {
        const float p = ph(tt(t, i, 900), static_cast<std::uint32_t>(4200 + 350 * i));
        const float x = g.x0 + pad + rr(i, 7) * (g.w - 2 * pad) + 1.5f * k * std::sin(2 * kPi * p * 2.0f + static_cast<float>(i));
        const float y = g.y0 + pad + p * (g.h - 2 * pad);
        const float fade = std::min(1.0f, (1.0f - p) * 6.0f) * std::min(1.0f, p * 6.0f);
        draw::circle(cv, std::clamp(x, g.x0 + pad, g.x1 - pad), y, 0.8f * k, hex(0xFFFFFF),
                     static_cast<int>(fade * 220));
    }
}

void window_open(const Canvas& cv, const Win& win, std::uint32_t t, int dark) noexcept {
    const Glass g(win);
    const float k = g.k, fw = 2.6f * k;
    auto dk = [dark](std::uint32_t c) { return hex(draw::lerp_rgb(c, 0x2A2C48, dark)); };

    // white frame around the opening, with a sill
    const auto frame_c = dk(0xF7F3EC);
    draw::capsule(cv, g.x0 - fw, g.y0 - fw * 0.5f, g.x1 + fw, g.y0 - fw * 0.5f, fw * 0.5f, frame_c);
    draw::capsule(cv, g.x0 - fw * 0.5f, g.y0 - fw, g.x0 - fw * 0.5f, g.y1 + fw, fw * 0.5f, frame_c);
    draw::capsule(cv, g.x1 + fw * 0.5f, g.y0 - fw, g.x1 + fw * 0.5f, g.y1 + fw, fw * 0.5f, frame_c);
    draw::capsule(cv, g.x0 - fw, g.y1 + fw * 0.5f, g.x1 + fw, g.y1 + fw * 0.5f, fw * 0.5f, frame_c);
    draw::capsule(cv, g.x0 - fw * 1.5f, g.y1 + fw * 1.9f, g.x1 + fw * 1.5f, g.y1 + fw * 1.9f, fw * 0.45f, dk(0xD9CFC0));

    // the sash swung into the room: a parallelogram hinged on the right edge, its free edge sways
    const float a = 0.55f * g.w;
    const float s = 0.10f * g.h + 1.2f * k * wave(t, 4200);
    auto para = [s](float xh, float xf, float yt, float yb, Pt* out) {
        out[0] = Pt{xh, yt};
        out[1] = Pt{xf, yt + s};
        out[2] = Pt{xf, yb + s};
        out[3] = Pt{xh, yb};
    };
    const float oxh = g.x1 + 0.6f * fw, oxf = g.x1 - a;
    const float oyt = g.y0 - 0.3f * fw, oyb = g.y1 + 0.3f * fw;
    Pt outer[4], pane[4];
    para(oxh, oxf, oyt, oyb, outer);
    draw::polygon(cv, outer, 4, frame_c);
    const float fs = 2.2f * k;
    para(oxh - fs, oxf + fs, oyt + fs, oyb - fs, pane);
    draw::polygon(cv, pane, 4, dk(0xBFE6FA), 205);
    const Pt corner{pane[1].x, pane[1].y};
    draw::capsule(cv, corner.x + 1.5f * k, corner.y + 2.0f * k, corner.x + 2.5f * k, corner.y + 7.0f * k, 0.5f * k,
                  hex(0xFFFFFF), 170);

    // pale curtain hanging left of the opening, its right edge billowing into the room
    const float left = g.x0 - 3.0f * k, right_base = g.x0 + 0.24f * g.w;
    const float top = g.y0 - 4.0f * k, bot = g.y1 + 4.0f * k;
    const float wv = ph(t, 2600) * 2 * kPi;
    constexpr int kSteps = 10;
    Pt cur[2 + kSteps + 1];
    int n = 0;
    cur[n++] = Pt{left, top};
    for (int j = 0; j <= kSteps; ++j) {
        const float fj = static_cast<float>(j) / kSteps;
        cur[n++] = Pt{right_base + 3.5f * k * std::sin(2 * kPi * 1.3f * fj + wv), top + (bot - top) * fj};
    }
    cur[n++] = Pt{left, bot};
    draw::polygon(cv, cur, n, dk(0xFFF0A8));
    const float d = right_base - left;
    draw::capsule(cv, left + 0.35f * d, top, left + 0.35f * d, bot, 0.5f * k, dk(0xE8CF6A), 150);
    draw::capsule(cv, left + 0.7f * d, top, left + 0.7f * d, bot, 0.5f * k, dk(0xE8CF6A), 120);
    draw::capsule(cv, left - 1.0f * k, top - 1.0f * k, right_base + 2.0f * k, top - 1.0f * k, 0.7f * k, dk(0x8A6A4A));

    // breeze lines drifting out of the window to the left and down
    for (int i = 0; i < 4; ++i) {
        const float p = ph(tt(t, i, 1100), static_cast<std::uint32_t>(3000 + 400 * i));
        const float al = std::sin(kPi * p);
        const float x = g.x0 - 3.5f * k - p * 12.0f * k;
        const float y = g.y0 + (0.25f + 0.22f * static_cast<float>(i)) * g.h + p * 7.0f * k;
        const float len = 7.0f * k;
        const float bx = x - len * 0.5f, by = y + 1.2f * k * std::sin(2 * kPi * p * 2.0f + static_cast<float>(i));
        const int a_breeze = static_cast<int>(al * 220);
        draw::capsule(cv, x, y, bx, by, 0.45f * k, hex(0xFFFFFF), a_breeze);
        draw::capsule(cv, bx, by, bx - len * 0.5f, y + 0.4f * k, 0.45f * k, hex(0xFFFFFF), a_breeze);
    }
}

}  // namespace screen::props
