// screen: day-page props, see props.hpp. Pure C++.
#include "screen/core/props.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>

#include "screen/core/draw.hpp"

namespace screen::props {
namespace {

using draw::hex;
using draw::Pt;

constexpr float kPi = 3.14159265f;

// Phase-locked sine over a repeating period in ms.
float wave(std::uint32_t t, std::uint32_t period) noexcept {
    return std::sin(2 * kPi * static_cast<float>(t % period) / static_cast<float>(period));
}

// Deterministic 0..1 value for piece i and field `salt` of a visit seed (integer hash, no rand()).
float rnd(std::uint32_t seed, std::uint32_t i, std::uint32_t salt) noexcept {
    std::uint32_t h = seed * 0x9E3779B1u ^ (i + 1) * 0x85EBCA77u ^ (salt + 7) * 0xC2B2AE3Du;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFu) / 65536.0f;
}

// Piece i of a visit enters above the top edge at progress 0 and has left below the bottom at 1. Pieces start
// within the first `span` of the visit and each take 0.55..0.6 of it, so all are gone by k = 1.
float progress(float k, std::uint32_t seed, std::uint32_t i, float span) noexcept {
    const float start = span * rnd(seed, i, 1);
    const float dur = 0.55f + 0.05f * rnd(seed, i, 2);
    return (k - start) / dur;
}

// Pointed leaf (a lens) centred at (cx, cy), long axis at angle a, half length L, half width W, in pixels.
void lens(const Canvas& cv, float cx, float cy, float L, float W, float a, std::uint32_t c, int alpha) noexcept {
    const float ca = std::cos(a), sa = std::sin(a);
    auto width = [&](float u) { return W * std::pow(std::max(0.0f, 1 - (u / L) * (u / L)), 0.6f); };
    Pt pts[12];
    int n = 0;
    for (int j = 0; j <= 6; ++j) {  // one edge, tip to tip
        const float u = L * (static_cast<float>(j) / 3 - 1), w = width(u);
        pts[n++] = {cx + u * ca - w * sa, cy + u * sa + w * ca};
    }
    for (int j = 5; j >= 1; --j) {  // the other edge back
        const float u = L * (static_cast<float>(j) / 3 - 1), w = width(u);
        pts[n++] = {cx + u * ca + w * sa, cy + u * sa - w * ca};
    }
    draw::polygon(cv, pts, n, hex(c), alpha);
}

// A leaf with a darker edge (grown by `grow` px) and a thin midrib in the edge colour.
void leaf(const Canvas& cv, float cx, float cy, float L, float W, float a, std::uint32_t c, std::uint32_t edge,
          float grow, int alpha) noexcept {
    lens(cv, cx, cy, L + grow, W + grow, a, edge, alpha);
    lens(cv, cx, cy, L, W, a, c, alpha);
    const float ca = std::cos(a) * L * 0.8f, sa = std::sin(a) * L * 0.8f;
    draw::capsule(cv, cx - ca, cy - sa, cx + ca, cy + sa, 0.12f * W + 0.15f, hex(edge), alpha);
}

// Heart with its lobes at cy - 0.2 r and the tip at cy + 1.2 r, with a darker outline.
void heart_px(const Canvas& cv, float cx, float cy, float r) noexcept {
    for (int pass = 0; pass < 2; ++pass) {
        const float rr = pass == 0 ? r * 1.25f : r;
        const std::uint32_t col = pass == 0 ? 0x9A1A2E : 0xE8384F;
        draw::circle(cv, cx - 0.5f * rr, cy - 0.2f * rr, 0.55f * rr, hex(col));
        draw::circle(cv, cx + 0.5f * rr, cy - 0.2f * rr, 0.55f * rr, hex(col));
        const Pt tri[3] = {{cx - 1.0f * rr, cy - 0.2f * rr}, {cx + 1.0f * rr, cy - 0.2f * rr}, {cx, cy + 1.2f * rr}};
        draw::polygon(cv, tri, 3, hex(col));
    }
}

// Design units with the origin at the bottom middle of a prop; 1 unit = 1 px at s = 1. Convex shapes get a
// darker edge grown by 0.7 units from their centre.
struct Grid {
    const Canvas& cv;
    float ox, oy, s;
    float X(float u) const noexcept { return ox + u * s; }
    float Y(float v) const noexcept { return oy + v * s; }
    void shape(const Pt* p, int n, std::uint32_t c, std::uint32_t edge) const noexcept {
        Pt q[8], o[8];
        Pt cen{0, 0};
        const float fn = static_cast<float>(n);
        for (int i = 0; i < n; ++i) {
            q[i] = {X(p[i].x), Y(p[i].y)};
            cen.x += q[i].x / fn;
            cen.y += q[i].y / fn;
        }
        for (int i = 0; i < n; ++i) {
            const float dx = q[i].x - cen.x, dy = q[i].y - cen.y;
            const float len = std::sqrt(dx * dx + dy * dy) + 1e-3f;
            o[i] = {q[i].x + dx / len * 0.7f * s, q[i].y + dy / len * 0.7f * s};
        }
        draw::polygon(cv, o, n, hex(edge));
        draw::polygon(cv, q, n, hex(c));
    }
};

// Terracotta pot, plant leaves and stem colours.
constexpr std::uint32_t kPot = 0xD0744E, kPotEdge = 0x8A4426, kPotRim = 0xE38B63, kSoil = 0x4E3020;
constexpr std::uint32_t kLeafFresh = 0x5FB85A, kLeafFreshEdge = 0x3A8240, kStemFresh = 0x4E9E3F;
constexpr std::uint32_t kOlive = 0xB5A642, kOliveEdge = 0x8A8230;
constexpr std::uint32_t kPetalPink = 0xFFD1DC, kPetalPale = 0xFFE6EC, kPetalEdge = 0xE8A0B4;
constexpr std::uint32_t kBlossom = 0xFF8FB8, kBlossomEdge = 0xD8578A, kBlossomCentre = 0xFFD84A;

}  // namespace

