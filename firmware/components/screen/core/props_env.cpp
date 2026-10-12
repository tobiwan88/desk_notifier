// screen: day-page props, see props.hpp. Pure C++.
#include "screen/core/props.hpp"

#include <algorithm>
#include <cmath>

#include "screen/core/draw.hpp"

namespace screen::props {
namespace {

using draw::hex;
using draw::Pt;

constexpr float kPi = 3.14159265f;

// Phase of a repeating cycle, 0..1.
float ph(std::uint32_t t, std::uint32_t period) noexcept {
    return static_cast<float>(t % period) / static_cast<float>(period);
}
float wave(std::uint32_t t, std::uint32_t period) noexcept { return std::sin(2 * kPi * ph(t, period)); }
float frac(float v) noexcept { return v - std::floor(v); }

// Draws in prop units: (dx, dy) relative to the origin (x, y), sizes scaled by s (1 px at s = 1).
struct Pen {
    const Canvas& cv;
    float ox, oy, s;
    float X(float dx) const noexcept { return ox + dx * s; }
    float Y(float dy) const noexcept { return oy + dy * s; }
    void circle(float dx, float dy, float r, std::uint32_t c, int a = 256) const noexcept {
        draw::circle(cv, X(dx), Y(dy), r * s, hex(c), a);
    }
    void ellipse(float dx, float dy, float rx, float ry, std::uint32_t c, float ang = 0, int a = 256) const noexcept {
        draw::ellipse(cv, X(dx), Y(dy), rx * s, ry * s, hex(c), ang, a);
    }
    void capsule(float x0, float y0, float x1, float y1, float r, std::uint32_t c, int a = 256) const noexcept {
        draw::capsule(cv, X(x0), Y(y0), X(x1), Y(y1), r * s, hex(c), a);
    }
    void poly(const Pt* p, int n, std::uint32_t c, int a = 256) const noexcept {
        Pt q[16];
        n = std::min(n, 16);
        for (int i = 0; i < n; ++i) q[i] = {X(p[i].x), Y(p[i].y)};
        draw::polygon(cv, q, n, hex(c), a);
    }
};

// Plant: a terracotta pot with six leaves on a swaying stem.
constexpr std::uint32_t kLeaf = 0x5FB85A, kLeafDark = 0x3A8240, kLeafLight = 0xA6E08F;
constexpr std::uint32_t kPot = 0xD9825B, kPotDark = 0xA85A38, kPotRim = 0xE8956A;
constexpr std::uint32_t kDrop = 0x9ADBFF, kDropDark = 0x3F8FC9;

// Cups: white tea cup, brown cocoa mug with cream and pink marshmallows.
constexpr std::uint32_t kMug = 0x9A6440, kMugDark = 0x5E3A24, kCocoa = 0x5A3420;
constexpr std::uint32_t kMallow = 0xFFFFFF, kMallowShade = 0xF2B8CB;

// Watering can: green metal.
constexpr std::uint32_t kCan = 0x5DBB63, kCanDark = 0x33803A, kCanLight = 0xB5EBA8;

// Mister: translucent blue bottle, white trigger head, nozzle to the left.
constexpr std::uint32_t kGlass = 0x8ECFF5, kGlassDark = 0x3F7FB0, kPlastic = 0xFFFFFF, kPlasticDark = 0xB8BEC8;

}  // namespace

void plant(const Canvas& cv, float x, float base_y, float s, std::uint32_t t, bool wet) noexcept {
    const Pen p{cv, x, base_y, s};
    // pot: body, rim, soil
    const Pt body_o[4] = {{-9.5f, -10}, {9.5f, -10}, {7.5f, 1.2f}, {-7.5f, 1.2f}};
    const Pt body[4] = {{-8, -10}, {8, -10}, {6, 0}, {-6, 0}};
    const Pt rim_o[4] = {{-10.5f, -15}, {10.5f, -15}, {10.5f, -9}, {-10.5f, -9}};
    const Pt rim[4] = {{-9.5f, -14}, {9.5f, -14}, {9.5f, -10}, {-9.5f, -10}};
    p.poly(body_o, 4, kPotDark);
    p.poly(body, 4, kPot);
    p.poly(rim_o, 4, kPotDark);
    p.poly(rim, 4, kPotRim);
    p.ellipse(0, -14.5f, 8.5f, 1.6f, 0x5A3A26);
    p.capsule(-5, -7, -4, -2, 0.9f, 0xFFFFFF, 160);

    // leaves: angle from straight up (rad), length in px; they sway together
    constexpr float kAng[6] = {-1.15f, -0.6f, -0.18f, 0.22f, 0.65f, 1.1f};
    constexpr float kLen[6] = {13, 15, 17, 16, 14, 12};
    const float sway = wave(t, 3200) * 0.10f + wave(t, 1700) * 0.04f;
    const float stem_y = -14;
    for (int i = 0; i < 6; ++i) {
        const float th = kAng[i] + sway * (0.6f + 0.15f * static_cast<float>(i));
        const float psi = -kPi / 2 + th;  // direction of the leaf's long axis
        const float dx = std::cos(psi), dy = std::sin(psi);
        const float ry = kLen[i] * 0.5f, rx = kLen[i] * 0.25f;
        const float cx = dx * ry, cy = stem_y + dy * ry;
        const float ang = th - kPi;  // ellipse x-axis, perpendicular to the leaf's long axis
        p.ellipse(cx, cy, rx + 0.9f, ry + 0.9f, kLeafDark, ang);
        p.ellipse(cx, cy, rx, ry, kLeaf, ang);
        p.circle(cx - std::cos(ang) * rx * 0.35f, cy - 0.5f * ry, 1.1f, kLeafLight, 180);
    }
    // stem
    p.capsule(0, stem_y, sway * 4, -10, 0.9f, kLeafDark);

    if (wet) {
        for (int i = 0; i < 6; ++i) {
            const float th = kAng[i] + sway * (0.6f + 0.15f * static_cast<float>(i));
            const float psi = -kPi / 2 + th;
            const float side = (i % 2 == 0) ? 1.0f : -1.0f;
            const float dx = std::cos(psi) * kLen[i] * 0.7f - std::sin(psi) * side * 2.0f;
            const float dy = stem_y + std::sin(psi) * kLen[i] * 0.7f + std::cos(psi) * side * 2.0f;
            const int a = 200 + static_cast<int>(wave(t, 2000 + 150u * static_cast<std::uint32_t>(i)) * 40);
            p.circle(dx, dy, 1.9f, kDropDark, a);
            p.circle(dx, dy, 1.4f, kDrop, a);
            p.circle(dx - 0.5f, dy - 0.5f, 0.45f, 0xFFFFFF, 200);
        }
    }
}

void mister(const Canvas& cv, float x, float y, float s, std::uint32_t t) noexcept {
    const Pen p{cv, x, y, s};
    // puffs first, behind the bottle: three soft clouds drifting left from the nozzle tip
    constexpr float kTipX = -12, kTipY = -10.5f;
    for (int k = 0; k < 3; ++k) {
        const float q = frac(ph(t, 1800) + static_cast<float>(k) / 3.0f);
        const float a = std::sin(kPi * q);
        const float px = kTipX - q * 16, py = kTipY + std::sin(q * 2 * kPi) * 1.5f;
        p.circle(px, py, 1.6f + q * 3.0f, 0xE6F6FF, static_cast<int>(a * 200));
    }
    // bottle: outline capsule, then translucent fill, then highlight
    p.capsule(3, -4, 3, 8, 10.2f, kGlassDark);
    p.capsule(3, -4, 3, 8, 9, kGlass, 215);
    p.capsule(-2, -1, -2, 5, 1.3f, 0xFFFFFF, 170);
    // nozzle, then the white trigger head on top
    p.capsule(-5, kTipY, kTipX, kTipY, 2.6f, kPlasticDark);
    p.capsule(-5, kTipY, kTipX + 0.5f, kTipY, 1.8f, kPlastic);
    const Pt head_o[4] = {{-6, -15}, {10, -15}, {10, -7}, {-6, -7}};
    const Pt head[4] = {{-5, -14}, {9, -14}, {9, -8}, {-5, -8}};
    p.poly(head_o, 4, kPlasticDark);
    p.poly(head, 4, kPlastic);
    p.capsule(-3, -12, 5, -12, 0.8f, kPlasticDark, 150);
}

void watering_can(const Canvas& cv, float x, float y, float s, float tilt, std::uint32_t t) noexcept {
    const Pen p{cv, x, y, s};
    // the whole can turns nose-down (spout to the left goes down) about its middle
    const float a = -0.6f * std::clamp(tilt, 0.0f, 1.0f);
    const float ca = std::cos(a), sa = std::sin(a);
    auto rot = [&](float dx, float dy) { return Pt{dx * ca - dy * sa, dx * sa + dy * ca}; };
    auto seg = [&](Pt u, Pt v, float r, std::uint32_t c) { p.capsule(u.x, u.y, v.x, v.y, r, c); };

    const Pt spout0 = rot(-8, -1), spout1 = rot(-18, -7);
    const Pt handle0 = rot(3, -7), handle_top = rot(4, -13), handle1 = rot(10, -9);
    // spout and handle, behind the body
    seg(spout0, spout1, 2.6f, kCanDark);
    seg(spout0, spout1, 1.7f, kCan);
    seg(handle0, handle_top, 1.5f, kCanDark);
    seg(handle_top, handle1, 1.5f, kCanDark);
    seg(handle0, handle_top, 0.9f, kCan);
    seg(handle_top, handle1, 0.9f, kCan);
    p.ellipse(0, 0, 11.6f, 8.6f, kCanDark, a);
    p.ellipse(0, 0, 10.6f, 7.6f, kCan, a);
    seg(rot(-9, 3), rot(9, 3), 0.9f, kCanDark);  // band around the middle
    p.ellipse(-3, -4.5f, 3, 1.1f, kCanLight, a, 200);  // shine
    const Pt rose_o = rot(-18.4f, -7.5f);
    p.circle(rose_o.x, rose_o.y, 2.6f, kCanDark);
    p.circle(rose_o.x, rose_o.y, 1.6f, kCanLight);

    // stream from the spout tip when tipped far enough
    if (tilt > 0.5f) {
        const float k = std::min(1.0f, (tilt - 0.5f) * 4.0f);
        for (int i = 0; i < 4; ++i) {
            const float q = frac(ph(t, 900) + static_cast<float>(i) / 4.0f);
            const int al = static_cast<int>(std::sin(kPi * q) * 230 * k);
            p.circle(spout1.x + 0.6f * std::sin(q * 6), spout1.y + 2 + q * 16, 1.4f, kDrop, al);
        }
    }
}

void water_glass(const Canvas& cv, float x, float y, float s) noexcept {
    const Pen p{cv, x, y, s};
    const Pt out[4] = {{-10, -11}, {10, -11}, {8, 11}, {-8, 11}};
    const Pt glass[4] = {{-9, -10}, {9, -10}, {7, 10}, {-7, 10}};
    const Pt water[4] = {{-8.2f, -3}, {8.2f, -3}, {6.6f, 9.2f}, {-6.6f, 9.2f}};
    p.poly(out, 4, 0x8FA8BC);
    p.poly(glass, 4, 0xEEF8FF, 150);
    p.poly(water, 4, 0x6DBDEE, 235);
    p.ellipse(0, -3, 8.2f, 1.2f, 0xB8E4FA, 0, 230);  // water surface
    p.capsule(-5.5f, -6, -4.5f, 4, 0.9f, 0xFFFFFF, 190);  // highlight
}

namespace {

// A C-shaped handle on the right side of a cup: from (x, top) out to x + reach, down, and back to x.
void handle(const Pen& p, float x, float top, float bottom, float reach, std::uint32_t dark, std::uint32_t c) noexcept {
    const float xr = x + reach;
    p.capsule(x, top, xr, top, 2.4f, dark);
    p.capsule(xr, top, xr, bottom, 2.4f, dark);
    p.capsule(xr, bottom, x, bottom, 2.4f, dark);
    p.capsule(x, top, xr, top, 1.4f, c);
    p.capsule(xr, top, xr, bottom, 1.4f, c);
    p.capsule(xr, bottom, x, bottom, 1.4f, c);
}

// Wisps of steam rising from (x0 .. x0 + spread, y0): dotted, fading as they climb.
void steam(const Pen& p, float x0, float y0, float height, int wisps, std::uint32_t period, std::uint32_t t) noexcept {
    for (int i = 0; i < wisps; ++i) {
        const float base = static_cast<float>(i) / static_cast<float>(wisps);
        for (int j = 0; j < 3; ++j) {
            const float q = frac(ph(t, period) + base + static_cast<float>(j) * 0.12f);
            const float px = x0 + static_cast<float>(i) * 4.0f + std::sin(q * 2 * kPi) * 1.6f;
            const float py = y0 - q * height;
            p.circle(px, py, 1.2f + 0.3f * q, 0xFFFFFF, static_cast<int>(std::sin(kPi * q) * 170));
        }
    }
}

}  // namespace

void tea_cup(const Canvas& cv, float x, float y, float s, std::uint32_t t) noexcept {
    const Pen p{cv, x, y, s};
    // saucer, handle, cup
    p.ellipse(0, 9, 13, 3.6f, 0xC9C2BA);
    p.ellipse(0, 8.4f, 12.2f, 2.9f, 0xF6F3EE);
    handle(p, 8, -4, 4, 5, 0xB9B2A8, 0xFFFFFF);
    const Pt cup_o[4] = {{-11, -8}, {11, -8}, {8, 10}, {-8, 10}};
    const Pt cup[4] = {{-10, -8}, {10, -8}, {7, 9}, {-7, 9}};
    p.poly(cup_o, 4, 0xC9C2BA);
    p.poly(cup, 4, 0xFFFFFF);
    p.ellipse(0, -8, 10, 2.4f, 0xC9C2BA);
    p.ellipse(0, -8, 9, 1.8f, 0xD9893A);  // tea
    // tea bag string and its tag hanging over the rim
    p.capsule(4, -8, 6, -14, 0.7f, 0x8A6A4A);
    const Pt tag_o[4] = {{4.5f, -16.5f}, {8.5f, -16.5f}, {8.5f, -12}, {4.5f, -12}};
    const Pt tag[4] = {{5, -16}, {8, -16}, {8, -12.5f}, {5, -12.5f}};
    p.poly(tag_o, 4, 0xB89A20);
    p.poly(tag, 4, 0xFFD84A);
    // highlight on the cup
    p.capsule(-6, -3, -5, 4, 0.9f, 0xFFFFFF, 190);
    steam(p, -5, -11, 13, 2, 2400, t);
}

void cocoa_mug(const Canvas& cv, float x, float y, float s, std::uint32_t t) noexcept {
    const Pen p{cv, x, y, s};
    // handle, then the body, the cocoa surface and marshmallows
    handle(p, 9, -4, 5, 5, kMugDark, kMug);
    const Pt body_o[4] = {{-11, -8}, {11, -8}, {9.5f, 10}, {-9.5f, 10}};
    const Pt body[4] = {{-10, -7}, {10, -7}, {8.5f, 9}, {-8.5f, 9}};
    p.poly(body_o, 4, kMugDark);
    p.poly(body, 4, kMug);
    p.ellipse(0, -7, 10, 2.5f, kMugDark);
    p.ellipse(0, -7, 9, 1.9f, kCocoa);
    // a cream band near the bottom
    p.capsule(-8, 5.5f, 8, 5.5f, 0.8f, 0xFBF0DC, 200);
    // marshmallows: white with a pink tint, sitting on the cocoa
    static constexpr float kMx[3] = {-4.5f, 1.0f, 5.5f};
    static constexpr float kMy[3] = {-10.5f, -12.0f, -9.5f};
    for (int i = 0; i < 3; ++i) {
        p.circle(kMx[i], kMy[i], 3.0f, kMallowShade);
        p.circle(kMx[i], kMy[i], 2.5f, kMallow);
        p.circle(kMx[i] - 0.8f, kMy[i] - 0.9f, 0.7f, 0xFFFFFF, 200);
    }
    steam(p, -3, -15, 13, 2, 2600, t);
}

void leaf_fan(const Canvas& cv, float x, float y, float s, float angle) noexcept {
    const Pen p{cv, x, y, s};
    const float ca = std::cos(angle), sa = std::sin(angle);
    // (dx, dy) in leaf units, leaf pointing up from the stem at the origin, turned by angle
    auto R = [&](float dx, float dy, float k = 1.0f) { return Pt{(dx * ca - dy * sa) * k, (dx * sa + dy * ca) * k}; };
    const float lx[8] = {0, 7, 13, 9, 0, -9, -13, -7};
    const float ly[8] = {0, -7, -17, -27, -31, -27, -17, -7};
    Pt out[8], leaf[8];
    for (int i = 0; i < 8; ++i) {
        out[i] = R(lx[i], ly[i], 1.12f);
        leaf[i] = R(lx[i], ly[i]);
    }
    const Pt stem0 = R(0, 0), stem1 = R(0, 6);
    p.capsule(stem0.x, stem0.y, stem1.x, stem1.y, 1.4f, kLeafDark);
    p.poly(out, 8, kLeafDark);
    p.poly(leaf, 8, kLeaf);
    // midrib, side veins, shine
    const Pt mid0 = R(0, -1), mid1 = R(0, -28);
    p.capsule(mid0.x, mid0.y, mid1.x, mid1.y, 0.9f, kLeafLight);
    for (int k = 0; k < 3; ++k) {
        const float vy = -9.0f - static_cast<float>(k) * 6.0f;
        const Pt v0 = R(0, vy), vl = R(-8, vy - 6), vr = R(8, vy - 6);
        p.capsule(v0.x, v0.y, vl.x, vl.y, 0.7f, kLeafLight, 170);
        p.capsule(v0.x, v0.y, vr.x, vr.y, 0.7f, kLeafLight, 170);
    }
    const Pt shine = R(-5, -20);
    p.circle(shine.x, shine.y, 1.2f, 0xFFFFFF, 150);
}

void mushroom(const Canvas& cv, float x, float base_y, float s) noexcept {
    const Pen p{cv, x, base_y, s * 1.8f};
    // stem, then the red cap with white dots
    const Pt stem_o[4] = {{-3.6f, 0}, {3.6f, 0}, {2.8f, -8}, {-2.8f, -8}};
    const Pt stem[4] = {{-3, 0}, {3, 0}, {2.3f, -8}, {-2.3f, -8}};
    p.poly(stem_o, 4, 0xC9B9A0);
    p.poly(stem, 4, 0xF6EBDD);
    p.ellipse(0, -8, 9.6f, 6.6f, 0x8E2430);
    p.ellipse(0, -8, 9, 6, 0xD9434B);
    p.circle(-4, -10.5f, 1.6f, 0xFFFFFF);
    p.circle(3.5f, -12, 1.3f, 0xFFFFFF);
    p.circle(5.5f, -7.5f, 1.0f, 0xFFFFFF);
    p.capsule(-6, -11, -4.5f, -13, 0.8f, 0xFFFFFF, 170);  // shine
}

void closed_umbrella(const Canvas& cv, float x, float base_y, float s) noexcept {
    // The tip rests on the base at the origin; the shaft leans right. (d, o) are distances along the shaft
    // and across it.
    const Pen p{cv, x, base_y, s};
    constexpr float kLean = 0.22f;
    const float sn = std::sin(kLean), cs = std::cos(kLean);
    auto P = [&](float d, float o) { return Pt{d * sn + o * cs, -d * cs + o * sn}; };
    constexpr std::uint32_t kBlue = 0x3E7FD6, kBlueDark = 0x23508F, kBlueLight = 0x9CC7FF;
    // crook handle at the top end, on the far side from the canopy
    const Pt h0 = P(30, 0), h1 = P(33, 0), h2 = P(33, -4), h3 = P(30, -6), h4 = P(28, -4);
    p.capsule(h0.x, h0.y, h1.x, h1.y, 1.2f, kBlueDark);
    p.capsule(h1.x, h1.y, h2.x, h2.y, 1.2f, kBlueDark);
    p.capsule(h2.x, h2.y, h3.x, h3.y, 1.2f, kBlueDark);
    p.capsule(h3.x, h3.y, h4.x, h4.y, 1.2f, kBlueDark);
    const Pt sh0 = P(0, 0), sh1 = P(30, 0);
    p.capsule(sh0.x, sh0.y, sh1.x, sh1.y, 1.2f, 0x5E5E68);
    // furled canopy: a spindle around the shaft, outline first
    const float d[7] = {9, 17, 24, 28, 24, 17, 9};
    const float o[7] = {-2.2f, -4.5f, -3, 0, 3, 4.5f, 2.2f};
    Pt out[7], body[7];
    for (int i = 0; i < 7; ++i) {
        out[i] = P(d[i] + (i == 3 ? 1.5f : 0), o[i] * 1.2f);
        body[i] = P(d[i], o[i]);
    }
    p.poly(out, 7, kBlueDark);
    p.poly(body, 7, kBlue);
    const Pt wrap0 = P(20, -4.6f), wrap1 = P(20, 4.6f);
    p.capsule(wrap0.x, wrap0.y, wrap1.x, wrap1.y, 0.9f, kBlueDark);  // strap that holds it furled
    const Pt shine = P(14, -2.2f);
    p.capsule(shine.x, shine.y, P(17, -2.0f).x, P(17, -2.0f).y, 0.8f, kBlueLight, 200);
    const Pt tip = P(0, 0);
    p.circle(tip.x, tip.y + 0.5f, 1.1f, 0x5E5E68);
}

void stink_lines(const Canvas& cv, float x, float y, std::uint32_t t) noexcept {
    const Pen p{cv, x, y, 1.0f};
    constexpr std::uint32_t kStink = 0x7DBB3C;
    for (int i = 0; i < 3; ++i) {
        const float off = static_cast<float>(i - 1) * 7.0f;
        const float phase = 2 * kPi * ph(t, 1600) + static_cast<float>(i) * 2.0f;
        // a wavy line rising from the base point, fading towards its top
        float px = off, py = 0;
        for (int j = 1; j <= 6; ++j) {
            const float fy = -4.0f * static_cast<float>(j);
            const float fx = off + std::sin(phase + static_cast<float>(j) * 1.1f) * 2.6f;
            p.capsule(px, py, fx, fy, 1.4f, kStink, 225 - j * 26);
            px = fx;
            py = fy;
        }
    }
}

void thermometer(const Canvas& cv, float x, float y, float s, float level, bool hot) noexcept {
    const Pen p{cv, x, y, s};
    const float lv = std::clamp(level, 0.0f, 1.0f);
    const std::uint32_t liquid = hot ? 0xE5484D : 0x3F9BE0;
    // white rounded body with an outline, a scale on the right, bulb at the bottom
    p.capsule(0, -12, 0, 8, 3.8f, 0xB8BEC8);
    p.capsule(0, -12, 0, 8, 3.0f, 0xFFFFFF);
    p.circle(0, 10, 4.8f, 0xB8BEC8);
    p.circle(0, 10, 4.0f, 0xFFFFFF);
    // the column: from the bulb up to the level
    const float top = 8.0f - lv * 18.0f;
    p.capsule(0, 8, 0, top, 1.7f, liquid);
    p.circle(0, 10, 3.0f, liquid);
    for (int k = 0; k < 3; ++k) {
        const float ty = -6.0f + static_cast<float>(k) * 6.0f;
        p.capsule(4.5f, ty, 6.8f, ty, 0.7f, 0x8A8F99);
    }
    p.capsule(-1.6f, -8, -1.6f, 0, 0.7f, 0xFFFFFF, 200);  // shine
}

void sweat_drop(const Canvas& cv, float x, float y, float s, std::uint32_t t) noexcept {
    const Pen p{cv, x, y, s};
    // a teardrop slides down 12 px and fades out, then starts again
    const float k = ph(t, 1400);
    const float dy = k * 12.0f;
    const int a = static_cast<int>(255.0f * (1.0f - k * 0.8f));
    const float fx = 0;
    const Pt tip_o[3] = {{fx, dy - 5.2f}, {fx + 3.0f, dy}, {fx - 3.0f, dy}};
    const Pt tip[3] = {{fx, dy - 4.2f}, {fx + 2.2f, dy}, {fx - 2.2f, dy}};
    p.circle(fx, dy, 3.4f, 0x3F8FC9, a);
    p.poly(tip_o, 3, 0x3F8FC9, a);
    p.circle(fx, dy, 2.6f, 0x8ED4FF, a);
    p.poly(tip, 3, 0x8ED4FF, a);
    p.circle(fx - 1.0f, dy - 0.5f, 0.8f, 0xFFFFFF, a * 3 / 4);
}

}  // namespace screen::props
