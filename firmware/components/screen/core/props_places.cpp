// screen: shop items, see props.hpp. Pure C++.
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

// Deterministic 0..1 value for index i and field `salt` (integer hash, no rand()).
float rnd(std::uint32_t i, std::uint32_t salt) noexcept {
    std::uint32_t h = (i + 1) * 0x9E3779B1u ^ (salt + 7) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return static_cast<float>(h & 0xFFFFu) / 65536.0f;
}

// Position within a loop of `period` ms, 0..1: every animation repeats exactly.
float loop(std::uint32_t t, std::uint32_t period) noexcept {
    return static_cast<float>(t % period) / static_cast<float>(period);
}

// Smooth -1..1 wave with the same period.
float wave(std::uint32_t t, std::uint32_t period) noexcept { return std::sin(2 * kPi * loop(t, period)); }

// Day colour or night colour.
std::uint32_t pick(bool day, std::uint32_t d, std::uint32_t n) noexcept { return day ? d : n; }

// Alpha 0..256 from a 0..1 value.
int alpha_of(float a) noexcept { return static_cast<int>(std::clamp(a, 0.0f, 1.0f) * 256.0f); }

void tri(const Canvas& cv, Pt a, Pt b, Pt c, std::uint32_t col, int alpha = 256) noexcept {
    const Pt q[3] = {a, b, c};
    draw::polygon(cv, q, 3, hex(col), alpha);
}

