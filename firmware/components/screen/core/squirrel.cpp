#include "screen/core/squirrel.hpp"

#include <cmath>
#include <cstdio>

#include "screen/core/draw.hpp"
#include "screen/core/sky.hpp"

namespace screen {
namespace {

using draw::hex;
using draw::Pt;

constexpr float kPi = 3.14159265f;

// Phase of a repeating cycle, 0..1.
float ph(std::uint32_t t, std::uint32_t period) noexcept {
    return static_cast<float>(t % period) / static_cast<float>(period);
}
float wave(std::uint32_t t, std::uint32_t period) noexcept { return std::sin(2 * kPi * ph(t, period)); }

// Palette (0xRRGGBB)
constexpr std::uint32_t kFur = 0xE0873A, kFurDark = 0xB0602A, kFurLight = 0xF4AE66, kBelly = 0xFBE4C4,
                        kEarIn = 0xF4A6A0, kEye = 0x2A1810, kNose = 0x5B2B1A, kCheek = 0xFF8C9A,
                        kAcorn = 0xB37B40, kAcornCap = 0x6E4220, kMouth = 0x7A2430, kTongue = 0xF07C8A,
                        kWhite = 0xFFFFFF, kHat = 0xE0505E, kHatShade = 0xB23A48, kHatStripe = 0xFFD6DC,
                        kHatCuff = 0xF6F3EE, kHatCuffShade = 0xC9C2BA, kCone = 0xE2A85A, kConeLine = 0xB37A36,
                        kScoop = 0xFF9EC4, kScoopShade = 0xE57BA6;

// Maps squirrel design coordinates (body centre at 125,150) to the screen.
struct Pen {
    const Canvas& cv;
    float ox, oy, s;
    float X(float x) const { return ox + (x - 125.0f) * s; }
    float Y(float y) const { return oy + (y - 150.0f) * s; }
    void circle(float x, float y, float r, std::uint32_t c, int a = 256) const { draw::circle(cv, X(x), Y(y), r * s, hex(c), a); }
    void ellipse(float x, float y, float rx, float ry, std::uint32_t c, float ang = 0, int a = 256) const {
        draw::ellipse(cv, X(x), Y(y), rx * s, ry * s, hex(c), ang, a);
    }
    void line(float x0, float y0, float x1, float y1, float r, std::uint32_t c, int a = 256) const {
        draw::capsule(cv, X(x0), Y(y0), X(x1), Y(y1), r * s, hex(c), a);
    }
    void arc(float x, float y, float R, float a0, float a1, float w, std::uint32_t c) const {
        draw::arc(cv, X(x), Y(y), R * s, a0, a1, w * s, hex(c));
    }
    void quad(float x0, float y0, float x1, float y1, std::uint32_t c) const {  // axis-aligned box
        const Pt p[4] = {{X(x0), Y(y0)}, {X(x1), Y(y0)}, {X(x1), Y(y1)}, {X(x0), Y(y1)}};
        draw::polygon(cv, p, 4, hex(c));
    }
};

struct Anim {
    float dx = 0, dy = 0;   // whole body offset
    float tail = 0;         // tail rotation around its base, radians (negative = droops away)
    float ear = 0;          // extra outward ear tilt
    bool blink = false;
};

Anim animate(Mood m, std::uint32_t t, bool windy) noexcept {
    Anim a;
    switch (m) {
        case Mood::VeryGood:
            a.dy = -std::fabs(std::sin(kPi * ph(t, 700))) * 8.0f;
            a.tail = wave(t, 500) * 0.15f;
            a.ear = wave(t, 350) * 0.08f;
            break;
        case Mood::Good:
            a.dy = wave(t, 1800) * 1.8f;
            a.tail = wave(t, 2000) * 0.08f;
            a.blink = ph(t, 3700) > 0.965f;
            break;
        case Mood::Okay:
            a.dy = wave(t, 3000) * 1.2f;
            a.tail = -0.12f + wave(t, 3000) * 0.04f;
            a.ear = 0.12f;
            a.blink = ph(t, 4300) > 0.96f;
            break;
        case Mood::Bad:
            a.dx = ph(t, 2500) < 0.25f ? wave(t, 90) * 1.2f : 0.0f;  // shiver now and then
            a.tail = -0.4f;
            a.ear = 0.35f;
            break;
        case Mood::Sleepy:
            a.dy = wave(t, 3500) * 1.5f;
            a.tail = 0.05f;
            a.ear = 0.2f;
            break;
    }
    if (windy) {  // lifted off the ground by the umbrella, the tail streaming behind
        a.dy = -16.0f + wave(t, 1500) * 4.0f;
        a.dx = wave(t, 1100) * 2.5f;
        a.tail = -1.5f + wave(t, 450) * 0.15f;
        a.ear = 0.3f + wave(t, 300) * 0.1f;
        a.blink = false;
    }
    return a;
}

void tail(const Pen& p, float angle) {
    struct Puff { float x, y, r; };
    // a "?"-shaped plume: thin at the base, bushy at the top, the tip curling over towards the head
    static constexpr Puff kTail[] = {{94, 168, 11}, {78, 156, 16}, {66, 138, 21}, {60, 116, 24},
                                     {62, 92, 25},  {72, 72, 22},  {88, 60, 18},  {101, 60, 13}};
    constexpr int kN = 8;
    constexpr float bx = 100, by = 172;   // pivot at the base
    constexpr float mx = 90, my = 112;    // inside of the curl
    const float c = std::cos(angle), s = std::sin(angle);
    Puff q[kN];
    for (int i = 0; i < kN; ++i) {
        const float x = kTail[i].x - bx, y = kTail[i].y - by;
        const float k = static_cast<float>(i) / (kN - 1);  // the tip swings more than the base
        const float ci = 1 + (c - 1) * k, si = s * k;
        q[i] = {bx + x * ci - y * si, by + x * si + y * ci, kTail[i].r};
    }
    // fluffy tufts on the outer rim
    Puff tufts[kN * 3];
    int nt = 0;
    for (int i = 1; i < kN; ++i) {
        float dx = q[i].x - mx, dy = q[i].y - my;
        const float len = std::sqrt(dx * dx + dy * dy) + 1e-3f;
        dx /= len; dy /= len;
        for (int j = -1; j <= 1; ++j) {
            const float a = static_cast<float>(j) * 0.55f, ca = std::cos(a), sa = std::sin(a);
            const float ux = dx * ca - dy * sa, uy = dx * sa + dy * ca;
            tufts[nt++] = {q[i].x + ux * q[i].r * 0.88f, q[i].y + uy * q[i].r * 0.88f, q[i].r * 0.36f};
        }
    }
    for (int i = 0; i < nt; ++i) p.circle(tufts[i].x, tufts[i].y, tufts[i].r + 2.5f, kFurDark);
    for (const Puff& u : q) p.circle(u.x, u.y, u.r + 2.5f, kFurDark);
    for (int i = 0; i < nt; ++i) p.circle(tufts[i].x, tufts[i].y, tufts[i].r, kFur);
    for (const Puff& u : q) p.circle(u.x, u.y, u.r, kFur);
    // lighter streak along the inside of the curl
    for (int i = 1; i < kN; ++i) {
        const float dx = mx - q[i].x, dy = my - q[i].y;
        const float len = std::sqrt(dx * dx + dy * dy) + 1e-3f;
        p.circle(q[i].x + dx / len * q[i].r * 0.35f, q[i].y + dy / len * q[i].r * 0.35f, q[i].r * 0.45f, kFurLight, 170);
    }
}

void ear(const Pen& p, float x, float y, float ang) {
    p.ellipse(x, y, 8.5f, 15, kFurDark, ang);
    p.ellipse(x, y, 7, 13.5f, kFur, ang);
    p.ellipse(x + std::sin(-ang) * -1.5f, y + 2, 4, 8.5f, kEarIn, ang);
    // tuft at the tip
    const float tx = x + std::sin(ang) * 14, ty = y - std::cos(ang) * 14;
    p.line(tx, ty, tx + std::sin(ang) * 6, ty - std::cos(ang) * 6, 2.6f, kFurDark);
}

void eye(const Pen& p, float x, float y, Mood m, bool blink) {
    if (m == Mood::VeryGood) { p.arc(x, y + 3, 5, kPi + 0.35f, 2 * kPi - 0.35f, 1.7f, kEye); return; }  // ^ ^
    if (m == Mood::Sleepy) { p.arc(x, y - 3, 5, 0.35f, kPi - 0.35f, 1.5f, kEye); return; }              // closed
    if (blink) { p.line(x - 5, y + 1, x + 5, y + 1, 1.5f, kEye); return; }
    p.ellipse(x, y, 5.5f, 7, kEye);
    p.circle(x + 2, y - 2.5f, 2.2f, kWhite);
    p.circle(x - 1.5f, y + 3, 1.0f, kWhite, 190);
    if (m == Mood::Okay) {  // half-closed lid
        p.quad(x - 7, y - 9, x + 7, y - 0.5f, kFur);
        p.line(x - 6, y - 0.5f, x + 6, y - 0.5f, 1.2f, kEye);
    }
}

void acorn(const Pen& p, float x, float y, float ang = 0) {
    p.ellipse(x, y + 2, 7, 8.5f, kAcorn, ang);
    p.circle(x - 2.5f, y + 1, 2, 0xD9A066, 160);
    p.ellipse(x, y - 4.5f, 8.5f, 4.5f, kAcornCap, ang);
    p.line(x, y - 8, x + 1.5f, y - 11.5f, 1.3f, kAcornCap);
}

// Knitted beanie with a pom-pom; the ears poke out through it.
void hat_dome(const Pen& p) {
    constexpr float cx = 125, cy = 91, rx = 32, ry = 27;
    Pt dome[13];
    for (int i = 0; i <= 12; ++i) {  // top half of an ellipse
        const float a = kPi + kPi * static_cast<float>(i) / 12;
        dome[i] = {p.X(cx + rx * std::cos(a)), p.Y(cy + ry * std::sin(a))};
    }
    draw::polygon(p.cv, dome, 13, hex(kHat));
    for (const float y : {79.0f, 71.0f}) {  // knit stripes, inset to the dome's width at that height
        const float d = (y - cy) / ry, half = rx * std::sqrt(std::max(0.0f, 1 - d * d)) - 4;
        p.line(cx - half, y, cx + half, y, 2.2f, kHatStripe, 220);
    }
}

void hat_cuff(const Pen& p, std::uint32_t t) {
    p.line(95, 90, 155, 90, 6, kHatCuffShade);
    p.line(95, 89, 155, 89, 5.2f, kHatCuff);
    for (int i = 0; i < 6; ++i) p.line(100 + i * 10.0f, 86.5f, 100 + i * 10.0f, 91.5f, 0.6f, kHatCuffShade, 170);  // ribbing
    const float bob = wave(t, 900) * 0.8f;
    p.circle(125, 62 + bob, 8, kHatCuffShade);
    p.circle(125, 61.5f + bob, 7, kHatCuff);
    p.circle(122.5f, 59 + bob, 2.5f, kWhite, 200);
}

void scarf(const Pen& p) {
    p.line(141, 131, 148, 151, 4.4f, kHatShade);
    p.line(141, 131, 148, 151, 3.6f, kHat);
    p.line(144.5f, 145, 148.5f, 144, 1.1f, kHatStripe);
    p.line(102, 129, 148, 129, 6, kHatShade);
    p.line(102, 128, 148, 128, 5.2f, kHat);
    for (int i = 0; i < 4; ++i) p.line(109 + i * 11.0f, 124, 107 + i * 11.0f, 132, 1.1f, kHatStripe, 200);
}

// Strawberry ice cream on a waffle cone, slowly melting.
void ice_cream(const Pen& p, std::uint32_t t, bool paws = true) {
    const Pt cone[3] = {{p.X(117.5f), p.Y(139)}, {p.X(132.5f), p.Y(139)}, {p.X(125), p.Y(160)}};
    draw::polygon(p.cv, cone, 3, hex(kCone));
    p.line(120, 143, 127, 151, 0.7f, kConeLine);
    p.line(130, 143, 123, 151, 0.7f, kConeLine);
    p.line(122, 141, 129, 148, 0.7f, kConeLine);
    p.line(128, 141, 121, 148, 0.7f, kConeLine);
    p.circle(125, 133, 8.5f, kScoopShade);
    p.circle(125, 132.5f, 8, kScoop);
    p.circle(119, 138.5f, 3, kScoop);
    p.circle(131, 138.5f, 3, kScoop);
    p.circle(122, 129.5f, 2.6f, kWhite, 150);
    for (int i = 0; i < 4; ++i)  // sprinkles
        p.line(120 + i * 3.5f, 133 - (i & 1) * 3.0f, 121.5f + i * 3.5f, 134 - (i & 1) * 3.0f, 0.7f, i & 1 ? 0x6FD0F0 : 0xFFE14D);
    const float k = ph(t, 2600);  // a drip running down the cone
    p.circle(131.5f, 139 + k * 9, 1.6f, kScoop, static_cast<int>(256 * (1 - k)));
    p.line(127, 124.5f, 129, 121, 0.7f, 0x3A7A2A);  // cherry
    p.circle(126.5f, 124.5f, 2.8f, 0xE53950);
    p.circle(125.7f, 123.6f, 0.9f, kWhite, 200);
    if (paws) {  // holding the cone
        p.circle(117.5f, 146, 4.2f, kFur);
        p.circle(132.5f, 146, 4.2f, kFur);
    }
}

void sunglasses(const Pen& p) {
    for (const float x : {111.0f, 139.0f}) {
        p.ellipse(x, 99, 12, 9, 0x14141C);
        p.ellipse(x - 3.5f, 95.5f, 4.5f, 2.2f, kWhite, -0.5f, 110);  // glint
    }
    p.line(122, 97.5f, 128, 97.5f, 1.4f, 0x14141C);  // bridge
    p.line(100, 96, 93, 91, 1.2f, 0x14141C);         // arms towards the ears
    p.line(150, 96, 157, 91, 1.2f, 0x14141C);
}

// Umbrella held in the right paw. tilt (radians) leans the canopy; positive = to the right.
void umbrella(const Pen& p, float tilt) {
    constexpr float hx = 165, hy = 128, len = 72, R = 54, H = 28;
    constexpr std::uint32_t kPanelA = 0x3F6FD8, kPanelB = 0x9CC0F5, kEdge = 0x24428C, kShaft = 0x4A3A30;
    const float c = std::cos(tilt), sn = std::sin(tilt);
    const float ux = sn, uy = -c;              // up along the shaft
    const float bx = hx + ux * len, by = hy + uy * len;  // canopy base centre
    auto at = [&](float lx, float ly) { return Pt{p.X(bx + lx * c + ly * ux), p.Y(by + lx * sn + ly * uy)}; };
    p.line(hx, hy, bx, by, 1.4f, kShaft);              // shaft
    p.arc(hx - 4.5f, hy, 4.5f, 0.0f, kPi, 1.4f, kShaft);  // hook
    constexpr int kSeg = 6, kStep = 4;
    for (int pass = 0; pass < 2; ++pass) {  // pass 0: dark rim, pass 1: panels
        const float r = pass == 0 ? R + 2.5f : R, h = pass == 0 ? H + 2.5f : H;
        for (int k = 0; k < kSeg; ++k) {
            Pt poly[kStep + 2];
            int n = 0;
            poly[n++] = at(0, pass == 0 ? -1.5f : 0);
            for (int j = 0; j <= kStep; ++j) {
                const float a = kPi * (static_cast<float>(k) + static_cast<float>(j) / kStep) / kSeg;
                poly[n++] = at(r * std::cos(a), h * std::sin(a));
            }
            draw::polygon(p.cv, poly, n, hex(pass == 0 ? kEdge : (k & 1 ? kPanelB : kPanelA)));
        }
    }
    p.line(bx + ux * (H + 1), by + uy * (H + 1), bx + ux * (H + 6), by + uy * (H + 6), 1.2f, kShaft);  // tip
}

void wind_streaks(const Pen& p, std::uint32_t t) {
    for (int i = 0; i < 4; ++i) {
        const float k = ph(t + static_cast<std::uint32_t>(i) * 330u, 1300);
        const float y = 64 + static_cast<float>(i) * 34 + wave(t + static_cast<std::uint32_t>(i) * 200u, 900) * 3;
        const float x = -30 + k * 230;
        const int al = static_cast<int>(200 * std::sin(kPi * k));
        p.line(x, y, x + 30 + 8.0f * static_cast<float>(i & 1), y, 1.1f, kWhite, al);
    }
}

}  // namespace

Bob squirrel_bob(Mood m, std::uint32_t t) noexcept {
    const Anim a = animate(m, t, false);
    return {a.dx, a.dy};
}

Outfit outfit_for(bool weather_ok, const weather::Forecast& w) noexcept {
    Outfit o;
    if (!weather_ok) return o;
    using weather::Sky;
    o.hat = w.temp_c < kColdC;
    o.ice_cream = w.temp_c >= kHotC;
    o.sunglasses = w.is_day && (w.sky == Sky::Clear || w.sky == Sky::PartlyCloudy);
    o.windy = w.wind_kmh >= kWindyKmh;
    o.umbrella = o.windy || w.sky == Sky::Drizzle || w.sky == Sky::Rain || w.sky == Sky::Thunder;
    if (o.umbrella) o.ice_cream = false;  // the paws hold the umbrella
    return o;
}

Mood mood_for(sensor::Level l) noexcept {
    switch (l) {
        case sensor::Level::Good: return Mood::VeryGood;
        case sensor::Level::Fair: return Mood::Good;
        case sensor::Level::Stale: return Mood::Okay;
        case sensor::Level::Poor: return Mood::Bad;
        case sensor::Level::WarmingUp: break;
    }
    return Mood::Sleepy;
}

const char* mood_label(Mood m) noexcept {
    switch (m) {
        case Mood::VeryGood: return "VERY GOOD";
        case Mood::Good: return "GOOD";
        case Mood::Okay: return "OKAY";
        case Mood::Bad: return "BAD";
        case Mood::Sleepy: return "WAKING UP";
    }
    return "";
}

void draw_squirrel(const Canvas& cv, float cx, float cy, float scale, Mood m, std::uint32_t t, Outfit o) noexcept {
    if (o.paws_free) o.ice_cream = o.umbrella = o.windy = false;  // indoors, paws busy
    const Anim a = animate(m, t, o.windy);
    // the ice cream replaces the acorn; a fanning (bad air) or sleeping squirrel keeps its pose
    const bool cone = o.ice_cream && !o.umbrella && (m == Mood::VeryGood || m == Mood::Good || m == Mood::Okay);
    const Pen p{cv, cx + a.dx * scale, cy + a.dy * scale, scale};

    tail(p, a.tail);

    // body, thighs, belly, feet
    p.ellipse(125, 150, 30, 35, kFurDark);
    p.ellipse(125, 150, 28, 33, kFur);
    p.ellipse(106, 167, 13, 15, kFurDark);
    p.ellipse(144, 167, 13, 15, kFurDark);
    p.ellipse(106, 167, 11.5f, 13.5f, kFur);
    p.ellipse(144, 167, 11.5f, 13.5f, kFur);
    p.ellipse(126, 157, 17, 23, kBelly);
    p.ellipse(104, 182, 13, 6, kFurDark);
    p.ellipse(148, 182, 13, 6, kFurDark);

    // arms and paws
    if (o.paws_free) {
        // the caller draws the arms with whatever they hold
    } else if (cone) {
        p.ellipse(114, 139, 6, 10, kFur, 0.6f);
        p.ellipse(136, 139, 6, 10, kFur, -0.6f);
    } else switch (m) {
        case Mood::VeryGood:
        case Mood::Good:
            p.ellipse(114, 139, 6, 10, kFur, 0.6f);
            p.ellipse(136, 139, 6, 10, kFur, -0.6f);
            acorn(p, 125, 141, m == Mood::VeryGood ? wave(t, 700) * 0.15f : 0.0f);
            p.circle(118, 143, 4.2f, kFur);
            p.circle(132, 143, 4.2f, kFur);
            break;
        case Mood::Okay:
            p.ellipse(103, 150, 5.5f, 11, kFur, 0.15f);
            p.ellipse(147, 150, 5.5f, 11, kFur, -0.15f);
            acorn(p, 170, 178, 0.5f);  // put down next to it
            break;
        case Mood::Bad: {
            p.ellipse(103, 150, 5.5f, 11, kFur, 0.15f);
            const float fy = 116 + wave(t, 260) * 4;  // fanning paw
            p.line(143, 137, 154, fy + 4, 5, kFurDark);
            p.line(143, 137, 154, fy + 4, 4, kFur);
            p.circle(155, fy, 5.5f, kFur);
            break;
        }
        case Mood::Sleepy:
            p.circle(117, 146, 5, kFur);
            p.circle(133, 146, 5, kFur);
            break;
    }

    // head
    if (!o.hat) {
        ear(p, 104, 76, -0.35f - a.ear);
        ear(p, 146, 76, 0.35f + a.ear);
    }
    p.ellipse(125, 103, 33, 29, kFurDark);
    p.ellipse(125, 103, 31, 27, kFur);
    p.ellipse(118, 92, 12, 7, kFurLight, -0.2f, 120);  // forehead shine
    p.ellipse(125, 114, 16, 11, kBelly);
    if (o.hat) {
        hat_dome(p);
        ear(p, 102, 72, -0.45f - a.ear);  // through the hat
        ear(p, 148, 72, 0.45f + a.ear);
        hat_cuff(p, t);
        scarf(p);
    }
    if (m != Mood::Bad || o.hat) {  // rosy cheeks, also from the cold
        p.circle(103, 112, 6, kCheek, m == Mood::VeryGood || o.hat ? 200 : 130);
        p.circle(147, 112, 6, kCheek, m == Mood::VeryGood || o.hat ? 200 : 130);
    }
    eye(p, 112, 99, m, a.blink);
    eye(p, 138, 99, m, a.blink);
    if (o.sunglasses) sunglasses(p);
    if (m == Mood::Bad) {  // worried brows (under the hat cuff they sit lower)
        const float by = o.hat ? 4.0f : 0.0f;
        p.line(106, 90 + by, 116, 86 + by, 1.5f, kFurDark);
        p.line(134, 86 + by, 144, 90 + by, 1.5f, kFurDark);
    }

    // mouth (before the nose so the nose sits on top)
    switch (m) {
        case Mood::VeryGood:
            p.ellipse(125, 116, 7, 6.5f, kMouth);
            p.ellipse(125, 119.5f, 4, 2.5f, kTongue);
            p.quad(116, 108, 134, 115.5f, kBelly);
            p.quad(122.5f, 115.5f, 127.5f, 118.5f, kWhite);
            break;
        case Mood::Good:
            p.quad(123, 113, 127, 117, kWhite);
            p.line(125, 113, 125, 117, 0.4f, 0xC8C0B8);
            p.arc(121, 111.5f, 4, 0.2f, kPi - 0.2f, 1.1f, kEye);
            p.arc(129, 111.5f, 4, 0.2f, kPi - 0.2f, 1.1f, kEye);
            break;
        case Mood::Okay:
            p.line(120, 116, 130, 116, 1.2f, kEye);
            break;
        case Mood::Bad:
            p.arc(125, 122, 6, kPi + 0.5f, 2 * kPi - 0.5f, 1.3f, kEye);
            break;
        case Mood::Sleepy:
            p.arc(125, 112, 3, 0.4f, kPi - 0.4f, 1.0f, kEye);
            break;
    }
    p.ellipse(125, 108, 4.5f, 3.2f, kNose);
    p.circle(123.8f, 107, 1.1f, kWhite, 150);
    if (cone) ice_cream(Pen{cv, p.ox, p.oy + 1.5f * p.s, p.s * 1.3f}, t);  // 1.3x, anchored at the paws
    else if (o.ice_cream && m == Mood::Bad)  // the free paw holds it while the other one fans
        ice_cream(Pen{cv, p.ox + (101 - 125) * p.s, p.oy + 2 * p.s, p.s * 0.85f}, t);
    else if (o.ice_cream && m == Mood::Sleepy)  // put down next to it for later
        ice_cream(Pen{cv, p.ox + (170 - 125) * p.s, p.oy + 18 * p.s, p.s * 1.0f}, t, false);

    if (o.umbrella) {
        p.line(138, 141, 165, 129, 5.5f, kFurDark);  // the arm reaching out to the handle
        p.line(138, 141, 165, 129, 4.3f, kFur);
        umbrella(p, o.windy ? -0.32f + wave(t, 1700) * 0.07f : 0.06f);
        p.circle(165, 128, 4.6f, kFur);
    }
    if (o.windy) wind_streaks(p, t);

    if (m == Mood::Bad) {  // sweat drop sliding down
        const float k = ph(t, 1800), y = 84 + k * 12;
        const int al = static_cast<int>(256 * (1 - k * k));
        const Pt tri[3] = {{p.X(156.2f), p.Y(y - 0.8f)}, {p.X(162.8f), p.Y(y - 0.8f)}, {p.X(159.5f), p.Y(y - 9)}};
        draw::polygon(cv, tri, 3, hex(0x8FD3FF), al);
        p.circle(159.5f, y, 3.5f, 0x8FD3FF, al);
        p.circle(158.5f, y - 1, 1.1f, kWhite, al);
    }
}

void render_air(const Canvas& cv, const AirData& d) noexcept {
    const std::uint32_t t = d.t_ms;
    const Mood m = d.sensor_ok ? mood_for(d.quality.level) : Mood::Sleepy;

    std::uint32_t top = 0, bottom = 0, hill = 0, hill_back = 0;
    switch (m) {
        case Mood::VeryGood: top = 0x8EDBFF; bottom = 0xE4FFF2; hill = 0x6FC75E; hill_back = 0x9AD98A; break;
        case Mood::Good: top = 0xA6D3EE; bottom = 0xEEF7EA; hill = 0x7DBD66; hill_back = 0xA5CF91; break;
        case Mood::Okay: top = 0xEBD9A6; bottom = 0xFFF5DD; hill = 0xAAB062; hill_back = 0xC8C68C; break;
        case Mood::Bad: top = 0x948BA3; bottom = 0xCBC3CD; hill = 0x85896A; hill_back = 0xA2A286; break;
        case Mood::Sleepy: top = 0x162447; bottom = 0x3B3D74; hill = 0x2C4A3B; hill_back = 0x24403A; break;
    }
    const bool wx = d.weather_ok;
    const auto& w = d.weather;
    const bool snowy = wx && w.sky == weather::Sky::Snow;
    if (wx) {
        // the sky shows the weather outside; the air quality tints it and colours the hill
        sky::Palette pal = sky::colors(w.sky, w.is_day, t);
        if (m == Mood::Bad) pal = {draw::lerp_rgb(pal.top, 0x8E8699, 110), draw::lerp_rgb(pal.bottom, 0xB8B0BC, 110)};
        if (d.alert) pal.top = draw::lerp_rgb(pal.top, 0xFF8A7A, static_cast<int>((wave(t, 1600) + 1) * 70));
        sky::draw(cv, w.sky, w.is_day, t, 200, 98, 0.55f, pal, false);
        if (m == Mood::Sleepy) { hill = 0x7DBD66; hill_back = 0xA5CF91; }  // the night colours come from the sky now
        if (snowy) { hill = 0xC9D8E8; hill_back = 0xE4ECF4; }
        const int gl = sky::gloom(w.sky);  // grey light under clouds and rain
        hill = draw::lerp_rgb(hill, 0x4A5A55, gl); hill_back = draw::lerp_rgb(hill_back, 0x4A5A55, gl);
        if (!w.is_day) { hill = draw::lerp_rgb(hill, 0x16302A, 130); hill_back = draw::lerp_rgb(hill_back, 0x16302A, 150); }
    } else {
        if (d.alert) top = draw::lerp_rgb(top, 0xFF8A7A, static_cast<int>((wave(t, 1600) + 1) * 70));
        draw::vgradient(cv, top, bottom);
        if (m == Mood::Sleepy) {
            static constexpr Pt kStars[] = {{50, 60}, {78, 32}, {176, 40}, {202, 96}, {40, 120}, {150, 22}, {205, 150}};
            for (int i = 0; i < 7; ++i) {
                const float tw = (wave(t + static_cast<std::uint32_t>(i) * 437u, 2200) + 1) * 0.5f;
                draw::sparkle(cv, kStars[i].x, kStars[i].y, 2.5f + tw * 2.5f, hex(0xFFF6C8), 120 + static_cast<int>(tw * 136));
            }
        }
        if (m == Mood::VeryGood || m == Mood::Good) {  // a little sun peeking in
            draw::circle(cv, 192, 56, 20, hex(0xFFE27A), 110);
            draw::circle(cv, 192, 56, 14, hex(0xFFD23F));
        }
    }

    draw::circle(cv, 30, 296, 125, hex(hill_back));
    draw::circle(cv, 125, 312, 143, hex(hill));

    if ((m == Mood::VeryGood || m == Mood::Good) && !snowy) {  // flowers on the hill
        static constexpr Pt kFlowers[] = {{30, 184}, {44, 196}, {200, 204}};
        for (int i = 0; i < 3; ++i) {
            const float sw = wave(t + static_cast<std::uint32_t>(i) * 300u, 2600) * 1.5f;
            const float x = kFlowers[i].x + sw, y = kFlowers[i].y;
            const std::uint32_t petal = (i & 1) ? 0xFFFFFF : 0xFFB6C8;
            for (int k = 0; k < 5; ++k) {
                const float an = static_cast<float>(k) * 2 * kPi / 5;
                draw::circle(cv, x + std::cos(an) * 3.2f, y + std::sin(an) * 3.2f, 2.6f, hex(petal));
            }
            draw::circle(cv, x, y, 2.2f, hex(0xFFD23F));
        }
    }

    if (wx) sky::precipitation(cv, w.sky, t);  // in front of the hills, behind the squirrel

    // the haze of bad air drifts past, partly behind and partly in front of the squirrel
    auto haze = [&](int from, int to, int alpha) {
        for (int i = from; i < to; ++i) {
            const std::uint32_t period = 9000u + static_cast<std::uint32_t>(i) * 1700u;
            const float x = -40 + ph(t + static_cast<std::uint32_t>(i) * 2100u, period) * 320;
            const float y = 60 + static_cast<float>((i * 37) % 110) + wave(t + static_cast<std::uint32_t>(i) * 500u, 3000) * 5;
            draw::ellipse(cv, x, y, 26, 15, hex(0x7E7690), 0, alpha);
            draw::ellipse(cv, x + 16, y - 6, 17, 12, hex(0x7E7690), 0, alpha);
        }
    };
    if (m == Mood::Bad) haze(0, 3, 110);

    const Outfit outfit = outfit_for(d.weather_ok, d.weather);
    if (outfit.umbrella) draw_squirrel(cv, 144, 156, 0.78f, m, t, outfit);  // smaller, canopy clear of the top text
    else draw_squirrel(cv, 146, 136, 0.92f, m, t, outfit);

    if (m == Mood::Bad) haze(3, 5, 70);
    if (m == Mood::VeryGood) {
        static constexpr Pt kSpark[] = {{52, 64}, {206, 146}, {40, 150}, {186, 40}};
        for (int i = 0; i < 4; ++i) {
            const float tw = std::fabs(wave(t + static_cast<std::uint32_t>(i) * 330u, 1400));
            draw::sparkle(cv, kSpark[i].x, kSpark[i].y, 3 + tw * 6, hex(i & 1 ? 0xFFFFFF : 0xFFF07A), static_cast<int>(tw * 256));
        }
    }
    if (m == Mood::Sleepy) {  // Zzz
        for (int i = 0; i < 3; ++i) {
            const float k = ph(t + static_cast<std::uint32_t>(i) * 1000u, 3000);
            if (k > 0.85f) continue;
            draw_text(cv, static_cast<int>(174 + k * 12), static_cast<int>(82 - k * 34), 1 + (k > 0.3f) + (k > 0.6f),
                      hex(0xE8E8FF), "Z");
        }
    }

    // soft dark plates behind the text, so it reads on bright snow and sky as well
    auto plate = [&](int y, int h, int w) {
        draw::capsule(cv, 120 - w / 2.0f, y + h / 2.0f, 120 + w / 2.0f, y + h / 2.0f, h / 2.0f + 3, hex(0x1A2233), 70);
    };
    plate(22, 14, 116);
    plate(180, 42, 136);

    const auto white = hex(kWhite);
    char buf[24];
    if (d.sensor_ok) {
        std::snprintf(buf, sizeof buf, "%.1f^C %.0f%%", static_cast<double>(d.reading.temp_c), static_cast<double>(d.reading.rh));
        draw::label(cv, 22, 2, white, buf);
    } else {
        draw::label(cv, 22, 2, white, "NO SENSOR");
    }
    if (m == Mood::Bad && ph(t, 4000) > 0.5f) draw::label(cv, 186, 2, hex(0xFFE0DA), "OPEN WINDOW");
    else {
        const char* lbl = mood_label(m);
        const bool big = text_width(lbl, 3) <= 140;  // long words get the smaller size to fit the round edge
        draw::label(cv, big ? 180 : 186, big ? 3 : 2, white, lbl);
    }
    if (d.sensor_ok && m != Mood::Sleepy) {
        std::snprintf(buf, sizeof buf, "AIR %d", d.quality.score);
        draw::label(cv, 208, 2, white, buf);
    }
}

}  // namespace screen
