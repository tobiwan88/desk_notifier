// screen: special-day props, see props.hpp. Pure C++.
#include "screen/core/props.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include "screen/core/draw.hpp"

namespace screen::props {
namespace {

using draw::hex;
using draw::Pt;

constexpr float kPi = 3.14159265f;

// Phase of a repeating cycle, 0..1, and a sine over it.
float ph(std::uint32_t t, std::uint32_t period) noexcept {
    return static_cast<float>(t % period) / static_cast<float>(period);
}
float wave(std::uint32_t t, std::uint32_t period) noexcept { return std::sin(2 * kPi * ph(t, period)); }

// Convex polygon in screen pixels: a darker outline grown by g px from the centroid, then the fill.
void fill_outlined(const Canvas& cv, const Pt* in, int n, std::uint32_t c, std::uint32_t dark, float g) noexcept {
    if (n < 3) return;
    Pt cen{0, 0};
    for (int i = 0; i < n; ++i) {
        cen.x += in[i].x / static_cast<float>(n);
        cen.y += in[i].y / static_cast<float>(n);
    }
    if (g > 0) {
        Pt out[16];
        for (int i = 0; i < n; ++i) {
            const float dx = in[i].x - cen.x, dy = in[i].y - cen.y;
            const float len = std::sqrt(dx * dx + dy * dy) + 1e-3f;
            out[i] = {in[i].x + dx / len * g, in[i].y + dy / len * g};
        }
        draw::polygon(cv, out, n, hex(dark));
    }
    draw::polygon(cv, in, n, hex(c));
}

// Darker shade of a 0xRRGGBB colour, for outlines of things drawn in screen pixels.
constexpr std::uint32_t shade(std::uint32_t c) noexcept { return draw::lerp_rgb(c, 0x000000, 80); }

// A heart with its top at cy - 0.55 r, the tip at cy + 1.25 r, in screen pixels, with a darker outline.
void heart(const Canvas& cv, float cx, float cy, float r, std::uint32_t c, int a) noexcept {
    for (const float k : {1.18f, 1.0f}) {
        const float rr = r * k;
        const std::uint32_t col = k > 1 ? shade(c) : c;
        draw::circle(cv, cx - 0.5f * rr, cy, 0.55f * rr, hex(col), a);
        draw::circle(cv, cx + 0.5f * rr, cy, 0.55f * rr, hex(col), a);
        const Pt tri[3] = {{cx - 1.02f * rr, cy + 0.1f * rr}, {cx + 1.02f * rr, cy + 0.1f * rr}, {cx, cy + 1.25f * rr}};
        draw::polygon(cv, tri, 3, hex(col), a);
    }
}

// A small rectangle w x h (half sizes) turned by ang, drawn as a confetti piece.
void piece(const Canvas& cv, float cx, float cy, float w, float h, float ang, std::uint32_t c) noexcept {
    const float co = std::cos(ang), si = std::sin(ang);
    const float dx[4] = {-w, w, w, -w}, dy[4] = {-h, -h, h, h};
    Pt q[4];
    for (int i = 0; i < 4; ++i) q[i] = {cx + dx[i] * co - dy[i] * si, cy + dx[i] * si + dy[i] * co};
    fill_outlined(cv, q, 4, c, shade(c), 0.5f);
}

// Design units: u to the right, v up, origin at the middle of the bottom edge; 1 unit = 1 px at s = 1.
// Every solid shape is drawn twice: a slightly larger shape in the darker colour, then the fill.
struct Pen {
    const Canvas& cv;
    float x, by, s;
    float X(float u) const noexcept { return x + u * s; }
    float Y(float v) const noexcept { return by - v * s; }