void rainbow(const Canvas& cv, float cx, float cy, float R, float w, int alpha) noexcept {
    // soft pastel red, orange, yellow, green, blue, violet, outside in
    constexpr std::uint32_t kBands[6] = {0xFF8585, 0xFFB366, 0xFFE27A, 0x9ADE8A, 0x7CC4F2, 0xB9A0F0};
    const int a = std::clamp(alpha, 0, 256) * 240 / 256;
    for (int i = 0; i < 6; ++i) {
        const float r = R - (static_cast<float>(i) + 0.5f) * w;  // centre line of this band
        draw::arc(cv, cx, cy, r, kPi, 2 * kPi, w / 2 + 0.5f, hex(kBands[i]), a, 24);
    }
}

void falling_leaves(const Canvas& cv, float k, std::uint32_t t, std::uint32_t seed) noexcept {
    constexpr std::uint32_t kFill[4] = {0xF08A24, 0xE0503A, 0xF5C542, 0x9C6A3C};
    constexpr std::uint32_t kEdge[4] = {0xB35A12, 0x9A2E1E, 0xB8902A, 0x5E3D1E};
    for (std::uint32_t i = 0; i < 7; ++i) {
        const float p = progress(k, seed, i, 0.4f);
        if (p < 0 || p > 1) continue;
        const float x = 14 + 212 * rnd(seed, i, 3) + 16 * std::sin(2 * kPi * (1.5f * p + rnd(seed, i, 4)));
        const float y = -12 + 264 * p;
        const float L = 8.5f + 2.5f * rnd(seed, i, 7);
        const float a = 2 * kPi * rnd(seed, i, 5) + 1.4f * wave(t, 5000 + 700 * i);  // slow tumbling
        const auto c = static_cast<std::uint32_t>(rnd(seed, i, 8) * 4) & 3u;
        leaf(cv, x, y, L, 0.42f * L, a, kFill[c], kEdge[c], 0.7f, 256);
    }
}