void rect(const Canvas& cv, float x0, float y0, float x1, float y1, std::uint32_t col,
          int alpha = 256) noexcept {
    const Pt q[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    draw::polygon(cv, q, 4, hex(col), alpha);
}

// ---- Alps --------------------------------------------------------------------------------------------------

// A mountain with apex (ax, ay) and feet at y `base` from lx to rx: a body, a shaded right flank and an
// optional snow cap (a kite whose lower point dips in the middle).
void mountain(const Canvas& cv, float ax, float ay, float base, float lx, float rx, bool capped,
              std::uint32_t body, std::uint32_t shade, std::uint32_t snow) noexcept {
    tri(cv, {ax, ay}, {lx, base}, {rx, base}, body);
    tri(cv, {ax, ay}, {rx, base}, {ax + 6, base}, shade);
    if (!capped) return;
    const float f = 0.3f;  // the cap reaches 30 % of the way down the peak
    const float sy = ay + f * (base - ay);
    const float xl = ax + (lx - ax) * f, xr = ax + (rx - ax) * f;
    const Pt kite[4] = {{ax, ay}, {xr, sy}, {ax, sy + 6}, {xl, sy}};
    draw::polygon(cv, kite, 4, hex(snow));
}

void alps(const Canvas& cv, std::uint32_t t, bool day) noexcept {
    const std::uint32_t haze = pick(day, 0xB9CADF, 0x3A4C74), haze_shade = pick(day, 0xA3B7D2, 0x30405F),
                        rock = pick(day, 0x7D92B3, 0x2B3D66), shade = pick(day, 0x64799C, 0x203052),
                        snow = pick(day, 0xF8FBFE, 0x9AAECD);
    mountain(cv, 100, 102, 182, 40, 168, true, haze, haze_shade, snow);   // far range
    mountain(cv, 160, 138, 182, 118, 206, false, rock, shade, snow);      // small peak on the right
    mountain(cv, 38, 92, 182, -30, 110, true, rock, shade, snow);         // the tallest, on the left

    // a tiny chalet on a meadow at the foot of the big peak
    draw::ellipse(cv, 56, 168, 34, 7, hex(pick(day, 0x8CC98A, 0x2D4B4E)));
    rect(cv, 48, 158, 64, 169, pick(day, 0xF5EBD0, 0x8E8A86));
    tri(cv, {45, 159}, {56, 147}, {67, 159}, pick(day, 0xB8604A, 0x6A3B45));
    rect(cv, 54, 162, 58, 169, pick(day, 0x7A4B33, 0x3A2A30));
    rect(cv, 50, 161, 53, 164, pick(day, 0x3E5A78, 0xFFD27A));

    // a cloud drifting across the peaks, wrapping off screen on both sides
    const float cx = -36 + 300 * loop(t, 30000);
    const std::uint32_t cloud = pick(day, 0xF4F8FF, 0x6D7FA8);
    draw::capsule(cv, cx - 5, 126, cx + 17, 126, 4.5f, hex(cloud), 230);
    draw::circle(cv, cx + 1, 121, 6, hex(cloud), 230);
    draw::circle(cv, cx + 9, 118, 8, hex(cloud), 230);
    draw::circle(cv, cx + 17, 123, 5, hex(cloud), 230);
}

// ---- Beach -------------------------------------------------------------------------------------------------

void palm(const Canvas& cv, std::uint32_t t, bool day) noexcept {
    const std::uint32_t trunk = pick(day, 0x9A6A44, 0x4A3A3E), leaf = pick(day, 0x3DA15A, 0x1E5C4A),
                        nut = pick(day, 0x6B4226, 0x33262E);
    // curved trunk: a quadratic Bezier from the sand up to the crown, thinning towards the top
    const Pt p0{26, 182}, p1{12, 138}, p2{44, 104};
    Pt prev = p0;
    for (int k = 1; k <= 6; ++k) {
        const float s = static_cast<float>(k) / 6, u = 1 - s;
        const Pt p{u * u * p0.x + 2 * u * s * p1.x + s * s * p2.x, u * u * p0.y + 2 * u * s * p1.y + s * s * p2.y};
        draw::capsule(cv, prev.x, prev.y, p.x, p.y, 3.6f - 1.4f * s, hex(trunk));
        prev = p;
    }
    // six fronds from the crown, each swaying a little and drooping along its length
    constexpr float kAngle[6] = {150 * kPi / 180, 200 * kPi / 180, 250 * kPi / 180,
                                 290 * kPi / 180, 335 * kPi / 180, 20 * kPi / 180};
    constexpr float kLen[6] = {26, 30, 28, 28, 30, 25};
    for (std::uint32_t i = 0; i < 6; ++i) {
        const float a = kAngle[i] + 0.12f * std::sin(2 * kPi * (loop(t, 3000) + 0.17f * static_cast<float>(i)));
        const float dx = std::cos(a), dy = std::sin(a), L = kLen[i];
        Pt q{44, 104};
        for (int k = 1; k <= 3; ++k) {
            const float s = static_cast<float>(k) / 3;
            const Pt n{44 + dx * L * s, 104 + dy * L * s + 0.4f * L * s * s};
            draw::capsule(cv, q.x, q.y, n.x, n.y, 2.8f - 0.8f * static_cast<float>(k - 1), hex(leaf));
            q = n;
        }
    }
    draw::circle(cv, 42, 108, 2.6f, hex(nut));
    draw::circle(cv, 47, 109, 2.4f, hex(nut));
}

void beach(const Canvas& cv, std::uint32_t t, bool day) noexcept {
    // the sea, lighter at the horizon
    rect(cv, -2, 140, 242, 152, pick(day, 0x86D8F2, 0x23497A));
    rect(cv, -2, 152, 242, 166, pick(day, 0x46B6E6, 0x1B3C6C));
    rect(cv, -2, 166, 242, 190, pick(day, 0x2C94CC, 0x142E58));

    // sun (at night, moon) reflection, shimmering
    const std::uint32_t glint = pick(day, 0xFFF4C4, 0xD8E6FF);
    const int a = alpha_of(0.8f + 0.15f * wave(t, 2400));
    draw::ellipse(cv, 80, 146, 20, 2.2f, hex(glint), 0, a);
    draw::ellipse(cv, 80, 155, 13, 1.8f, hex(glint), 0, a);
    draw::ellipse(cv, 80, 163, 7, 1.5f, hex(glint), 0, a);

    // short wave lines sliding slowly to the right
    const std::uint32_t foam = pick(day, 0xEEFAFF, 0x9CC2EC);
    for (std::uint32_t row = 0; row < 3; ++row) {
        const float y = 147.0f + 9.0f * static_cast<float>(row);
        for (std::uint32_t i = 0; i < 4; ++i) {
            const std::uint32_t id = 10 + row * 4 + i;
            const float x0 = 260 * rnd(id, 1), len = 8 + 8 * rnd(id, 2);
            const auto period = 5000 + static_cast<std::uint32_t>(3000 * rnd(id, 3));
            const float x = std::fmod(x0 + 260 * loop(t, period), 260.0f) - 10;
            draw::capsule(cv, x, y, x + len, y, 1.1f, hex(foam), 210);
        }
    }

    palm(cv, t, day);
}

// ---- Autumn forest -----------------------------------------------------------------------------------------

// A round tree: trunk down to `base`, crown centred (x, cy) with radius r and a light spot on its top left.
void round_tree(const Canvas& cv, float x, float cy, float r, float base, std::uint32_t crown_col,
                std::uint32_t trunk) noexcept {
    draw::capsule(cv, x, cy + r * 0.6f, x, base, 2.4f, hex(trunk));
    draw::circle(cv, x, cy, r, hex(crown_col));
    draw::circle(cv, x - r * 0.35f, cy - r * 0.35f, r * 0.35f, hex(draw::lerp_rgb(crown_col, 0xFFFFFF, 60)), 110);
}

// A pointed fir: two stacked triangles over a trunk, apex at `top`, bottom of the crown at `base`.
void fir(const Canvas& cv, float x, float top, float base, float hw, std::uint32_t col,
         std::uint32_t trunk) noexcept {
    const float h = base - top;
    draw::capsule(cv, x, top + 0.85f * h, x, base, 2.4f, hex(trunk));
    tri(cv, {x, top}, {x - 0.8f * hw, top + 0.6f * h}, {x + 0.8f * hw, top + 0.6f * h}, col);
    tri(cv, {x, top + 0.3f * h}, {x - hw, top + 0.95f * h}, {x + hw, top + 0.95f * h}, col);
}

void forest(const Canvas& cv, std::uint32_t t, bool day) noexcept {
    const std::uint32_t orange = pick(day, 0xEE8A2E, 0x7E4F2A), red = pick(day, 0xD2473A, 0x5E2C35),
                        yellow = pick(day, 0xF4C43A, 0x6F6440), green = pick(day, 0x2F7A45, 0x203F3A),
                        trunk = pick(day, 0x7A4E32, 0x3A2A30);
    // back row, smaller
    fir(cv, 16, 98, 182, 15, green, trunk);
    round_tree(cv, 46, 108, 15, 182, red, trunk);
    fir(cv, 78, 94, 182, 14, green, trunk);
    round_tree(cv, 108, 106, 14, 182, yellow, trunk);
    round_tree(cv, 226, 112, 15, 182, orange, trunk);
    // front row, bigger
    round_tree(cv, 24, 134, 22, 182, orange, trunk);
    fir(cv, 60, 116, 182, 19, green, trunk);
    round_tree(cv, 94, 136, 19, 182, yellow, trunk);

    // three leaves drifting down, each fading in and out
    const std::uint32_t leaf_col[3] = {orange, red, yellow};
    for (std::uint32_t i = 0; i < 3; ++i) {
        const float p = std::fmod(loop(t, 7000 + 900 * i) + 0.33f * static_cast<float>(i), 1.0f);
        const float x = 24 + 50 * static_cast<float>(i) + 22 * p + 5 * std::sin(2 * kPi * 2 * p);
        draw::ellipse(cv, x, 92 + 82 * p, 3.4f, 1.9f, hex(leaf_col[i]), 2 * kPi * 2 * p,
                      alpha_of(std::sin(kPi * p)));
    }
}

// ---- Cherry blossom ----------------------------------------------------------------------------------------

void blossom(const Canvas& cv, std::uint32_t t, bool day) noexcept {
    const std::uint32_t bark = pick(day, 0x6A4636, 0x2E2434), light = pick(day, 0xFBD3E0, 0x8E6186),
                        mid = pick(day, 0xF4A6C0, 0x7C4E74), dark = pick(day, 0xE87EA3, 0x66416A),
                        petal = pick(day, 0xFFE0EA, 0xB99AC4);

    // left tree
    draw::capsule(cv, 30, 184, 36, 140, 3.6f, hex(bark));
    draw::capsule(cv, 36, 140, 40, 116, 2.6f, hex(bark));
    draw::capsule(cv, 40, 118, 26, 104, 1.8f, hex(bark));
    draw::capsule(cv, 40, 118, 54, 102, 1.8f, hex(bark));
    draw::circle(cv, 40, 94, 16, hex(mid));
    draw::circle(cv, 24, 104, 12, hex(light));
    draw::circle(cv, 56, 101, 13, hex(light));
    draw::circle(cv, 34, 80, 12, hex(light));
    draw::circle(cv, 52, 82, 11, hex(mid));
    draw::circle(cv, 40, 110, 9, hex(dark));

    // far right edge, cut off by the panel
    draw::capsule(cv, 220, 184, 214, 136, 3.4f, hex(bark));
    draw::capsule(cv, 214, 136, 204, 118, 1.8f, hex(bark));
    draw::capsule(cv, 214, 136, 228, 116, 1.8f, hex(bark));
    draw::circle(cv, 222, 106, 15, hex(mid));
    draw::circle(cv, 205, 104, 11, hex(light));
    draw::circle(cv, 236, 96, 11, hex(light));
    draw::circle(cv, 216, 84, 10, hex(light));
    draw::circle(cv, 232, 118, 8, hex(dark));

    // seven petals falling and swaying
    for (std::uint32_t i = 0; i < 7; ++i) {
        const auto period = 5000 + static_cast<std::uint32_t>(2500 * rnd(i, 4));
        const float q = std::fmod(loop(t, period) + rnd(i, 5), 1.0f);
        const float x0 = i < 4 ? 14 + 56 * rnd(i, 6) : 196 + 36 * rnd(i, 6);
        const float x = x0 + 12 * std::sin(2 * kPi * (1.5f * q + rnd(i, 7)));
        draw::ellipse(cv, x, 84 + 92 * q, 2.6f, 1.5f, hex(petal), 2 * kPi * 2 * q, alpha_of(std::sin(kPi * q)));
    }
}

// ---- City skyline ------------------------------------------------------------------------------------------

// Twinkling lit windows on a building from x0 to x1 starting at `top`; `seed` keeps buildings apart.
void windows(const Canvas& cv, float x0, float x1, float top, std::uint32_t seed, std::uint32_t t) noexcept {
    std::uint32_t k = 0;
    for (float y = top + 7; y < 178; y += 9) {
        for (float x = x0 + 3; x + 2.5f <= x1 - 2; x += 5) {
            const std::uint32_t id = seed * 64 + k++;
            if (rnd(id, 1) < 0.35f) continue;  // unlit
            const auto period = static_cast<std::uint32_t>(3000 + 3000 * rnd(id, 2));
            const bool dim = loop(t + 700u * id, period) < 0.12f;  // a slow twinkle
            rect(cv, x, y, x + 2.5f, y + 3, dim ? 0x6E5A44 : 0xFFD27A);
        }
    }
}

void skyline(const Canvas& cv, std::uint32_t t, bool day) noexcept {
    const std::uint32_t far = pick(day, 0xC4D2E4, 0x2A3556), near = pick(day, 0x9AAEC6, 0x1E2844),
                        tower = pick(day, 0x8FA3BC, 0x252F4C), dome = pick(day, 0x5FB9B0, 0x2E5A62),
                        ball = pick(day, 0xC9D6E6, 0x4A5A80);

    // far row, behind
    rect(cv, 0, 124, 12, 184, far);
    rect(cv, 84, 118, 98, 184, far);
    rect(cv, 98, 108, 112, 184, far);
    rect(cv, 120, 124, 134, 184, far);
    rect(cv, 150, 112, 164, 184, far);
    rect(cv, 178, 118, 192, 184, far);
    rect(cv, 226, 130, 240, 184, far);

    // near row
    rect(cv, 26, 138, 40, 184, near);
    rect(cv, 78, 130, 92, 184, near);
    rect(cv, 134, 136, 148, 184, near);
    rect(cv, 200, 136, 216, 184, near);
    rect(cv, 222, 120, 240, 184, near);

    // twin towers with onion domes, and the nave between them
    rect(cv, 44, 140, 78, 184, tower);
    rect(cv, 46, 116, 58, 184, tower);
    rect(cv, 64, 116, 76, 184, tower);
    for (float cx : {52.0f, 70.0f}) {
        draw::circle(cv, cx, 112, 6, hex(dome));
        draw::circle(cv, cx, 105, 3.2f, hex(dome));
        draw::capsule(cv, cx, 103, cx, 98, 0.8f, hex(dome));
    }

    // tall thin TV tower with a ball and a blinking red light on top
    tri(cv, {18, 88}, {11.5f, 184}, {24.5f, 184}, tower);
    draw::circle(cv, 18, 112, 5.2f, hex(ball));
    draw::capsule(cv, 18, 88, 18, 80, 0.9f, hex(tower));
    const bool on = loop(t, 1600) < 0.5f;
    if (on && !day) draw::circle(cv, 18, 80, 5, hex(0xFF4A4A), 70);
    draw::circle(cv, 18, 80, 2.2f, hex(on ? 0xFF4A4A : 0x7A2E36));

    if (!day) {
        windows(cv, 26, 40, 138, 1, t);
        windows(cv, 78, 92, 130, 2, t);
        windows(cv, 134, 148, 136, 3, t);
        windows(cv, 200, 216, 136, 4, t);
        windows(cv, 222, 240, 120, 5, t);
        windows(cv, 46, 58, 116, 6, t);
        windows(cv, 64, 76, 116, 7, t);
    }
}

}  // namespace

void place_scenery(const Canvas& cv, int which, std::uint32_t t, bool day) noexcept {
    switch (which) {
        case kPlaceAlps: alps(cv, t, day); break;
        case kPlaceBeach: beach(cv, t, day); break;
        case kPlaceForest: forest(cv, t, day); break;
        case kPlaceBlossom: blossom(cv, t, day); break;
        case kPlaceSkyline: skyline(cv, t, day); break;
        default: break;
    }
}

}  // namespace screen::props