    void oval(float u, float v, float rx, float ry, std::uint32_t c, std::uint32_t dark, float ang = 0,
              float g = 0.8f) const noexcept {
        draw::ellipse(cv, X(u), Y(v), (rx + g) * s, (ry + g) * s, hex(dark), ang);
        draw::ellipse(cv, X(u), Y(v), rx * s, ry * s, hex(c), ang);
    }
    void stroke(float u0, float v0, float u1, float v1, float r, std::uint32_t c, std::uint32_t dark,
                float g = 0.8f) const noexcept {
        draw::capsule(cv, X(u0), Y(v0), X(u1), Y(v1), (r + g) * s, hex(dark));
        draw::capsule(cv, X(u0), Y(v0), X(u1), Y(v1), r * s, hex(c));
    }
    // Convex polygon in design units. g > 0 grows a darker outline by g units (grown from the centroid).
    void poly(std::initializer_list<Pt> pts, std::uint32_t c, std::uint32_t dark, float g = 0.8f) const noexcept {
        Pt in[16];
        int n = 0;
        for (const Pt q : pts) {
            if (n < 16) in[n++] = {X(q.x), Y(q.y)};
        }
        fill_outlined(cv, in, n, c, dark, g * s);
    }
    void disc(float u, float v, float r, std::uint32_t c, std::uint32_t dark, float g = 0.8f) const noexcept {
        draw::circle(cv, X(u), Y(v), (r + g) * s, hex(dark));
        draw::circle(cv, X(u), Y(v), r * s, hex(c));
    }
    // small white glint
    void glint(float u, float v, float r, int a = 170) const noexcept {
        draw::circle(cv, X(u), Y(v), r * s, hex(0xFFFFFF), a);
    }
};

// Fur-free palette for the special-day props.
constexpr std::uint32_t kPumpkin = 0xFF8A1F, kPumpkinDark = 0xC85A10, kPumpkinShade = 0xFFA64A,
                        kFace = 0x3A1A05, kFaceLit = 0xFFE27A, kStem = 0x6E9A34, kStemDark = 0x3E5A1C;
constexpr std::uint32_t kLeaf = 0x4AA45E, kLeafLight = 0x6CC27A, kLeafDark = 0x25602E, kCandle = 0xD63A3A,
                        kCandleDark = 0x9A2030, kFlame = 0xFF9A1E, kFlameDark = 0xC8650F, kCore = 0xFFF2A0;
constexpr std::uint32_t kBoot = 0xC8252F, kBootDark = 0x7E1520, kFur = 0xF6F3EE, kFurShade = 0xC9C2BA,
                        kOrange = 0xFF9A2E, kOrangeDark = 0xC86A14, kNut = 0xB07A40, kNutDark = 0x6E4220;
constexpr std::uint32_t kPot = 0xD9824A, kPotDark = 0x8A4A24, kPotRim = 0xE8985E, kFir = 0x2E8B57,
                        kFirDark = 0x1B5A36, kFirLight = 0x44A86E, kStar = 0xFFD23A, kStarDark = 0xC89A12,
                        kPresent = 0x3AA0FF, kPresentDark = 0x1F6FC0, kRibbon = 0xFF5A7A;
constexpr std::uint32_t kCakePink = 0xFF9EC4, kCakePinkDark = 0xE0679E, kIcing = 0xFFF6F8, kIcingDark = 0xD9C4CC,
                        kCandleCream = 0xFFF4DC, kCandleCreamDark = 0xC9B48A, kAcorn = 0xB37B40,
                        kAcornDark = 0x6E4220, kAcornCap = 0x6E4220, kAcornCapDark = 0x3E2410;
constexpr std::uint32_t kPretzel = 0xB5703A, kPretzelDark = 0x7A4220, kEggPink = 0xFFB3C6,
                        kEggPinkDark = 0xD97E98, kEggBlue = 0xB8E0FF, kEggBlueDark = 0x7AAAD6,
                        kEggYellow = 0xFFF3A3, kEggYellowDark = 0xD6C866, kEggGreen = 0xC5F5C0,
                        kEggGreenDark = 0x7CB878, kWicker = 0xC8914F, kWickerDark = 0x7A4E22, kWickerRim = 0xDDA86A;
constexpr std::uint32_t kStick = 0x9A6A3A, kLantern = 0xFFB040, kLanternCore = 0xFFF6C8, kLanternCap = 0x8A4A20;
constexpr std::uint32_t kHeartRed = 0xE8384F, kFlagColour[5] = {0xFF6B6B, 0xFFD23A, 0x5CC8FF, 0x7BE07B, 0xB48CFF};
constexpr std::uint32_t kConfetti[6] = {0xFF5A7A, 0xFFD23A, 0x3AA0FF, 0x5CD67A, 0xB48CFF, 0xFF9A2E};
constexpr std::uint32_t kSpark[5] = {0xFF5A7A, 0xFFD23A, 0x5CC8FF, 0x7BE07B, 0xD9A6FF};
constexpr std::uint32_t kPartyHat = 0xFF6BB5, kPom = 0xFFD23A, kBrimBlue = 0x5CC8FF, kSantaRed = 0xE0505E,
                        kGold = 0xFFD23A, kGem = 0xE8384F, kGemBlue = 0x3AA0FF;
constexpr std::uint32_t kRose = 0xE0364B, kRoseDark = 0x9A1A2E, kRoseLight = 0xFF6A7A, kVase = 0x8EC5F0,
                        kVaseDark = 0x4F86B8;

}  // namespace

void pumpkin(const Canvas& cv, float x, float base_y, float s, std::uint32_t t, bool lit) noexcept {
    const Pen p{cv, x, base_y, s};
    const float f = 1.0f + 0.07f * wave(t, 260) + 0.04f * wave(t, 137);  // flame flicker
    if (lit) {  // glow halo behind the pumpkin
        draw::circle(cv, p.X(0), p.Y(14), 26 * s * f, hex(0xFFC040), 40);
        draw::circle(cv, p.X(0), p.Y(14), 19 * s * f, hex(0xFFC040), 60);
    }
    p.stroke(0, 25, 2, 32, 2.2f, kStem, kStemDark);
    p.oval(0, 14, 16, 13, kPumpkin, kPumpkinDark);
    p.oval(-8, 15, 4.5f, 8, kPumpkinShade, kPumpkinShade, 0, 0);  // lit side of the body
    // carved face: eyes and a trapezoid grin, glowing when lit
    const std::uint32_t face = lit ? kFaceLit : kFace;
    p.poly({{-12, 21}, {-6, 21}, {-9, 15}}, face, face, 0);
    p.poly({{6, 21}, {12, 21}, {9, 15}}, face, face, 0);
    p.poly({{-10, 10}, {10, 10}, {5, 4}, {-5, 4}}, face, face, 0);
    p.glint(-11, 23, 2.0f, 170);
}

void advent_wreath(const Canvas& cv, float x, float base_y, float s, std::uint32_t t, int lit) noexcept {
    const Pen p{cv, x, base_y, s};
    constexpr float kRing = 10.0f, kTube = 3.5f, kCentre = 13.0f;
    draw::arc(cv, p.X(0), p.Y(kCentre), kRing * s, 0, 2 * kPi, (kTube + 0.8f) * s, hex(kLeafDark), 256, 16);
    draw::arc(cv, p.X(0), p.Y(kCentre), kRing * s, 0, 2 * kPi, kTube * s, hex(kLeaf), 256, 16);
    for (int i = 0; i < 12; ++i) {  // needle leaves along the ring, tangent to it
        const float a = 2 * kPi * static_cast<float>(i) / 12.0f;
        const float u = kRing * std::cos(a), v = kCentre + kRing * std::sin(a);
        p.oval(u, v, 3.6f, 1.8f, i % 2 == 0 ? kLeafLight : kLeaf, kLeafDark, kPi / 2 - a);
    }
    constexpr float kCandleU[4] = {-9.0f, -3.0f, 3.0f, 9.0f};
    for (int i = 0; i < 4; ++i) {
        const float u = kCandleU[i];
        p.stroke(u, 11, u, 21, 1.8f, kCandle, kCandleDark);
        p.glint(u - 0.7f, 18, 0.7f, 150);
        if (i >= lit) {  // unlit: just a wick
            draw::circle(cv, p.X(u), p.Y(22.8f), 0.9f * s, hex(0x333333));
            continue;
        }
        const auto k = static_cast<std::uint32_t>(i);
        const float fl = wave(t, 170 + 41 * k), fl2 = wave(t, 230 + 37 * k);
        const float ry = 2.8f + 0.5f * fl, tilt = 0.12f * fl2;
        draw::circle(cv, p.X(u), p.Y(25), 6.5f * s, hex(0xFFC040), 40);
        p.oval(u, 24.5f, 1.8f, ry, kFlame, kFlameDark, tilt);
        p.oval(u, 23.8f, 0.8f, ry * 0.55f, kCore, kCore, tilt, 0);
    }
}

void boot(const Canvas& cv, float x, float base_y, float s) noexcept {
    const Pen p{cv, x, base_y, s};
    // foot and shaft, red leather
    p.poly({{-9, 0}, {17, 0}, {17, 4}, {12, 8}, {-9, 9}}, kBoot, kBootDark);
    p.poly({{-9, 8}, {4, 8}, {4, 29}, {-9, 29}}, kBoot, kBootDark);
    p.glint(-6, 22, 1.6f, 150);
    // white fur cuff over the top of the shaft
    p.poly({{-10, 23}, {5, 23}, {5, 28}, {-10, 28}}, kFur, kFurShade, 0.6f);
    // orange and two nuts peeking out of the top
    p.disc(-3, 31, 4.2f, kOrange, kOrangeDark, 0.7f);
    p.oval(2, 31.5f, 2.4f, 1.8f, kNut, kNutDark, -0.5f);
    p.oval(-7, 32, 2.2f, 1.7f, kNut, kNutDark, 0.4f);
}

void xmas_tree(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept {
    const Pen p{cv, x, base_y, s};
    // pot, then the trunk
    p.poly({{-7, 0}, {7, 0}, {8, 6}, {-8, 6}}, kPot, kPotDark);
    p.poly({{-9, 6}, {9, 6}, {9, 8}, {-9, 8}}, kPotRim, kPotDark, 0.5f);
    // three tiers of fir, each a triangle with a darker outline
    p.poly({{-11, 8}, {11, 8}, {0, 27}}, kFir, kFirDark);
    p.poly({{-8, 16}, {8, 16}, {0, 33}}, kFir, kFirDark, 0);  // (overlaps the top tier)
    p.poly({{-6, 22}, {6, 22}, {0, 30}}, kFirLight, kFirDark, 0);
    // yellow star on top
    draw::circle(cv, p.X(0), p.Y(31.5f), 4.5f * s, hex(0xFFD23A), 60);
    p.poly({{0, 36}, {1.3f, 33}, {4, 33}, {2, 31.4f}, {2.5f, 28.8f}, {0, 30.4f}, {-2.5f, 28.8f}, {-2, 31.4f},
            {-4, 33}, {-1.3f, 33}},
           kStar, kStarDark, 0.6f);
    // twinkling lights: a few coloured dots, each blinking on its own phase
    constexpr float kLightU[7] = {-6, 3, -3, 6, -1, 4, -8};
    constexpr float kLightV[7] = {11, 13, 18, 16, 24, 22, 9};
    constexpr std::uint32_t kLightC[7] = {0xE8384F, 0x3AA0FF, 0xFFD23A, 0xE8384F, 0x3AA0FF, 0xFFD23A, 0x7BE07B};
    for (int i = 0; i < 7; ++i) {
        const auto k = static_cast<std::uint32_t>(i);
        const float on = 0.5f + 0.5f * wave(t, 900 + 130 * k);
        draw::circle(cv, p.X(kLightU[i]), p.Y(kLightV[i]), 1.9f * s, hex(kLightC[i]), 120 + static_cast<int>(136 * on));
    }
    // a small wrapped present beside the tree
    p.poly({{11, 0}, {22, 0}, {22, 7}, {11, 7}}, kPresent, kPresentDark);
    p.poly({{15.5f, 0}, {17.5f, 0}, {17.5f, 7}, {15.5f, 7}}, kRibbon, kRibbon, 0);
    p.poly({{11, 7}, {22, 7}, {22, 8.5f}, {11, 8.5f}}, kRibbon, kRibbon, 0);
    p.glint(13, 5, 1.0f, 170);
}

void cake(const Canvas& cv, float x, float base_y, float s, std::uint32_t t, bool acorn) noexcept {
    const Pen p{cv, x, base_y, s};
    // bottom tier (pink) and top tier (white), with a few drips of icing
    p.poly({{-14, 0}, {14, 0}, {14, 7}, {11, 9}, {-11, 9}, {-14, 7}}, kCakePink, kCakePinkDark);
    p.poly({{-9, 9}, {9, 9}, {9, 16}, {6, 18}, {-6, 18}, {-9, 16}}, kIcing, kIcingDark);
    p.disc(-11, 6.5f, 1.6f, kCakePink, kCakePinkDark, 0.5f);
    p.disc(4, 7.5f, 1.4f, kIcing, kIcingDark, 0.5f);
    p.disc(9.5f, 14.5f, 1.3f, kCakePink, kCakePinkDark, 0.5f);
    p.oval(0, 17.6f, 6, 1.1f, kIcing, kIcingDark, 0, 0.3f);
    p.glint(-5, 12, 1.3f, 170);
    if (acorn) {  // a big acorn on top: cap, nut and stalk
        p.oval(0, 22.5f, 4.2f, 4.8f, kAcorn, kAcornDark);
        p.oval(0, 26.5f, 4.6f, 2.2f, kAcornCap, kAcornCapDark);
        p.stroke(0, 27.5f, 0.8f, 30.5f, 0.9f, kAcornCap, kAcornCapDark, 0.4f);
        p.glint(-1.5f, 22, 1.2f, 150);
        return;
    }
    // one candle with a flickering flame
    p.stroke(0, 18, 0, 26, 1.2f, kCandleCream, kCandleCreamDark, 0.5f);
    const float fl = wave(t, 180), fl2 = wave(t, 260);
    draw::circle(cv, p.X(0), p.Y(29), 5.5f * s, hex(0xFFC040), 40);
    p.oval(0, 28.5f, 1.6f, 2.6f + 0.4f * fl, kFlame, kFlameDark, 0.1f * fl2);
    p.oval(0, 27.8f, 0.7f, 1.4f + 0.2f * fl, kCore, kCore, 0.1f * fl2, 0);
}

void pretzel(const Canvas& cv, float x, float base_y, float s) noexcept {
    // a Bavarian Brezn: a fat belly at the bottom, two thinner loops on top, the arms crossing in the middle
    constexpr float kPi = 3.14159265f;
    const float u = 10 * s, w = 0.2f * u;
    const auto dough = draw::hex(0xA8642C), crust = draw::hex(0x6E3A14), salt = draw::hex(0xFAF6EE);
    struct Arc { float cx, cy, r, a0, a1, w; };
    const Arc arcs[3] = {
        {x, base_y - 1.4f * u, 1.25f * u, -0.12f * kPi, 1.12f * kPi, w * 1.5f},          // belly, through the bottom
        {x - 0.78f * u, base_y - 2.1f * u, 0.72f * u, 1.0f * kPi, 2.3f * kPi, w},          // left loop, over the top
        {x + 0.78f * u, base_y - 2.1f * u, 0.72f * u, 0.0f, -1.3f * kPi, w},               // right loop
    };
    const draw::Pt cross[2][2] = {{{x - 0.3f * u, base_y - 1.5f * u}, {x + 0.85f * u, base_y - 0.45f * u}},
                                  {{x + 0.3f * u, base_y - 1.5f * u}, {x - 0.85f * u, base_y - 0.45f * u}}};
    for (int pass = 0; pass < 2; ++pass) {  // crust outline, then the dough
        const auto c = pass == 0 ? crust : dough;
        const float extra = pass == 0 ? 0.09f * u : 0.0f;
        for (const Arc& a : arcs) draw::arc(cv, a.cx, a.cy, a.r, a.a0, a.a1, a.w + extra, c, 256, 14);
        for (const auto& l : cross) draw::capsule(cv, l[0].x, l[0].y, l[1].x, l[1].y, w + extra, c);
    }
    draw::arc(cv, x, base_y - 1.35f * u, 1.25f * u, 0.15f * kPi, 0.85f * kPi, w * 0.35f, draw::hex(0xC98A4E), 200, 10);  // shine
    static constexpr float kSalt[][2] = {{-1.0f, 1.0f}, {-0.4f, 0.12f}, {0.5f, 0.15f}, {1.05f, 0.9f}, {-0.95f, 2.3f},
                                         {-0.45f, 2.7f}, {0.5f, 2.65f}, {1.0f, 2.2f}};
    for (const auto& k : kSalt) draw::circle(cv, x + k[0] * u, base_y - k[1] * u, 0.09f * u + 0.3f, salt);
}

void egg_basket(const Canvas& cv, float x, float base_y, float s) noexcept {
    const Pen p{cv, x, base_y, s};
    // eggs sit in the basket, the front of the basket covers their bottoms
    p.oval(-7, 13, 3.8f, 4.8f, kEggPink, kEggPinkDark, -0.15f);
    p.oval(0, 16, 3.9f, 5.0f, kEggBlue, kEggBlueDark, 0.1f);
    p.oval(7, 13, 3.8f, 4.8f, kEggYellow, kEggYellowDark, 0.15f);
    p.oval(-2, 9.5f, 3.4f, 4.2f, kEggGreen, kEggGreenDark, -0.1f);
    p.glint(-8.5f, 15.5f, 1.0f, 170);
    p.glint(-1.5f, 18, 1.0f, 170);
    // the basket: a bowl with a rim
    p.poly({{-14, 9}, {14, 9}, {11, 0}, {-11, 0}}, kWicker, kWickerDark);
    p.poly({{-15, 10}, {15, 10}, {15, 12}, {-15, 12}}, kWickerRim, kWickerDark, 0.6f);
}

void vase_rose(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept {
    const Pen p{cv, x, base_y, s};
    const float sway = wave(t, 2600);       // the rose sways from its vase
    const float hx = 1.6f * sway, hy = 27.0f + 0.4f * wave(t, 1300);
    // stem and two leaves
    p.stroke(0, 14, hx, hy - 4, 0.9f, kStem, kStemDark, 0.5f);
    p.oval(hx * 0.4f - 3, hy - 9, 3, 1.5f, kLeaf, kLeafDark, 0.5f + 0.1f * sway);
    p.oval(hx * 0.5f + 2.5f, hy - 12, 2.8f, 1.4f, kLeafLight, kLeafDark, -0.5f + 0.1f * sway);
    // the rose: a red head with a lighter core
    p.disc(hx, hy, 4.6f, kRose, kRoseDark, 0.8f);
    p.disc(hx + 0.4f, hy + 0.4f, 2.4f, kRoseLight, kRoseDark, 0);
    p.glint(hx - 1.8f, hy + 2.2f, 0.8f, 170);
    // vase: pale blue, narrow neck
    p.poly({{-4.5f, 0}, {4.5f, 0}, {7, 9}, {3, 14}, {-3, 14}, {-7, 9}}, kVase, kVaseDark);
    p.poly({{-4, 13}, {4, 13}, {3.5f, 15}, {-3.5f, 15}}, kVase, kVaseDark, 0.6f);
    p.glint(-4.5f, 6, 1.2f, 150);
}

void lantern(const Canvas& cv, float x, float top_y, float s, std::uint32_t t) noexcept {
    const float swing = 0.12f * wave(t, 2200);
    const float px = x, py = top_y + 8 * s;             // pivot: the end of the stick
    const float cx = px + std::sin(swing) * 16 * s, cy = py + std::cos(swing) * 16 * s;  // lantern middle
    const float glow = 1.0f + 0.08f * wave(t, 170) + 0.05f * wave(t, 97);
    draw::circle(cv, cx, cy, 26 * s * glow, hex(0xFFC040), 45);
    // stick (wood) and string
    draw::capsule(cv, x, top_y, px, py, 1.6f * s, hex(shade(kStick)));
    draw::capsule(cv, x, top_y, px, py, 1.1f * s, hex(kStick));
    draw::capsule(cv, px, py, cx, cy - 10 * s, 0.5f * s, hex(0x4A3A2A));
    // paper lantern: darker rim, lit paper, bright core, caps top and bottom
    draw::ellipse(cv, cx, cy, 10 * s, 11 * s, hex(shade(kLantern)), swing);
    draw::ellipse(cv, cx, cy, 9 * s, 10 * s, hex(kLantern), swing);
    draw::ellipse(cv, cx, cy, 5.5f * s * glow, 6.5f * s * glow, hex(kLanternCore), swing, 200);
    draw::capsule(cv, cx - 5 * s, cy - 10.5f * s, cx + 5 * s, cy - 10.5f * s, 1.5f * s, hex(kLanternCap));
    draw::capsule(cv, cx - 5 * s, cy + 10.5f * s, cx + 5 * s, cy + 10.5f * s, 1.5f * s, hex(kLanternCap));
    draw::circle(cv, cx - 4 * s, cy - 4 * s, 1.5f * s, hex(0xFFFFFF), 150);
}

void bunting(const Canvas& cv, float y, std::uint32_t t, bool hearts) noexcept {
    constexpr int kFlags = 8;
    constexpr float kX0 = 20, kX1 = 220, kSag = 16;
    auto sx = [](int i) { return kX0 + (kX1 - kX0) * static_cast<float>(i) / kFlags; };
    auto sy = [y](float xx) { return y + kSag * std::sin(kPi * (xx - kX0) / (kX1 - kX0)); };
    // flags (or hearts) hang from the string, each swaying on its own phase
    for (int i = 0; i < kFlags; ++i) {
        const float x0 = sx(i), x1 = sx(i + 1), mid = (x0 + x1) / 2;
        const float sway = 1.6f * wave(t, 1400 + 110 * static_cast<std::uint32_t>(i));
        if (hearts) {
            heart(cv, mid + sway, sy(mid) + 5, 3.8f, kHeartRed, 256);
            continue;
        }
        const Pt tri[3] = {{x0, sy(x0)}, {x1, sy(x1)}, {mid + sway, sy(mid) + 15}};
        const std::uint32_t c = kFlagColour[i % 5];
        fill_outlined(cv, tri, 3, c, shade(c), 0.8f);
    }
    // the string over the top of the flags
    for (int i = 0; i < kFlags; ++i) {
        draw::capsule(cv, sx(i), sy(sx(i)), sx(i + 1), sy(sx(i + 1)), 1.0f, hex(0x6B4A2A));
    }
}

void confetti(const Canvas& cv, std::uint32_t t) noexcept {
    // pieces lying on the desk, turned differently
    for (std::uint32_t k = 0; k < 18; ++k) {
        const float cx = 14 + static_cast<float>((k * 53) % 212);
        const float cy = 180 + static_cast<float>((k * 37) % 56);
        const float ang = static_cast<float>((k * 29) % 180) * kPi / 180.0f;
        piece(cv, cx, cy, 4.0f, 1.8f, ang, kConfetti[k % 6]);
    }
    // a few still falling, slowly, drifting side to side
    for (std::uint32_t k = 0; k < 5; ++k) {
        const float fall = std::fmod(static_cast<float>(t) * 0.02f + static_cast<float>(k) * 47.0f, 176.0f) - 4.0f;
        const float cx = 30 + static_cast<float>((k * 61) % 180) + 6.0f * wave(t, 1700 + 200 * k);
        piece(cv, cx, fall, 3.4f, 1.6f, static_cast<float>(t) / 500.0f + static_cast<float>(k), kConfetti[(k + 2) % 6]);
    }
}

void hearts_rising(const Canvas& cv, float x, float y, std::uint32_t t) noexcept {
    for (std::uint32_t i = 0; i < 4; ++i) {
        const float p = std::fmod(static_cast<float>(t) / 2400.0f + 0.25f * static_cast<float>(i), 1.0f);
        const float hx = x + 7.0f * std::sin(2 * kPi * (p * 1.3f + 0.37f * static_cast<float>(i)));
        const float hy = y - 70.0f * p;
        const int a = static_cast<int>(230.0f * (1.0f - p));
        heart(cv, hx, hy, 4.4f + 1.0f * p, kHeartRed, a);
    }
}

void fireworks(const Canvas& cv, const Win& w, std::uint32_t t) noexcept {
    constexpr std::uint32_t kPeriod = 2100, kStagger = 700;
    constexpr std::uint32_t kBursts = 3;
    constexpr int kSparks = 12;
    const float bw = w.x1 - w.x0, bh = w.y1 - w.y0;
    for (std::uint32_t b = 0; b < kBursts; ++b) {
        const std::uint32_t tb = t + b * kStagger;
        const std::uint32_t k = tb / kPeriod;  // every new cycle, the burst goes off somewhere else
        const float p = ph(tb, kPeriod);       // 0..1 within the burst
        const std::uint32_t h = k * 2654435761u + b * 40503u;
        const float cx = w.x0 + bw * (0.25f + 0.5f * static_cast<float>(h % 1000u) / 1000.0f);
        const float cy = w.y0 + bh * (0.3f + 0.4f * static_cast<float>((h / 1000u) % 1000u) / 1000.0f);
        const float reach = 0.4f * std::min(bw, bh) * (1.0f - (1.0f - p) * (1.0f - p));
        const int alpha = static_cast<int>(256.0f * std::clamp((1.0f - p) * 2.0f, 0.0f, 1.0f));
        for (int i = 0; i < kSparks; ++i) {
            const float ang = 2 * kPi * static_cast<float>(i) / kSparks + 0.2f * static_cast<float>(h % 7);
            const float sx = cx + reach * std::cos(ang), sy = cy + reach * std::sin(ang);
            // only sparks inside the window: anything outside it is left out, not clipped
            if (sx < w.x0 + 2 || sx > w.x1 - 2 || sy < w.y0 + 2 || sy > w.y1 - 2) continue;
            draw::circle(cv, sx, sy, 2.3f, hex(kSpark[(static_cast<std::uint32_t>(i) + b) % 5]), alpha);
        }
    }
}

void party_hat(const Canvas& cv, float cx, float brim_y, float width, std::uint32_t t) noexcept {
    const float w = width, h = 1.05f * w;
    const float apex_x = cx + 0.08f * w * wave(t, 1600), apex_y = brim_y - h;
    const Pt cone[3] = {{cx - 0.36f * w, brim_y}, {cx + 0.36f * w, brim_y}, {apex_x, apex_y}};
    fill_outlined(cv, cone, 3, kPartyHat, shade(kPartyHat), 0.9f);
    // white dots marching up the cone, alternating sides
    for (int i = 0; i < 4; ++i) {
        const float f = 0.22f + 0.2f * static_cast<float>(i);
        const float side = i % 2 == 0 ? -0.5f : 0.5f;
        const float mx = cx + (apex_x - cx) * f + side * 0.36f * w * (1 - f);
        draw::circle(cv, mx, brim_y - f * h, 0.07f * w, hex(0xFFFFFF), 230);
    }
    draw::circle(cv, apex_x, apex_y, 0.16f * w, hex(shade(kPom)));
    draw::circle(cv, apex_x, apex_y, 0.13f * w, hex(kPom));
    draw::ellipse(cv, cx, brim_y, 0.42f * w + 0.8f, 0.08f * w + 0.8f, hex(shade(kBrimBlue)));
    draw::ellipse(cv, cx, brim_y, 0.42f * w, 0.08f * w, hex(kBrimBlue));
}

void santa_hat(const Canvas& cv, float cx, float brim_y, float width) noexcept {
    const float w = width;
    // the floppy red cap, its tip falling over to the right
    const float tipx = cx + 0.5f * w, tipy = brim_y - 1.0f * w;
    const Pt body[4] = {{cx - 0.42f * w, brim_y}, {cx - 0.3f * w, brim_y - 0.55f * w}, {tipx, tipy},
                        {cx + 0.42f * w, brim_y}};
    fill_outlined(cv, body, 4, kSantaRed, shade(kSantaRed), 0.9f);
    draw::circle(cv, tipx, tipy, 0.16f * w + 0.8f, hex(kFurShade));
    draw::circle(cv, tipx, tipy, 0.16f * w, hex(0xFFFFFF));
    // the white fur brim
    draw::ellipse(cv, cx, brim_y, 0.48f * w + 0.8f, 0.11f * w + 0.8f, hex(kFurShade));
    draw::ellipse(cv, cx, brim_y, 0.48f * w, 0.11f * w, hex(kFur));
}

void crown(const Canvas& cv, float cx, float brim_y, float width, std::uint32_t t) noexcept {
    const float w = width;
    // band, then five points, each a convex spike standing on the band's top edge
    const Pt band[4] = {{cx - 0.5f * w, brim_y}, {cx + 0.5f * w, brim_y},
                        {cx + 0.5f * w, brim_y - 0.22f * w}, {cx - 0.5f * w, brim_y - 0.22f * w}};
    fill_outlined(cv, band, 4, kGold, shade(kGold), 0.9f);
    constexpr float kTipU[5] = {-0.4f, -0.2f, 0.0f, 0.2f, 0.4f};
    constexpr float kTipV[5] = {0.5f, 0.6f, 0.7f, 0.6f, 0.5f};
    for (int i = 0; i < 5; ++i) {
        const float u = kTipU[i] * w, top = brim_y - kTipV[i] * w;
        const Pt spike[3] = {{cx + u - 0.11f * w, brim_y - 0.2f * w}, {cx + u + 0.11f * w, brim_y - 0.2f * w},
                             {cx + u, top}};
        fill_outlined(cv, spike, 3, kGold, shade(kGold), 0.9f);
        draw::circle(cv, cx + u, top, 0.07f * w + 0.6f, hex(shade(kGem)));
        draw::circle(cv, cx + u, top, 0.07f * w, hex(i % 2 == 0 ? kGem : kGemBlue));
    }
    // jewels on the band
    const float jx[3] = {-0.25f, 0.0f, 0.25f};
    for (int i = 0; i < 3; ++i) {
        const std::uint32_t jc = i == 1 ? kGemBlue : kGem;
        draw::circle(cv, cx + jx[i] * w, brim_y - 0.11f * w, 0.07f * w + 0.6f, hex(shade(jc)));
        draw::circle(cv, cx + jx[i] * w, brim_y - 0.11f * w, 0.07f * w, hex(jc));
    }
    // a sparkle that twinkles on two of the points in turn
    const float s1 = std::max(0.0f, wave(t, 1800)), s2 = std::max(0.0f, wave(t, 1800) * -1.0f);
    draw::sparkle(cv, cx - 0.4f * w, brim_y - 0.82f * w, 0.14f * w * (0.4f + 0.6f * s1), hex(0xFFFFFF),
                  static_cast<int>(256.0f * s1));
    draw::sparkle(cv, cx + 0.4f * w, brim_y - 0.82f * w, 0.14f * w * (0.4f + 0.6f * s2), hex(0xFFFFFF),
                  static_cast<int>(256.0f * s2));
}

}  // namespace screen::props