void petals(const Canvas& cv, float k, std::uint32_t t, std::uint32_t seed) noexcept {
    for (std::uint32_t i = 0; i < 12; ++i) {
        const float p = progress(k, seed, i, 0.4f);
        if (p < 0 || p > 1) continue;
        // mostly sideways: a wide swing, plus a drift across the screen from one side to the other
        const float dir = rnd(seed, i, 6) < 0.5f ? -1.0f : 1.0f;
        const float x = 12 + 216 * rnd(seed, i, 3) + 26 * std::sin(2 * kPi * (1.2f * p + rnd(seed, i, 4))) +
                        dir * 40 * (p - 0.5f);
        const float y = -10 + 262 * p;
        const float a = 2 * kPi * rnd(seed, i, 5) + 1.8f * wave(t, 2600 + 130 * i);  // light spin
        const float rx = 5.2f, ry = 3.2f;
        const std::uint32_t fill = i % 2 == 0 ? kPetalPink : kPetalPale;
        draw::ellipse(cv, x, y, rx + 0.9f, ry + 0.9f, hex(kPetalEdge), a);
        draw::ellipse(cv, x, y, rx, ry, hex(fill), a);
        draw::circle(cv, x - 0.6f * rx * std::cos(a), y - 0.6f * rx * std::sin(a) - 0.4f, 0.7f, hex(0xFFFFFF), 170);
    }
}

void wall_calendar(const Canvas& cv, float x, float y, float s, int days, bool heart) noexcept {
    // design units: origin at the top-left corner, 28 x 32 px at s = 1
    const float x0 = x - 14 * s, y0 = y - 16 * s;
    auto X = [&](float u) { return x0 + u * s; };
    auto Y = [&](float v) { return y0 + v * s; };
    auto box = [&](float u0, float v0, float u1, float v1, std::uint32_t c, std::uint32_t edge, float grow,
                   int alpha) {
        const float g = grow * s;
        if (grow > 0) {
            const Pt e[4] = {{X(u0) - g, Y(v0) - g}, {X(u1) + g, Y(v0) - g}, {X(u1) + g, Y(v1) + g}, {X(u0) - g, Y(v1) + g}};
            draw::polygon(cv, e, 4, hex(edge), alpha);
        }
        const Pt f[4] = {{X(u0), Y(v0)}, {X(u1), Y(v0)}, {X(u1), Y(v1)}, {X(u0), Y(v1)}};
        draw::polygon(cv, f, 4, hex(c), alpha);
    };
    box(1.6f, 8.6f, 29.6f, 33.6f, 0x1E1E2E, 0x1E1E2E, 0, 70);  // slight shadow under the page
    box(0, 7, 28, 32, 0xFFFFFF, 0xB8BCC8, 0.8f, 256);          // page
    box(0, 0, 28, 8, 0xD9534F, 0x9E2F2C, 0.8f, 256);            // red top band
    for (const float ru : {7.0f, 21.0f}) {                       // two rings
        draw::circle(cv, X(ru), Y(0.8f), 2.0f * s, hex(0x3A3A44));
        draw::circle(cv, X(ru), Y(0.8f), 0.9f * s, hex(0xD8D8E0));
    }
    // the day number, big and centred on the page
    char buf[8];
    std::snprintf(buf, sizeof buf, "%d", std::clamp(days, 1, 99));
    const std::string_view txt{buf};
    const int scale = std::max(1, static_cast<int>(std::lround(2 * s)));
    const int tx = static_cast<int>(std::lround(X(14))) - screen::text_width(txt, scale) / 2;
    const int ty = static_cast<int>(std::lround(Y(heart ? 9.0f : 12.5f)));
    screen::draw_text(cv, tx, ty, scale, hex(0x2B2B3A), txt);
    if (heart) heart_px(cv, X(14), Y(26.0f), 2.0f * s);
}

