// screen: drawing the bunny, the stag, the lucky cat (and her dog) and the penguin, and their home page.
// Shapes only (circles, ellipses, polygons, thick lines), like the squirrel. Pure C++.
#include "screen/core/mascot_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "screen/core/day.hpp"
#include "screen/core/draw.hpp"
#include "screen/core/pages.hpp"
#include "screen/core/props.hpp"
#include "screen/core/view.hpp"

namespace screen {

namespace {

using draw::hex;
using draw::Pt;

constexpr float kPi = 3.14159265f;
constexpr float kDeg = kPi / 180.0f;
float ph(std::uint32_t t, std::uint32_t period) noexcept { return static_cast<float>(t % period) / static_cast<float>(period); }
float wave(std::uint32_t t, std::uint32_t period) noexcept { return std::sin(2 * kPi * ph(t, period)); }

// Design coordinates (the 240-unit drawings, (120, 120) at (cx, cy)) to the screen.
struct P {
    const Canvas& cv;
    float cx, cy, s;
    float X(float x) const { return cx + (x - 120.0f) * s; }
    float Y(float y) const { return cy + (y - 120.0f) * s; }
    void circle(float x, float y, float r, std::uint32_t c, int a = 256) const { draw::circle(cv, X(x), Y(y), r * s, hex(c), a); }
    void ellipse(float x, float y, float rx, float ry, std::uint32_t c, float deg = 0, int a = 256) const {
        draw::ellipse(cv, X(x), Y(y), rx * s, ry * s, hex(c), deg * kDeg, a);
    }
    // an ellipse with a thin outline (white animals on a light background)
    void oellipse(float x, float y, float rx, float ry, std::uint32_t fill, std::uint32_t line, float deg = 0) const {
        if (line != fill) ellipse(x, y, rx + 1.4f, ry + 1.4f, line, deg);
        ellipse(x, y, rx, ry, fill, deg);
    }
    void ocircle(float x, float y, float r, std::uint32_t fill, std::uint32_t line) const {
        if (line != fill) circle(x, y, r + 1.4f, line);
        circle(x, y, r, fill);
    }
    void line(float x0, float y0, float x1, float y1, float r, std::uint32_t c, int a = 256) const {
        draw::capsule(cv, X(x0), Y(y0), X(x1), Y(y1), std::max(r * s, 0.45f), hex(c), a);
    }
    void arc(float x, float y, float R, float a0, float a1, float w, std::uint32_t c) const {
        draw::arc(cv, X(x), Y(y), R * s, a0, a1, std::max(w * s, 0.5f), hex(c));
    }
    void poly(const Pt* d, int n, std::uint32_t c, int a = 256) const {
        Pt q[16];
        n = std::min(n, 16);
        for (int i = 0; i < n; ++i) q[i] = {X(d[i].x), Y(d[i].y)};
        draw::polygon(cv, q, n, hex(c), a);
    }
    void box(float x0, float y0, float x1, float y1, std::uint32_t c, int a = 256) const {
        const Pt q[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
        poly(q, 4, c, a);
    }
    // the upper half of an ellipse (a knitted hat, a hat crown)
    void dome(float x, float y, float rx, float ry, std::uint32_t c) const {
        Pt q[13];
        for (int i = 0; i < 13; ++i) {
            const float a = kPi + kPi * static_cast<float>(i) / 12.0f;
            q[i] = {x + std::cos(a) * rx, y + std::sin(a) * ry};
        }
        poly(q, 13, c);
    }
};

constexpr std::uint32_t kHatRed = 0xD9584A, kKnitCream = 0xF3EDE4, kScarfBlue = 0x3E7CB1;

void eye(const P& p, float x, float y, float r, Face f, std::uint32_t ink, bool blinking) {
    switch (f) {
        case Face::Open:
            if (blinking) { p.line(x - r, y, x + r, y, 1.2f, ink); break; }
            p.circle(x, y, r, ink);
            p.circle(x + r * 0.35f, y - r * 0.38f, r * 0.38f, 0xFFFFFF);
            break;
        case Face::Worried:
            p.circle(x, y + r * 0.1f, r * 0.85f, ink);
            p.circle(x + r * 0.3f, y - r * 0.25f, r * 0.3f, 0xFFFFFF);
            break;
        case Face::Happy: p.arc(x, y + r * 0.7f, r * 1.15f, kPi * 1.18f, kPi * 1.82f, 1.4f, ink); break;
        case Face::Half:
            p.ellipse(x, y + r * 0.35f, r, r * 0.5f, ink);
            p.line(x - r * 1.1f, y - r * 0.15f, x + r * 1.1f, y - r * 0.15f, 1.0f, ink);
            break;
        case Face::Asleep: p.arc(x, y - r * 0.6f, r * 1.1f, kPi * 0.18f, kPi * 0.82f, 1.4f, ink); break;
    }
}

// worried brows, sloping up towards the middle
void brows(const P& p, float lx, float rx, float y, float w, std::uint32_t ink) {
    p.line(lx - w, y, lx + w, y - w * 0.6f, 1.1f, ink);
    p.line(rx + w, y, rx - w, y - w * 0.6f, 1.1f, ink);
}

bool blink_now(const MascotLook& l, std::uint32_t t) {
    return l.blink && l.face == Face::Open && ph(t + static_cast<std::uint32_t>(l.who) * 911u, 3700) > 0.955f;
}

void knitted_hat(const P& p, float x, float y, float rx, float ry) {
    p.dome(x, y, rx, ry, kHatRed);
    for (int i = -2; i <= 2; ++i) p.line(x + static_cast<float>(i) * rx * 0.33f, y - ry * 0.9f, x + static_cast<float>(i) * rx * 0.36f, y - 2, 0.7f, 0xB8443A);
    p.line(x - rx - 2, y, x + rx + 2, y, 5.5f, kKnitCream);
    p.circle(x, y - ry - 3, 7, kKnitCream);
}

void scarf(const P& p, float x0, float x1, float y, float tail_x) {
    p.line(x0, y, x1, y, 6.5f, kScarfBlue);
    p.box(tail_x - 6, y, tail_x + 6, y + 30, kScarfBlue);
    p.box(tail_x - 6, y + 16, tail_x + 6, y + 19, kKnitCream);
    p.box(tail_x - 6, y + 24, tail_x + 6, y + 27, kKnitCream);
}

// ---- the bunny: plain white, pink inner ears, the right ear tip folded, a smile with two front teeth --------------

void bunny(const P& p, const MascotLook& l, std::uint32_t t) {
    constexpr std::uint32_t W = 0xFBFAF7, OL = 0xC9D3CC, PINK = 0xF4B3BE, INK = 0x120F14;
    p.oellipse(98, 190, 15, 6, W, OL);
    p.oellipse(142, 190, 15, 6, W, OL);
    p.oellipse(120, 158, 42, 33, W, OL);
    p.oellipse(104, 54, 11, 32, W, OL, -8);
    p.ellipse(104, 56, 5, 22, PINK, -8);
    p.oellipse(138, 64, 11, 22, W, OL, 10);
    p.ellipse(138, 66, 5, 14, PINK, 10);
    p.oellipse(152, 44, 9, 15, W, OL, 62);
    p.oellipse(120, 104, 35, 31, W, OL);
    if (l.wear.scarf) scarf(p, 92, 148, 134, 132);
    p.oellipse(110, 166, 7, 9, W, OL);
    p.oellipse(130, 166, 7, 9, W, OL);
    const bool bl = blink_now(l, t);
    eye(p, 103, 100, 5.5f, l.face, INK, bl);
    eye(p, 137, 100, 5.5f, l.face, INK, bl);
    if (l.face == Face::Worried) brows(p, 103, 137, 90, 5, INK);
    const Pt nose[3] = {{116, 111}, {124, 111}, {120, 116}};
    p.poly(nose, 3, 0xF08A9C);
    constexpr std::uint32_t M = 0x8A8590;
    switch (l.face) {
        case Face::Open: case Face::Happy:
            p.line(120, 116, 120, 119, 0.9f, M);
            p.line(120, 119, 115.5f, 122.5f, 0.9f, M); p.line(115.5f, 122.5f, 110.5f, 120.5f, 0.9f, M);
            p.line(120, 119, 124.5f, 122.5f, 0.9f, M); p.line(124.5f, 122.5f, 129.5f, 120.5f, 0.9f, M);
            p.box(116.2f, 120.2f, 123.8f, 127.4f, OL);
            p.box(117, 121, 123, 126.6f, 0xFFFFFF);
            p.line(120, 121, 120, 126.5f, 0.5f, OL);
            break;
        case Face::Half: p.line(114, 121, 126, 121, 0.9f, M); break;
        case Face::Worried: p.ellipse(120, 122, 3, 3.5f, 0x7A2430); break;
        case Face::Asleep: p.arc(120, 117, 4, kPi * 0.25f, kPi * 0.75f, 0.9f, M); break;
    }
    const int blush = l.face == Face::Happy ? 230 : 190;
    p.ellipse(100, 116, 9, 5, 0xF4A3B2, 0, blush);
    p.ellipse(140, 116, 9, 5, 0xF4A3B2, 0, blush);
    if (l.wear.hat) knitted_hat(p, 120, 80, 30, 18);
    if (l.wear.summer) {  // a straw hat, the ears poke out above the brim
        p.ellipse(120, 80, 42, 8, 0xE8C27A);
        p.dome(120, 79, 24, 14, 0xEDCB86);
        p.line(97, 78, 143, 78, 2.6f, kHatRed);
    }
}

// ---- the stag: warm brown, cream muzzle, antlers --------------------------------------------------------------

void antlers(const P& p) {
    constexpr std::uint32_t A = 0xE8D3A8;
    p.line(106, 76, 98, 46, 3, A); p.line(101, 58, 86, 50, 3, A);
    p.line(134, 76, 142, 46, 3, A); p.line(139, 58, 154, 50, 3, A);
}

void stag(const P& p, const MascotLook& l, std::uint32_t t) {
    constexpr std::uint32_t BODY = 0xB5734A, HEAD = 0xC98557, DARK = 0x9A5F3B, INK = 0x2A1A12;
    if (!l.wear.hat) antlers(p);
    p.ellipse(92, 186, 14, 8, DARK);
    p.ellipse(148, 186, 14, 8, DARK);
    p.ellipse(120, 160, 40, 32, BODY);
    p.ellipse(120, 166, 20, 18, 0xC98F5E);
    p.ellipse(86, 90, 13, 6.5f, HEAD, -25);
    p.ellipse(154, 90, 13, 6.5f, HEAD, 25);
    p.ellipse(88, 90, 7, 3, 0xE8A890, -25);
    p.ellipse(152, 90, 7, 3, 0xE8A890, 25);
    p.circle(120, 102, 31, HEAD);
    if (l.wear.scarf) scarf(p, 94, 146, 131, 132);
    p.ellipse(120, 114, 15, 10, 0xF2DCC2);
    p.ellipse(120, 109, 5, 4, 0xD9584A);
    const bool bl = blink_now(l, t);
    eye(p, 107, 97, 4.5f, l.face, INK, bl);
    eye(p, 133, 97, 4.5f, l.face, INK, bl);
    if (l.face == Face::Worried) brows(p, 107, 133, 88, 4.5f, INK);
    constexpr std::uint32_t M = 0x3A2418;
    switch (l.face) {
        case Face::Open: case Face::Happy: p.arc(120, 116, 5, kPi * 0.2f, kPi * 0.8f, 1.1f, M); break;
        case Face::Half: p.line(115, 120, 125, 120, 0.9f, M); break;
        case Face::Worried: p.ellipse(120, 121, 2.5f, 3, 0x5A2A2A); break;
        case Face::Asleep: p.arc(120, 116, 3.5f, kPi * 0.25f, kPi * 0.75f, 0.9f, M); break;
    }
    p.ellipse(98, 108, 5, 3, 0xF29C8C, 0, 190);
    p.ellipse(142, 108, 5, 3, 0xF29C8C, 0, 190);
    if (l.wear.summer && l.face != Face::Asleep) {  // sunglasses
        p.ellipse(107, 97, 8, 6, 0x1C1C24);
        p.ellipse(133, 97, 8, 6, 0x1C1C24);
        p.line(115, 95, 125, 95, 1.2f, 0x1C1C24);
        p.ellipse(104, 95, 2.5f, 1.5f, 0x8A8AA0);
        p.ellipse(130, 95, 2.5f, 1.5f, 0x8A8AA0);
    }
    if (l.wear.hat) { knitted_hat(p, 120, 80, 26, 15); antlers(p); }
}

// ---- the lucky cat: sitting, collar with a bell, one paw raised (it beckons when `wave`) ----------------------

void cat(const P& p, const MascotLook& l, std::uint32_t t) {
    std::uint32_t base = 0xFFFDF8, ol = 0x5A4A44, inner = 0xF4A9A8, ink = 0x3A2E2A, soft = 0x5A4A44;
    std::uint32_t ear_l = 0xE8954A, ear_r = 0x3A3438;
    if (l.coat == Coat::Gold) { base = 0xEAB83E; ol = 0xA57A1E; inner = 0xF7D57A; ink = 0x5E3B12; soft = 0xA57A1E; ear_l = ear_r = base; }
    if (l.coat == Coat::Black) { base = 0x2D2B36; ol = 0x1E1C24; inner = 0xC97A8A; ink = 0xC9C2D6; soft = 0xC9C2D6; ear_l = ear_r = base; }
    const float sway = l.wave ? wave(t, 1200) * 5.0f : 0.0f;
    // the raised paw
    p.oellipse(160 + sway * 0.4f, 126, 11, 22, base, ol, 20 + sway);
    p.ocircle(170 + sway, 96, 14, base, ol);
    p.circle(170 + sway, 100, 4, inner);
    p.circle(164 + sway, 92, 2, inner); p.circle(170 + sway, 89, 2, inner); p.circle(176 + sway, 92, 2, inner);
    p.oellipse(120, 160, 42, 32, base, ol);
    if (l.coat == Coat::Calico) {
        p.ellipse(140, 170, 16, 11, 0xE8954A);
        p.ellipse(100, 182, 10, 6, 0x3A3438);
    }
    p.ocircle(92, 160, 10, base, ol);
    const Pt el[3] = {{82, 86}, {86, 52}, {110, 72}}, er[3] = {{158, 86}, {154, 52}, {130, 72}};
    p.poly(el, 3, ear_l); p.poly(er, 3, ear_r);
    for (int i = 0; i < 3; ++i) { p.line(el[i].x, el[i].y, el[(i + 1) % 3].x, el[(i + 1) % 3].y, 1.0f, ol); p.line(er[i].x, er[i].y, er[(i + 1) % 3].x, er[(i + 1) % 3].y, 1.0f, ol); }
    if (l.coat != Coat::Calico) {
        const Pt il[3] = {{90, 76}, {91, 62}, {102, 72}}, ir[3] = {{150, 76}, {149, 62}, {138, 72}};
        p.poly(il, 3, inner); p.poly(ir, 3, inner);
    }
    p.oellipse(120, 100, 44, 36, base, ol);
    if (l.coat == Coat::Calico) {
        p.ellipse(101, 79, 12, 8, 0xE8954A, -20);
        p.ellipse(140, 79, 11, 8, 0x3A3438, 20);
    }
    if (l.wear.scarf) {
        scarf(p, 88, 152, 132, 134);
    } else {
        p.line(88, 132.5f, 152, 132.5f, 4.5f, 0xD8463A);
        p.ocircle(120, 142, 7, 0xF2C14E, 0xB8892A);
        p.line(120, 144, 120, 148, 0.7f, 0xB8892A);
    }
    const bool bl = blink_now(l, t);
    if (l.coat == Coat::Black && (l.face == Face::Open || l.face == Face::Worried) && !bl) {  // golden eyes with slits
        for (const float ex : {106.0f, 134.0f}) {
            p.ellipse(ex, 98, 7, 8, 0xF2C14E);
            p.ellipse(ex, 98, 2.2f, 6.5f, 0x1A1820);
            p.circle(ex + 2.5f, 94.5f, 1.6f, 0xFFFFFF);
        }
    } else {
        eye(p, 106, 98, 5, l.face, ink, bl);
        eye(p, 134, 98, 5, l.face, ink, bl);
    }
    if (l.face == Face::Worried) brows(p, 106, 134, 88, 5, ink);
    const Pt nose[3] = {{116, 106}, {124, 106}, {120, 111}};
    p.poly(nose, 3, l.coat == Coat::Black ? 0xC97A8A : 0xE88A8A);
    switch (l.face) {
        case Face::Open: case Face::Happy: case Face::Asleep:
            p.arc(116.5f, 112, 3.5f, kPi * 0.1f, kPi * 0.9f, 0.9f, ink);
            p.arc(123.5f, 112, 3.5f, kPi * 0.1f, kPi * 0.9f, 0.9f, ink);
            break;
        case Face::Half: p.line(114, 115, 126, 115, 0.9f, ink); break;
        case Face::Worried: p.ellipse(120, 116, 2.5f, 3, 0x7A2430); break;
    }
    for (const float sgn : {-1.0f, 1.0f}) {
        p.line(120 + sgn * 40, 106, 120 + sgn * 24, 108, 0.7f, soft);
        p.line(120 + sgn * 40, 114, 120 + sgn * 24, 112, 0.7f, soft);
    }
    if (l.coat != Coat::Black) {
        p.ellipse(96, 114, 6, 3.5f, 0xF4A9A8, 0, 180);
        p.ellipse(144, 114, 6, 3.5f, 0xF4A9A8, 0, 180);
    }
    if (l.wear.hat) knitted_hat(p, 120, 72, 30, 14);
    if (l.wear.summer) {  // a paper fan in the low paw
        const float f = wave(t, 900) * 4;
        const Pt fan[5] = {{92, 158}, {62 + f, 136}, {66 + f, 126}, {74 + f, 120}, {84 + f, 118}};
        p.poly(fan, 5, 0xE05A6A);
        for (int i = 1; i <= 3; ++i) p.line(92, 158, fan[i].x + 1, fan[i].y + 1, 0.6f, 0xFFFFFF);
        p.ocircle(92, 160, 10, base, ol);
    }
}

// ---- the penguin: dark blue back, white front, orange beak and feet -------------------------------------------

void penguin(const P& p, const MascotLook& l, std::uint32_t t) {
    constexpr std::uint32_t BACK = 0x2B3145, FRONT = 0xF5F7FA, ORANGE = 0xF29E38, INK = 0x1C2030;
    p.ellipse(104, 190, 13, 6, ORANGE);
    p.ellipse(136, 190, 13, 6, ORANGE);
    p.ellipse(74, 142, 10, 26, BACK, 18);
    p.ellipse(166, 142, 10, 26, BACK, -18);
    p.ellipse(120, 132, 48, 58, BACK);
    p.ellipse(120, 146, 34, 40, FRONT);
    p.ellipse(108, 110, 14, 15, FRONT);
    p.ellipse(132, 110, 14, 15, FRONT);
    if (l.wear.scarf) scarf(p, 90, 150, 137, 138);
    const bool bl = blink_now(l, t);
    eye(p, 108, 106, 4.5f, l.face, INK, bl);
    eye(p, 132, 106, 4.5f, l.face, INK, bl);
    if (l.face == Face::Worried) brows(p, 108, 132, 97, 4.5f, INK);
    const Pt beak[3] = {{113, 116}, {127, 116}, {120, 125}};
    p.poly(beak, 3, ORANGE);
    if (l.face == Face::Worried) p.line(116, 119, 124, 119, 0.8f, 0xB86A20);
    p.ellipse(98, 118, 5, 3, 0xF5A3A3, 0, 200);
    p.ellipse(142, 118, 5, 3, 0xF5A3A3, 0, 200);
    if (l.glasses) {
        const float gy = l.glasses_up ? 86.0f : 107.0f;
        p.arc(108, gy, 9.5f, 0, 2 * kPi, 1.2f, 0x8B5E3C);
        p.arc(132, gy, 9.5f, 0, 2 * kPi, 1.2f, 0x8B5E3C);
        p.line(117.5f, gy, 122.5f, gy, 1.2f, 0x8B5E3C);
    }
    if (l.wear.hat) knitted_hat(p, 120, 95, 40, 28);
    if (l.wear.summer) {  // an ice lolly in the right flipper
        p.line(170, 152, 173, 168, 1.6f, 0xE8C27A);
        p.ellipse(169, 138, 7, 12, 0xF27B9B, -8);
        p.ellipse(167, 133, 3, 4, 0xFFC2D2, -8);
        p.ellipse(166, 142, 10, 22, BACK, -30);
    }
}

}  // namespace

Face face_for(Mood m) noexcept {
    switch (m) {
        case Mood::VeryGood: return Face::Happy;
        case Mood::Good: return Face::Open;
        case Mood::Okay: return Face::Half;
        case Mood::Bad: return Face::Worried;
        case Mood::Sleepy: return Face::Asleep;
    }
    return Face::Open;
}

Wear wear_for(bool weather_ok, const weather::Forecast& w) noexcept {
    Wear r;
    if (!weather_ok) return r;
    r.hat = r.scarf = w.temp_c < kColdC;
    r.summer = w.temp_c >= kHotC;
    return r;
}

void draw_mascot(const Canvas& cv, float cx, float cy, float s, const MascotLook& l, std::uint32_t t) noexcept {
    const P p{cv, cx, cy, s};
    switch (l.who) {
        case Mascot::Bunny: bunny(p, l, t); break;
        case Mascot::Stag: stag(p, l, t); break;
        case Mascot::Cat: cat(p, l, t); break;
        case Mascot::Penguin: penguin(p, l, t); break;
        case Mascot::Squirrel: break;  // drawn by draw_squirrel
    }
}

void draw_dog(const Canvas& cv, float x, float y, float s, float face, DogPose pose, std::uint32_t t) noexcept {
    constexpr std::uint32_t D = 0x1E1C22, EAR = 0x121015, SNOUT = 0x2E2B34, COLLAR = 0xD8463A;
    const float f = face >= 0 ? 1.0f : -1.0f;
    auto X = [&](float dx) { return x + dx * s * f; };
    auto Y = [&](float dy) { return y + dy * s; };
    auto c = [&](float dx, float dy, float r, std::uint32_t col) { draw::circle(cv, X(dx), Y(dy), r * s, hex(col)); };
    auto e = [&](float dx, float dy, float rx, float ry, std::uint32_t col, float deg = 0) {
        draw::ellipse(cv, X(dx), Y(dy), rx * s, ry * s, hex(col), deg * kDeg * f);
    };
    auto l = [&](float x0, float y0, float x1, float y1, float r, std::uint32_t col) {
        draw::capsule(cv, X(x0), Y(y0), X(x1), Y(y1), r * s, hex(col));
    };
    const float wag = wave(t, 360) * 4;
    switch (pose) {
        case DogPose::Stand: {
            const float step = wave(t, 500) * 2;
            l(-14, -4, -18 + wag * 0.5f, -20, 2.5f, D);
            l(-12, 4, -12 - step, 14, 3, D); l(-4, 5, -4 + step, 14, 3, D);
            l(6, 5, 6 - step, 14, 3, D); l(13, 4, 13 + step, 14, 3, D);
            e(0, 0, 18, 10, D);
            c(16, -12, 10, D);
            e(25, -9, 6.5f, 4.5f, SNOUT);
            c(30, -10, 2, 0x000000);
            e(10, -10, 4, 8, EAR, 15);
            c(19, -15, 1.6f, 0xFFFFFF);
            l(8, -5, 10, 1, 1.6f, COLLAR);
            e(26, -3, 2.5f, 3.5f, 0xF07C8C);
            break;
        }
        case DogPose::Sit:
            l(-9, 12, -20, 8 + wag * 0.4f, 2.5f, D);
            e(0, 0, 12, 16, D);
            e(-4, 14, 4, 3, D); e(6, 14, 4, 3, D);
            c(3, -20, 11, D);
            e(12, -18, 6.5f, 4.5f, SNOUT);
            c(17, -19, 2, 0x000000);
            e(-4, -18, 4, 9, EAR, 10);
            c(6, -24, 1.6f, 0xFFFFFF);
            l(-7, -9, 9, -9, 1.8f, COLLAR);
            e(14, -12, 2.2f, 3, 0xF07C8C);
            break;
        case DogPose::Sleep: {
            const float breathe = wave(t, 3000) * 0.8f;
            e(0, 0, 26, 11 + breathe, D);
            c(22, 2, 10, D);
            e(16, 2, 4, 8, EAR, 60);
            draw::arc(cv, X(25), Y(0), 3 * s, kPi * 0.15f, kPi * 0.85f, 0.7f * s, hex(0x6A6574));
            l(12, -6, 12, 6, 1.8f, COLLAR);
            break;
        }
    }
}

void render_mascot_home(const Canvas& cv, const Inputs& in) noexcept {
    static constexpr const char* kDays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    static constexpr const char* kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                              "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    const std::uint32_t t = in.t_ms;
    HomeData h;
    h.t_ms = t;
    h.alert = in.alert;
    render_background(cv, h);

    const auto white = hex(0xFFFFFF), soft = hex(0xE6ECFF);
    char buf[32];
    if (in.time_valid) std::snprintf(buf, sizeof buf, (in.second & 1) ? "%02d:%02d" : "%02d %02d", in.hour, in.minute);
    else std::snprintf(buf, sizeof buf, "--:--");
    draw::label(cv, 44, 7, white, buf);
    if (in.time_valid) {
        std::snprintf(buf, sizeof buf, "%s %d %s", kDays[in.weekday % 7], in.day, kMonths[(in.month + 11) % 12]);
        draw::label(cv, 100, 2, soft, buf);
    }
    char inside[12] = "--", outside[12] = "--";
    if (in.sensor_ok) std::snprintf(inside, sizeof inside, "%.0f^", static_cast<double>(in.reading.temp_c));
    if (in.weather_ok) std::snprintf(outside, sizeof outside, "%.0f^", static_cast<double>(in.weather.temp_c));
    std::snprintf(buf, sizeof buf, "IN %s OUT %s", inside, outside);
    draw::label(cv, 120, 2, soft, buf);
    if (in.weather_ok) {
        const auto& w = in.weather;
        if (w.precip_pct >= 20)
            std::snprintf(buf, sizeof buf, "%.0f-%.0f^ RAIN %d%%", static_cast<double>(w.min_c), static_cast<double>(w.max_c), w.precip_pct);
        else
            std::snprintf(buf, sizeof buf, "LO %.0f^ HI %.0f^", static_cast<double>(w.min_c), static_cast<double>(w.max_c));
        draw::label(cv, 138, 2, hex(0xDDF0FF), buf);
    }

    // the air level, in words and on the mascot's face
    const Mood m = in.sensor_ok ? mood_for(in.quality.level) : Mood::Sleepy;
    std::uint32_t col = 0xDDF0FF;
    switch (m) {
        case Mood::VeryGood: col = 0xA8F5A0; break;
        case Mood::Good: col = 0xDDF0FF; break;
        case Mood::Okay: col = 0xFFE39A; break;
        case Mood::Bad: col = 0xFFA090; break;
        case Mood::Sleepy: col = 0xC8C8E8; break;
    }
    const char* word = (m == Mood::Bad && (t / 2000u) % 2u) ? "OPEN WINDOW" : nullptr;
    if (!word) { std::snprintf(buf, sizeof buf, "AIR %s", mood_label(m)); word = buf; }
    draw::label(cv, 158, 2, hex(col), word);

    MascotLook l;
    l.who = in.mascot;
    l.face = face_for(m);
    l.wear = wear_for(in.weather_ok, in.weather);
    l.coat = in.time_valid ? cat_coat(cal::day_number(in.year, in.month, in.day)) : Coat::Calico;
    l.wave = m == Mood::VeryGood || m == Mood::Good;
    const float hop = m == Mood::VeryGood ? std::fabs(wave(t, 1200)) * 5.0f : 0.0f;
    constexpr float kS = 0.55f, kCx = 120;
    const float cy = in.mascot == Mascot::Penguin ? 214.0f : 232.0f;  // the penguin has no ears to peek with
    draw_mascot(cv, kCx, cy - hop, kS, l, t);
    switch (m) {
        case Mood::VeryGood:
            for (int i = 0; i < 3; ++i) {
                const float tw = (wave(t + static_cast<std::uint32_t>(i) * 500u, 1500) + 1) * 0.5f;
                const float sx = i == 1 ? 158.0f : 84.0f + static_cast<float>(i) * 6;
                draw::sparkle(cv, sx, 200 + static_cast<float>(i) * 9, 3 + tw * 3, hex(0xFFF6C8), 140 + static_cast<int>(tw * 116));
            }
            break;
        case Mood::Bad: props::sweat_drop(cv, 146, 204, 1.2f, t); break;
        case Mood::Sleepy:
            for (int i = 0; i < 3; ++i) {
                const float k = ph(t + static_cast<std::uint32_t>(i) * 1000u, 3000);
                if (k > 0.85f) continue;
                draw_text(cv, static_cast<int>(146 + k * 10), static_cast<int>(210 - k * 26), 1 + (k > 0.4f), hex(0xE8E8FF), "Z");
            }
            break;
        default: break;
    }
}

}  // namespace screen