void pen(const Canvas& cv, float x, float y, float s, float angle) noexcept {
    const float ca = std::cos(angle), sa = std::sin(angle);
    // pen units: u along the pen (tip to the left), v across; rotated about the centre (x, y)
    auto P = [&](float u, float v) { return Pt{x + (u * ca - v * sa) * s, y + (u * sa + v * ca) * s}; };
    constexpr std::uint32_t kInk = 0x2F6FDB, kInkEdge = 0x1B4A9A, kMetal = 0xC6CED8, kMetalEdge = 0x5E6878;
    // body capsule: darker edge, then the blue
    const Pt b0 = P(-6, 0), b1 = P(6, 0);
    draw::capsule(cv, b0.x, b0.y, b1.x, b1.y, 2.9f * s, hex(kInkEdge));
    draw::capsule(cv, b0.x, b0.y, b1.x, b1.y, 2.3f * s, hex(kInk));
    // grip highlight along the top of the body
    const Pt h0 = P(-5, -0.9f), h1 = P(5, -0.9f);
    draw::capsule(cv, h0.x, h0.y, h1.x, h1.y, 0.45f * s, hex(0xFFFFFF), 150);
    // metal nib cone to the left, with the ink tip
    const Pt ne[3] = {P(-7.2f, -2.1f), P(-7.2f, 2.1f), P(-11.6f, 0.0f)};
    draw::polygon(cv, ne, 3, hex(kMetalEdge));
    const Pt nf[3] = {P(-7, -1.5f), P(-7, 1.5f), P(-11, 0.3f)};
    draw::polygon(cv, nf, 3, hex(kMetal));
    const Pt ink0 = P(-11, 0), ink1 = P(-11.3f, 0);
    draw::capsule(cv, ink0.x, ink0.y, ink1.x, ink1.y, 0.45f * s, hex(0x1B2A4A));
    // end cap, metal
    const Pt c0 = P(6, 0), c1 = P(8.6f, 0);
    draw::capsule(cv, c0.x, c0.y, c1.x, c1.y, 1.6f * s, hex(kMetalEdge));
    draw::capsule(cv, c0.x, c0.y, c1.x, c1.y, 1.1f * s, hex(kMetal));
    // white clip on the top side, a little curl at its end
    const Pt k0 = P(-3.5f, -2.6f), k1 = P(3.5f, -2.6f), k2 = P(3.5f, -0.4f);
    draw::capsule(cv, k0.x, k0.y, k1.x, k1.y, 0.9f * s, hex(0xB8BEC8));
    draw::capsule(cv, k1.x, k1.y, k2.x, k2.y, 0.9f * s, hex(0xB8BEC8));
    draw::capsule(cv, k0.x, k0.y, k1.x, k1.y, 0.5f * s, hex(0xFFFFFF));
    draw::capsule(cv, k1.x, k1.y, k2.x, k2.y, 0.5f * s, hex(0xFFFFFF));
}

void memory_plant(const Canvas& cv, float x, float base_y, float s, std::uint32_t t, int leaves, int droop,
                  bool flower) noexcept {
    const int n = std::clamp(leaves, 0, 9);
    const float dr = static_cast<float>(std::clamp(droop, 0, 3));
    const Grid g{cv, x, base_y, s};
    const float tint = dr * 70.0f;  // how far the green has gone towards olive
    const auto leaf_c = draw::lerp_rgb(kLeafFresh, kOlive, static_cast<int>(tint));
    const auto leaf_e = draw::lerp_rgb(kLeafFreshEdge, kOliveEdge, static_cast<int>(tint));
    const auto stem_c = draw::lerp_rgb(kStemFresh, kOlive, static_cast<int>(tint));

    // stem: from the soil up, swaying a little, bending outwards with droop
    const float sway = 0.8f * wave(t, 3000);
    const float bend = 2.4f * dr;
    const float height = n == 0 ? 3.0f : 3.0f + 0.9f * static_cast<float>(n);
    auto sp = [&](float f) { return Pt{sway * f + bend * f * f, -10.0f - height * f}; };
    const Pt s0 = sp(0), s1 = sp(0.5f), s2 = sp(1.0f);
    draw::capsule(cv, g.X(s0.x), g.Y(s0.y), g.X(s1.x), g.Y(s1.y), 1.3f * s, hex(leaf_e));
    draw::capsule(cv, g.X(s1.x), g.Y(s1.y), g.X(s2.x), g.Y(s2.y), 1.3f * s, hex(leaf_e));
    draw::capsule(cv, g.X(s0.x), g.Y(s0.y), g.X(s1.x), g.Y(s1.y), 0.7f * s, hex(stem_c));
    draw::capsule(cv, g.X(s1.x), g.Y(s1.y), g.X(s2.x), g.Y(s2.y), 0.7f * s, hex(stem_c));

    // leaves, alternating sides up the stem; a sprout has two small leaves at the top
    const int count = n == 0 ? 2 : n;
    const float len = n == 0 ? 2.5f : 2.9f + 0.12f * static_cast<float>(n);  // half length in design units
    for (int i = 0; i < count; ++i) {
        const float f = n == 0 ? 1.0f
                        : n == 1 ? 0.6f
                                 : 0.2f + 0.7f * static_cast<float>(i) / static_cast<float>(n - 1);
        const bool right = i % 2 == 0;
        const Pt b = sp(f);
        const float wob = 0.12f * std::sin(static_cast<float>(i) * 2.3f + 0.7f) +
                          0.07f * wave(t, 2200 + 200 * static_cast<std::uint32_t>(i));
        const float lift = 0.7f;
        const float a = right ? -lift + 0.5f * dr + wob : kPi + lift - 0.5f * dr - wob;
        const float L = len * s;
        // the leaf's base is at the stem; its middle is one half length further out
        leaf(cv, g.X(b.x) + std::cos(a) * L, g.Y(b.y) + std::sin(a) * L, L, 0.42f * L, a, leaf_c, leaf_e, 0.7f * s,
             256);
    }

    if (flower) {
        const Pt top = sp(1.0f);
        const float fx = g.X(top.x), fy = g.Y(top.y) - 0.5f * s;
        const float spin = 0.06f * wave(t, 4000);
        for (int pass = 0; pass < 2; ++pass) {
            for (int k = 0; k < 5; ++k) {
                const float a = -kPi / 2 + 2 * kPi * static_cast<float>(k) / 5 + spin;
                const float d = 2.0f * s;
                const float px = fx + std::cos(a) * d, py = fy + std::sin(a) * d;
                if (pass == 0) draw::ellipse(cv, px, py, 2.0f * s + 0.6f * s, 1.5f * s + 0.6f * s, hex(kBlossomEdge), a);
                else draw::ellipse(cv, px, py, 2.0f * s, 1.5f * s, hex(kBlossom), a);
            }
        }
        draw::circle(cv, fx, fy, 1.5f * s, hex(0xD9A020));
        draw::circle(cv, fx, fy, 1.1f * s, hex(kBlossomCentre));
    }

    // pot: rim and body, soil in the top
    const Pt rim[4] = {{-7.5f, -10}, {7.5f, -10}, {7.5f, -7.5f}, {-7.5f, -7.5f}};
    const Pt body[4] = {{-6.5f, -7.5f}, {6.5f, -7.5f}, {5.0f, 0.0f}, {-5.0f, 0.0f}};
    g.shape(body, 4, kPot, kPotEdge);
    g.shape(rim, 4, kPotRim, kPotEdge);
    draw::capsule(cv, g.X(-4.0f), g.Y(-5.5f), g.X(-4.0f), g.Y(-2.0f), 0.7f * s, hex(0xFFFFFF), 160);
    draw::ellipse(cv, g.X(0), g.Y(-10), 6.4f * s, 1.0f * s, hex(kSoil));
}

}  // namespace screen::props
