// screen: drawing the day page. The squirrel at its desk at home, doing what scene_for() picked, with the real
// weather in the window and the real time on the wall clock. Pure C++.
#include "screen/core/day.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "screen/core/draw.hpp"
#include "screen/core/fun.hpp"
#include "screen/core/pages.hpp"
#include "screen/core/props.hpp"
#include "screen/core/sky.hpp"
#include "screen/core/squirrel.hpp"

namespace screen {

// ---- drawing ---------------------------------------------------------------------------------------------------

namespace {

using draw::hex;
using draw::Pt;

constexpr float kPi = 3.14159265f;
float ph(std::uint32_t t, std::uint32_t period) noexcept { return static_cast<float>(t % period) / static_cast<float>(period); }
float wave(std::uint32_t t, std::uint32_t period) noexcept { return std::sin(2 * kPi * ph(t, period)); }
float smooth(float k) noexcept { k = std::clamp(k, 0.0f, 1.0f); return k * k * (3 - 2 * k); }
// 0 -> 1 -> 0 over [a, b), with eased edges `e` long; 0 outside
float pulse(float k, float a, float b, float e) noexcept { return std::min(smooth((k - a) / e), smooth((b - k) / e)); }

// Same colours as the squirrel (squirrel.cpp).
constexpr std::uint32_t kFur = 0xE0873A, kFurDark = 0xB0602A, kCheek = 0xFF8C9A, kWhite = 0xFFFFFF;

// Where the squirrel sits and how big: design coordinates (body centre at 125,150) map to the screen like this.
constexpr float kCx = 136, kCy = 138, kScale = 0.92f;
constexpr float kDeskTop = 172;                                   // screen y of the desk surface
constexpr float kWinX0 = 160, kWinY0 = 38, kWinX1 = 206, kWinY1 = 94;  // the window (screen)

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
    void box(float x0, float y0, float x1, float y1, std::uint32_t c, int a = 256) const {
        const Pt p[4] = {{X(x0), Y(y0)}, {X(x1), Y(y0)}, {X(x1), Y(y1)}, {X(x0), Y(y1)}};
        draw::polygon(cv, p, 4, hex(c), a);
    }
    void poly(const Pt* d, int n, std::uint32_t c, int a = 256) const {
        Pt q[12];
        n = std::min(n, 12);
        for (int i = 0; i < n; ++i) q[i] = {X(d[i].x), Y(d[i].y)};
        draw::polygon(cv, q, n, hex(c), a);
    }
};

// An arm from the shoulder to the paw, in design coordinates.
void arm(const Pen& p, float sx, float sy, float px, float py) {
    p.line(sx, sy, px, py, 5.5f, kFurDark);
    p.line(sx, sy, px, py, 4.4f, kFur);
    p.circle(px, py, 4.6f, kFur);
}

struct Room { std::uint32_t top, bottom; int dark; };  // wall gradient, 0..256 how dark the room is

Room room_for(Activity a, int hour) noexcept {
    if (a == Activity::Read && hour >= 6 && hour < 17) return {0xF6F0DA, 0xE8DCB6, 0};  // a day off, daylight
    switch (a) {
        case Activity::Sleep: return {0x1C2242, 0x2B3060, 200};
        case Activity::Coffee: return {0xFFE9CF, 0xF7D2AA, 0};
        case Activity::Work: return {0xE6EEF2, 0xD0DDE5, 0};
        case Activity::Lunch: return {0xF6F0DA, 0xE8DCB6, 0};
        case Activity::CloseLaptop: return {0xF6CFA8, 0xE0A57E, 30};
        case Activity::Read: return {0x4A4266, 0x6A5878, 130};
    }
    return {0xE6EEF2, 0xD0DDE5, 0};
}

// The wall, with a hole for the window (the sky was drawn before).
void wall(const Canvas& cv, const Room& r) {
    for (int y = 0; y < gfx::kScreen; ++y) {
        const gfx::Chord ch = gfx::kChord[static_cast<std::size_t>(y)];
        const auto c = hex(draw::lerp_rgb(r.top, r.bottom, y * 256 / (gfx::kScreen - 1)));
        gfx::Rgb565be* row = cv.px.data() + static_cast<std::size_t>(y) * gfx::kScreen;
        if (y >= static_cast<int>(kWinY0) && y < static_cast<int>(kWinY1)) {
            const int x0 = ch.x0, x1 = ch.x1;
            std::fill(row + x0, row + std::clamp(static_cast<int>(kWinX0), x0, x1), c);
            std::fill(row + std::clamp(static_cast<int>(kWinX1), x0, x1), row + x1, c);
        } else {
            std::fill(row + ch.x0, row + ch.x1, c);
        }
    }
}

void window_frame(const Canvas& cv, int dark) {
    const auto frame = hex(draw::lerp_rgb(0xF7F3EC, 0x2A2C48, dark)), shade = hex(draw::lerp_rgb(0xC9BFB2, 0x1A1C30, dark));
    const float mx = (kWinX0 + kWinX1) / 2, my = (kWinY0 + kWinY1) / 2;
    draw::capsule(cv, kWinX0, kWinY0, kWinX1, kWinY0, 2.5f, frame);
    draw::capsule(cv, kWinX0, kWinY1, kWinX1, kWinY1, 2.5f, frame);
    draw::capsule(cv, kWinX0, kWinY0, kWinX0, kWinY1, 2.5f, frame);
    draw::capsule(cv, kWinX1, kWinY0, kWinX1, kWinY1, 2.5f, frame);
    draw::capsule(cv, mx, kWinY0, mx, kWinY1, 1.5f, frame);
    draw::capsule(cv, kWinX0, my, kWinX1, my, 1.5f, frame);
    draw::capsule(cv, kWinX0 - 5, kWinY1 + 4, kWinX1 + 5, kWinY1 + 4, 2.5f, shade);  // sill
}

// The wall clock shows the real time. `spin` turns the hands extra (clock change day, April fool), in hours.
void wall_clock(const Canvas& cv, const Inputs& in, int dark, float spin = 0) {
    constexpr float x = 208, y = 128, r = 14;
    draw::circle(cv, x, y, r + 2.5f, hex(draw::lerp_rgb(0x8A5A36, 0x2A2238, dark)));
    draw::circle(cv, x, y, r, hex(draw::lerp_rgb(0xFFF9EE, 0x5C5A78, dark)));
    const auto ink = hex(draw::lerp_rgb(0x3A2A20, 0x1A1828, dark));
    for (int i = 0; i < 12; i += 3) {
        const float a = static_cast<float>(i) * kPi / 6;
        draw::circle(cv, x + std::sin(a) * (r - 3.5f), y - std::cos(a) * (r - 3.5f), 1.2f, ink);
    }
    if (!in.time_valid) return;
    const float hm = static_cast<float>(in.hour % 12) + static_cast<float>(in.minute) / 60 + spin;
    const float ah = hm * kPi / 6, am = (hm - std::floor(hm)) * 2 * kPi;
    draw::capsule(cv, x, y, x + std::sin(ah) * (r - 7), y - std::cos(ah) * (r - 7), 1.5f, ink);
    draw::capsule(cv, x, y, x + std::sin(am) * (r - 4), y - std::cos(am) * (r - 4), 1.0f, ink);
    draw::circle(cv, x, y, 1.6f, hex(0xD9534F));
}

void desk(const Canvas& cv, int dark) {
    const std::uint32_t top = draw::lerp_rgb(0xD39A62, 0x3A2C3A, dark), front = draw::lerp_rgb(0xA8714A, 0x2C2230, dark);
    draw::vgradient(cv, front, draw::lerp_rgb(front, 0x000000, 70), static_cast<int>(kDeskTop) + 6);
    draw::vgradient(cv, top, top, static_cast<int>(kDeskTop), static_cast<int>(kDeskTop) + 6);
    draw::capsule(cv, 0, kDeskTop + 6, 240, kDeskTop + 6, 1.0f, hex(draw::lerp_rgb(front, 0x000000, 90)));
}

// A mug of coffee centred at (x, y) in design units; steam when hot.
void mug(const Pen& p, float x, float y, bool steam, std::uint32_t t, std::uint32_t colour = 0xD9534F) {
    const std::uint32_t shade = draw::lerp_rgb(colour, 0x000000, 40);
    p.arc(x + 10, y, 5.5f, -kPi / 2, kPi / 2, 1.8f, shade);  // handle
    p.box(x - 10, y - 10, x + 10, y + 11, colour);
    p.ellipse(x, y + 11, 10, 2.5f, colour);
    p.ellipse(x, y - 10, 10, 2.8f, shade);
    p.ellipse(x, y - 10, 8.5f, 2.0f, 0x5A3420);  // coffee
    p.circle(x - 2, y + 1, 2.2f, kWhite, 220);   // a little heart
    p.circle(x + 2, y + 1, 2.2f, kWhite, 220);
    const Pt heart[3] = {{x - 4.1f, y + 1.8f}, {x + 4.1f, y + 1.8f}, {x, y + 6}};
    p.poly(heart, 3, kWhite, 220);
    if (!steam) return;
    for (int i = 0; i < 3; ++i) {
        const float k = ph(t + static_cast<std::uint32_t>(i) * 700u, 2100);
        const float sx = x + static_cast<float>(i - 1) * 5 + wave(t + static_cast<std::uint32_t>(i) * 300u, 1400) * 2.5f;
        p.circle(sx, y - 15 - k * 22, 2.4f + k * 1.5f, kWhite, static_cast<int>(170 * (1 - k)));
    }
}

void laptop_lid(const Pen& p, float open) {  // open 1 = upright, 0 = shut
    constexpr float hinge = 186, H = 56;
    const float top = hinge - H * open;
    p.box(96, hinge - 2, 154, hinge + 3, 0xB9BFC8);  // base
    if (open < 0.04f) { p.box(96, hinge - 5, 154, hinge - 1, 0x8E96A3); return; }
    p.box(99, top, 151, hinge, 0x8E96A3);
    p.box(101.5f, top + 2.5f * open, 148.5f, hinge - 2.5f, 0xA3ABB7);
    if (open > 0.5f) {  // an acorn logo on the lid
        const float ly = (top + hinge) / 2;
        p.ellipse(125, ly + 2.5f, 5, 6, 0xE8ECF2);
        p.ellipse(125, ly - 2.5f, 6.2f, 3.2f, 0xE8ECF2);
        p.line(125, ly - 5, 126.5f, ly - 8, 1.0f, 0xE8ECF2);
    }
}

void bowl(const Pen& p, float x, float y) {
    Pt b[9];
    for (int i = 0; i <= 8; ++i) {  // lower half of an ellipse
        const float a = kPi * static_cast<float>(i) / 8;
        b[i] = {x + 17 * std::cos(a), y + 11 * std::sin(a)};
    }
    p.poly(b, 9, 0x5E9BD6);
    p.ellipse(x, y, 17, 3.5f, 0x3F78B0);
    p.ellipse(x, y, 15, 2.6f, 0xF2A65A);  // pumpkin soup
    p.line(x - 9, y + 5, x + 9, y + 5, 1.0f, kWhite, 150);
}

void lamp(const Canvas& cv, std::uint32_t t) {  // on the left of the desk, in front of the tail
    const float flick = 1 + wave(t, 5300) * 0.03f;
    draw::circle(cv, 70, 128, 66 * flick, hex(0xFFE3A0), 45);
    draw::circle(cv, 70, 128, 38 * flick, hex(0xFFE9B0), 45);
}

void lamp_stand(const Canvas& cv) {
    draw::ellipse(cv, 40, kDeskTop + 1, 13, 3.5f, hex(0x2E3440));
    draw::capsule(cv, 40, kDeskTop, 46, 126, 1.8f, hex(0x2E3440));
    draw::capsule(cv, 46, 126, 58, 108, 1.8f, hex(0x2E3440));
    const Pt shade[4] = {{62, 98}, {78, 108}, {70, 124}, {50, 112}};
    draw::polygon(cv, shade, 4, hex(0x3F8F7A));
    draw::circle(cv, 68, 119, 3.5f, hex(0xFFF4C8));
}


void nightcap(const Pen& p) {
    const Pt cap[8] = {{94, 92}, {98, 72}, {116, 58}, {140, 58}, {162, 70}, {178, 96}, {166, 88}, {156, 92}};
    p.poly(cap, 8, 0x4D5BB5);
    p.line(118, 64, 150, 64, 1.6f, 0x7E8AD8, 200);
    p.line(102, 78, 156, 78, 1.6f, 0x7E8AD8, 200);
    p.line(95, 91, 155, 91, 4.2f, 0xEDEFFB);
    p.circle(178, 98, 6, 0xEDEFFB);
}

// Tucked in up to the chest; `raise` pulls it higher (px), up to the nose in a thunderstorm.
void blanket(const Canvas& cv, std::uint32_t t, float raise) {
    const float breathe = wave(t, 3500) * 1.5f - raise;  // rises and falls with the sleeping squirrel
    draw::ellipse(cv, 120, 262 + breathe, 176, 124, hex(0xC9CEEA));
    draw::ellipse(cv, 120, 262 + breathe, 174, 122, hex(0xEDEFFB));  // the folded-over sheet
    draw::ellipse(cv, 120, 272 + breathe, 174, 122, hex(0x3E4A8C));
    draw::ellipse(cv, 120, 274 + breathe, 172, 120, hex(0x5868B8));
    for (int i = 0; i < 4; ++i)  // quilt stitching
        draw::capsule(cv, 48 + i * 48.0f, 160 + breathe, 48 + i * 48.0f, 240, 0.8f, hex(0x3E4A8C), 160);
    draw::capsule(cv, 0, 196 + breathe, 240, 196 + breathe, 0.8f, hex(0x3E4A8C), 160);
}

// A knitted blanket around the shoulders, open at the front, for a cold room.
void shawl(const Pen& p, std::uint32_t t) {
    const float sway = wave(t, 3000) * 1.0f;
    constexpr std::uint32_t kWool = 0x3E9C95, kWoolDark = 0x2C736E, kStripe = 0xCDEDE8;
    for (const int side : {-1, 1}) {
        const float sx = static_cast<float>(side);
        const Pt flap[5] = {{125 + sx * 10, 120}, {125 + sx * 33, 122}, {125 + sx * 36 + sway, 176},
                            {125 + sx * 22 + sway, 180}, {125 + sx * 13, 132}};
        const Pt edge[5] = {{125 + sx * 9, 118}, {125 + sx * 35, 120}, {125 + sx * 38 + sway, 178},
                            {125 + sx * 21 + sway, 182}, {125 + sx * 12, 132}};
        p.poly(edge, 5, kWoolDark);
        p.poly(flap, 5, kWool);
        for (int i = 0; i < 3; ++i) {  // knit stripes
            const float y = 140 + static_cast<float>(i) * 12;
            p.line(125 + sx * 17, y, 125 + sx * 33 + sway * (y - 120) / 60, y + 1, 1.3f, kStripe, 200);
        }
        p.line(125 + sx * 24 + sway, 181, 125 + sx * 24 + sway, 186, 1.0f, kStripe);  // a fringe
    }
    p.line(110, 121, 140, 121, 5.5f, kWoolDark);  // collar round the neck
    p.line(110, 120, 140, 120, 4.5f, kWool);
}

// An acorn rolling along (angle = how far it has turned), centred at (x, y).
void rolling_acorn(const Canvas& cv, float x, float y, float angle) {
    const float c = std::cos(angle), s = std::sin(angle);
    draw::ellipse(cv, x, y, 5.2f, 6.2f, hex(0x8A5A2C), angle);
    draw::ellipse(cv, x, y, 4.4f, 5.4f, hex(0xB37B40), angle);
    draw::ellipse(cv, x + s * 3.6f, y - c * 3.6f, 5.6f, 3.0f, hex(0x6E4220), angle);
    draw::capsule(cv, x + s * 6, y - c * 6, x + s * 8.5f, y - c * 8.5f, 0.9f, hex(0x6E4220));
}

// A New Year's Eve sparkler held up in the paw at (x, y), design units.
void sparkler(const Pen& p, float x, float y, std::uint32_t t) {
    p.line(x, y, x + 6, y - 30, 1.0f, 0x6A6A72);
    const float tx = x + 6, ty = y - 30;
    for (int i = 0; i < 9; ++i) {
        const float a = static_cast<float>(i) * 0.7f + ph(t, 400) * 6.3f;
        const float r = 5 + 6 * ph(t + static_cast<std::uint32_t>(i) * 97u, 330);
        p.line(tx, ty, tx + std::cos(a) * r, ty + std::sin(a) * r, 0.5f, i & 1 ? 0xFFF4B0 : 0xFFD24A, 220);
    }
    draw::sparkle(p.cv, p.X(tx), p.Y(ty), 6 * p.s, hex(kWhite));
}

// Lederhosen braces for the Oktoberfest.
void braces(const Pen& p) {
    p.line(110, 126, 113, 182, 2.4f, 0x4E5B2E);
    p.line(140, 126, 137, 182, 2.4f, 0x4E5B2E);
    p.line(113, 146, 137, 146, 2.6f, 0x4E5B2E);
    p.circle(125, 146, 3.2f, 0xF4F1E6);  // an edelweiss on the bar
    p.circle(125, 146, 1.3f, 0xE8C34A);
}

// A yawn: the mouth opens wide for a moment every few seconds.
void yawn(const Pen& p, std::uint32_t t) {
    const float k = pulse(ph(t, 5200), 0.55f, 0.85f, 0.08f);
    if (k < 0.05f) return;
    p.ellipse(125, 117, 5.5f * k + 1, 7.5f * k + 1, 0x7A2430);
    p.ellipse(125, 121 * 1.0f, 3.5f * k, 2.5f * k, 0xF07C8A);
}

const char* situation_label(Situation s, const Inputs& in) noexcept {
    switch (s) {
        case Situation::None: return nullptr;
        case Situation::Party: return in.month == 1 ? "HAPPY NEW YEAR" : "COUNTDOWN";
        case Situation::Smell: return "STINKY AIR!";
        case Situation::Storm: return "THUNDER!";
        case Situation::AirOut: return "AIR OUT";
        case Situation::CoolDown: return "COOL OFF";
        case Situation::Yawn: return "STUFFY AIR";
        case Situation::HotRoom: return "TOO WARM";
        case Situation::ColdRoom: return "CHILLY";
        case Situation::Humid: return "TOO HUMID";
        case Situation::DryAir: return "DRY AIR";
        case Situation::Umbrella: return "TAKE UMBRELLA";
        case Situation::Cocoa: return "COSY EVENING";
        case Situation::Watering: return "WATERING";
    }
    return nullptr;
}

const char* occasion_label(Occasion o, const Inputs& in) noexcept {
    switch (o) {
        case Occasion::None: return nullptr;
        case Occasion::Birthday: return "HAPPY BIRTHDAY";
        case Occasion::Anniversary: return "ANNIVERSARY";
        case Occasion::Christmas: return "MERRY XMAS";
        case Occasion::NewYearsEve: return "NEW YEAR'S EVE";
        case Occasion::NewYear: return "HAPPY NEW YEAR";
        case Occasion::Easter: return "HAPPY EASTER";
        case Occasion::Nikolaus: return "NIKOLAUS";
        case Occasion::StMartin: return "ST. MARTIN";
        case Occasion::Halloween: return "HALLOWEEN";
        case Occasion::ClockChange: return in.month == 10 ? "CLOCKS GO BACK" : "CLOCKS FORWARD";
        case Occasion::Oktoberfest: return "O'ZAPFT IS!";
        case Occasion::SquirrelDay: return "SQUIRREL DAY";
        case Occasion::Valentine: return "VALENTINE";
        case Occasion::AprilFool: return "APRIL FOOL!";
    }
    return nullptr;
}

// `say`: a pun or a gag's words, shown instead of the usual first line while it lasts.
void caption(const Canvas& cv, const Scene& sc, const Inputs& in, const char* say = nullptr) {
    static constexpr const char* kAdvent[] = {"", "1. ADVENT", "2. ADVENT", "3. ADVENT", "4. ADVENT"};
    const char* lines[6] = {};
    int n = 0;
    if (say) {
        const bool big = text_width(say, 3) <= 150;
        const float half = static_cast<float>(text_width(say, big ? 3 : 2)) / 2 - 6;
        draw::capsule(cv, 120 - half, 197, 120 + half, 197, 14, hex(0x1A2233), 60);
        draw::label(cv, big ? 187 : 190, big ? 3 : 2, hex(0xFFF2B8), say);
        return;
    }
    const char* first = situation_label(sc.situation, in);
    lines[n++] = first ? first : activity_label(sc.activity);
    char sleeps[32] = "", what[32] = "";
    if (sc.countdown > 0) {
        std::snprintf(sleeps, sizeof sleeps, sc.countdown == 1 ? "1 SLEEP TO GO" : "%d SLEEPS TO GO", sc.countdown);
        if (sc.countdown_to == Occasion::Birthday) {
            const char* name = in.schedule.birthday_name;
            if (name[0] && std::strlen(name) <= 4) std::snprintf(what, sizeof what, "%s'S BIRTHDAY", name);
            else std::snprintf(what, sizeof what, "%s", name[0] ? name : "BIRTHDAY");
        } else {
            std::snprintf(what, sizeof what, "%s", sc.countdown_to == Occasion::Anniversary ? "OUR DAY" : "CHRISTMAS");
        }
        lines[n++] = sleeps;
        lines[n++] = what;
    }
    if (in.plant.grew) lines[n++] = "NEW LEAF!";
    if (const char* o = occasion_label(sc.occasion, in); o && std::string_view(o) != std::string_view(lines[0])) lines[n++] = o;
    if (sc.occasion == Occasion::Birthday && in.schedule.birthday_name[0] && n < 6) lines[n++] = in.schedule.birthday_name;
    if (sc.occasion == Occasion::Anniversary && n < 6) lines[n++] = "LOVE YOU!";
    if (sc.advent > 0 && in.weekday == 0 && n < 6) lines[n++] = kAdvent[sc.advent];
    if (sc.friday_party && n < 6) lines[n++] = "WEEKEND!";
    if (sc.monday && n < 6) lines[n++] = "MONDAY...";
    const char* lbl = lines[(in.t_ms / 4000u) % static_cast<unsigned>(n)];
    const bool big = text_width(lbl, 3) <= 150;
    const float half = static_cast<float>(text_width(lbl, big ? 3 : 2)) / 2 - 6;
    draw::capsule(cv, 120 - half, 197, 120 + half, 197, 14, hex(0x1A2233), 60);
    draw::label(cv, big ? 187 : 190, big ? 3 : 2, hex(kWhite), lbl);
}

// What stands on the right of the desk for the special day (and the advent wreath).
void desk_decor(const Canvas& cv, const Scene& sc, const Inputs& in, std::uint32_t t) {
    namespace P = props;
    constexpr float x = 206, y = kDeskTop + 1;
    const bool evening = in.hour >= 17 || in.hour < 6;
    switch (sc.occasion) {
        case Occasion::Halloween: P::pumpkin(cv, x - 2, y, 1.35f, t, evening); break;
        case Occasion::Nikolaus: P::boot(cv, x + 2, y, 1.4f); break;
        case Occasion::Christmas: P::xmas_tree(cv, x - 10, y, 1.9f, t); break;
        case Occasion::Birthday: P::cake(cv, x, y, 1.6f, t, false); break;
        case Occasion::SquirrelDay: P::cake(cv, x, y, 1.6f, t, true); break;
        case Occasion::Easter: P::egg_basket(cv, x, y, 1.6f); break;
        case Occasion::Valentine:
        case Occasion::Anniversary: P::vase_rose(cv, x, y, 1.6f, t); break;
        case Occasion::Oktoberfest: P::pretzel(cv, x, y, 1.6f); break;
        default: break;
    }
    if (sc.advent > 0) {
        const bool beside = sc.occasion == Occasion::Nikolaus;  // shares the desk with the boot
        P::advent_wreath(cv, beside ? x - 30 : x, y, beside ? 1.3f : 1.6f, t, sc.advent);
    }
}

}  // namespace

void render_day(const Canvas& cv, const Inputs& in) noexcept {
    namespace P = props;
    const std::uint32_t t = in.t_ms;
    const Scene sc = scene_for(in);
    const Activity act = sc.activity;
    const Situation sit = sc.situation;
    const Occasion occ = sc.occasion;
    const bool party = sit == Situation::Party;
    const bool bed = (act == Activity::Sleep || sit == Situation::Storm) && !party;
    Room room = room_for(bed ? Activity::Sleep : act, in.hour);
    if (party) room = {0x2A2350, 0x45306A, 150};
    const P::Win win{kWinX0, kWinY0, kWinX1, kWinY1};
    const fun::Visit vis = fun::visit(t, in.seed, fun::allowed_visitors(Page::Day, in));

    // the real sky in the window, and what happens on the glass
    const auto& w = in.weather;
    const weather::Sky sk = in.weather_ok ? weather::now_sky(w) : weather::Sky::Clear;  // what the window shows right now
    const bool day = party ? false : (in.weather_ok ? w.is_day : room.dark < 100);
    const float moon = in.time_valid ? cal::moon_phase(in.year, in.month, in.day, in.hour) : -1.0f;
    sky::draw(cv, sk, day, t, (kWinX0 + kWinX1) / 2, (kWinY0 + kWinY1) / 2 - 2, 0.42f, sky::colors(sk, day, t), true, moon);
    if (const int a = fun::rainbow_alpha(in); a > 0) P::rainbow(cv, kWinX0 - 4, kWinY1 + 30, 70, 4.5f, a);  // the wall clips it
    wall(cv, room);
    if (vis.who == fun::Visitor::Owl) P::owl(cv, (kWinX0 + kWinX1) / 2 + 7, kWinY1 - 1, 0.85f, t);
    if (vis.who == fun::Visitor::ShootingStar) fun::shooting_star(cv, kWinX0, kWinY0, kWinX1, kWinY1, vis);
    if (party) P::fireworks(cv, win, t);
    if (in.weather_ok) {
        if (sk == weather::Sky::Snow && w.is_day) P::snowman(cv, win, t);
        if (sk == weather::Sky::Fog) P::glass_fog(cv, win, t);
        if (sk == weather::Sky::Drizzle || sk == weather::Sky::Rain || sk == weather::Sky::Thunder) P::glass_rain(cv, win, t);
        if (w.min_c < 0 && in.hour < 11) P::glass_frost(cv, win, t);
        if (w.wind_kmh >= 30 && sk != weather::Sky::Snow) P::leaves_flying(cv, win, t);
    }
    if (sit == Situation::Humid) P::glass_steam(cv, win, t);
    if (sit == Situation::AirOut || sit == Situation::CoolDown) P::window_open(cv, win, t, room.dark);
    else window_frame(cv, room.dark);
    P::memory_plant(cv, kWinX0 + 10, kWinY1 + 2, 1.3f, t, in.plant.leaves, in.plant.droop, in.plant.flower);
    if (vis.who == fun::Visitor::Snail) {
        const float k = vis.k - std::sin(2 * kPi * vis.k * 3) / (2 * kPi * 3) * 0.8f;
        P::snail(cv, kWinX1 - 2 - k * (kWinX1 - kWinX0 - 16), kWinY1 + 2, 0.8f, -1, t);
    }
    if (vis.who == fun::Visitor::Bird) fun::bird_on_sill(cv, kWinX0 + 4, kWinX1 - 2, kWinY1 + 2, vis, t);
    if (sit == Situation::Humid) P::mushroom(cv, kWinX1 - 6, kWinY1 + 2, 0.55f);
    if (occ == Occasion::StMartin && (in.hour >= 16 || in.hour < 6)) P::lantern(cv, (kWinX0 + kWinX1) / 2, kWinY0 + 4, 0.9f, t);

    // the wall: clock (spun on special days), thermometer, garland
    float spin = 0;
    if (occ == Occasion::ClockChange) spin = (in.month == 10 ? -1.0f : 1.0f) * smooth(ph(t, 6000) / 0.5f);
    if (occ == Occasion::AprilFool) spin = -ph(t, 4000) * 2;  // runs backwards
    wall_clock(cv, in, room.dark, spin);
    if (sit == Situation::HotRoom || sit == Situation::ColdRoom)
        P::thermometer(cv, 34, 116, 1.3f, sit == Situation::HotRoom ? 0.9f : 0.15f, sit == Situation::HotRoom);
    if (occ == Occasion::Birthday || occ == Occasion::SquirrelDay || occ == Occasion::NewYearsEve || party)
        P::bunting(cv, 22, t, false);
    if (occ == Occasion::Anniversary || occ == Occasion::Valentine) P::bunting(cv, 22, t, true);
    if (in.bond.gifts) P::gift_shelf(cv, 46, 48, 42, in.bond.gifts);  // the owner's gifts
    if (sc.countdown > 0) P::wall_calendar(cv, 37, 76, 1.0f, sc.countdown, sc.countdown_to != Occasion::Christmas);

    // who owns the left of the desk: a prop of the situation, else the reading lamp, else yesterday's mug
    const bool plant_left = sit == Situation::DryAir || sit == Situation::Watering;
    const bool lamp_on = reading_lamp(sc, in);
    if (lamp_on) lamp(cv, t);

    // the face follows the air, but calmer than on the air page (no hopping at the desk)
    Mood mood = in.sensor_ok ? mood_for(in.quality.level) : Mood::Sleepy;
    if (mood == Mood::VeryGood) mood = Mood::Good;
    const float cycle = ph(t, 6000);
    if (act == Activity::Sleep && !party && sit != Situation::Storm) mood = Mood::Sleepy;
    if (sit == Situation::Storm) mood = Mood::Bad;
    if (sit == Situation::Yawn || sit == Situation::Smell) mood = Mood::Okay;
    if (party || occ == Occasion::Birthday || occ == Occasion::SquirrelDay || occ == Occasion::Anniversary) {
        if (mood != Mood::Bad && mood != Mood::Sleepy) mood = Mood::Good;
    }
    // a gag now and then (the time-window ones only in their window)
    const fun::GagNow gag = fun::gag(t, in.seed, allowed_gags(sc, in));
    const bool nodding = gag.gag == fun::Gag::NodOff && gag.k < 0.75f;
    const bool startled = gag.gag == fun::Gag::NodOff && gag.k >= 0.75f;
    const float reach = gag.gag == fun::Gag::Stretch ? pulse(gag.k, 0.1f, 0.9f, 0.15f) : 0.0f;
    if (nodding) mood = Mood::Sleepy;
    if (startled) mood = Mood::Good;

    const bool stretch = (act == Activity::CloseLaptop && cycle > 0.45f && mood != Mood::Bad && !bed) || reach > 0.05f;
    if (stretch || party) mood = Mood::VeryGood;

    // in a thunderstorm the squirrel flinches at each flash
    const bool flash = in.weather_ok && sky::lightning(sk, t);
    const float cx = kCx + (sit == Situation::Storm && flash ? wave(t, 90) * 2 : 0);

    Outfit o;
    o.paws_free = true;
    o.wolf = occ == Occasion::Halloween && (in.hour >= 17 || in.hour < 5) && !bed;
    if (bed) {  // pillow behind, in bed
        draw::ellipse(cv, kCx, kCy - 30, 50, 28, hex(0xAEB4D8));
        draw::ellipse(cv, kCx, kCy - 32, 47, 25, hex(0xC9CEEA));
    }
    fun::Quirks q = fun::quirks(t, in.seed);
    q.hop = 0;  // it sits at the desk
    if (sc.howl_night && act == Activity::Read && !bed && !party) q.howl = fun::howl(t, in.seed);
    // the eyes follow whatever flies or rolls past, and on a Friday afternoon the clock
    const float plane_x = -20 + 280 * gag.k, acorn_x = 60 + 200 * gag.k;
    if (gag.gag == fun::Gag::Plane) q.look = std::clamp((plane_x - 136) / 50, -1.0f, 1.0f);
    if (gag.gag == fun::Gag::Acorn) q.look = std::clamp((acorn_x - 136) / 50, -1.0f, 1.0f);
    if (gag.gag == fun::Gag::ClockWatch) q.look = pulse(gag.k, 0, 1, 0.1f);
    const float cy = kCy + (nodding ? 2.5f * smooth(gag.k / 0.3f) : 0.0f);  // the head sinks while it nods off
    draw_squirrel(cv, cx, cy, kScale, mood, t, o, q);
    const Bob b = squirrel_bob(mood, t);
    const Pen p{cv, cx + b.dx * kScale, cy + b.dy * kScale, kScale};  // moves with the squirrel
    const Pen still{cv, kCx, kCy, kScale};                             // the desk does not

    // hats and clothes
    const float brim_x = p.X(125), brim_y = p.Y(81), brim_w = 48;
    if (bed) nightcap(p);
    else if (party || occ == Occasion::Birthday || sc.friday_party) P::party_hat(cv, brim_x, brim_y, brim_w, t);
    else if (occ == Occasion::SquirrelDay) P::crown(cv, brim_x, brim_y, brim_w, t);
    else if (occ == Occasion::Christmas || occ == Occasion::Nikolaus) P::santa_hat(cv, brim_x, brim_y, brim_w);
    if (occ == Occasion::Oktoberfest && !bed) braces(p);
    if (sit == Situation::Yawn) yawn(p, t);
    if (q.howl > 0.05f) fun::howl_text(cv, p.X(130), p.Y(116), 168, 100, q.howl, t);
    if (startled) draw::label(cv, static_cast<int>(p.Y(52)), 3, hex(0xFFE27A), "!");
    if (gag.gag == fun::Gag::Sigh) {  // a little puff of air drifts away
        const float k = gag.k;
        draw::ellipse(cv, p.X(112) - 30 * k, p.Y(118) - 8 * k, 5 + 6 * k, 3 + 4 * k, hex(0xE8EEF5), 0, static_cast<int>(200 * (1 - k)));
    }

    if (bed) {
        if (sit == Situation::Storm) {  // under the blanket up to the nose, paws holding the edge
            blanket(cv, t, 30);
            p.circle(110, 122, 5, kFur);
            p.circle(140, 122, 5, kFur);
            if (flash) draw::circle(cv, 120, 120, 170, hex(kWhite), 70);
        } else {
            blanket(cv, t, 0);
            for (int i = 0; i < 3; ++i) {  // Zzz
                const float k = ph(t + static_cast<std::uint32_t>(i) * 1000u, 3000);
                if (k > 0.85f) continue;
                draw_text(cv, static_cast<int>(184 + k * 12), static_cast<int>(112 - k * 30), 1 + (k > 0.3f) + (k > 0.6f),
                          hex(0xE8E8FF), "Z");
            }
        }
        caption(cv, sc, in);
        return;
    }

    // Situations that keep the paws busy replace the activity's hand-held things; the desk things stay.
    const bool own_paws = party || sit == Situation::Smell || sit == Situation::HotRoom || sit == Situation::ColdRoom ||
                          sit == Situation::DryAir || sit == Situation::Watering;
    if (sit == Situation::ColdRoom) shawl(p, t);

    if (own_paws) {
        switch (sit) {
            case Situation::Party:
                arm(p, 144, 128, 152, 96);
                sparkler(p, 152, 96, t);
                arm(p, 106, 128, 104, 170);
                break;
            case Situation::Smell:  // holds its nose, waves the other paw
                arm(p, 144, 128, 131, 110);
                arm(p, 106, 128, 92, 100 + wave(t, 300) * 5);
                break;
            case Situation::HotRoom: {  // fans itself with a big leaf, held out to the side
                const float a = 0.55f + wave(t, 600) * 0.35f;
                arm(p, 144, 128, 166, 128);
                P::leaf_fan(cv, p.X(166), p.Y(128), 1.1f, a);
                p.circle(166, 128, 4.6f, kFur);
                arm(p, 106, 128, 104, 170);
                P::sweat_drop(cv, p.X(150), p.Y(84), 1.2f, t);
                break;
            }
            case Situation::ColdRoom: {  // tea in both paws, a sip now and then
                const float sip = pulse(cycle, 0.62f, 0.95f, 0.08f);
                P::tea_cup(cv, p.X(125), p.Y(140 - sip * 20), 1.4f, t);
                arm(p, 106, 128, 112, 146 - sip * 20);
                arm(p, 144, 128, 138, 146 - sip * 20);
                break;
            }
            case Situation::DryAir: {  // a glass of water, and the mister for the plant
                const float sip = pulse(cycle, 0.62f, 0.95f, 0.08f);
                P::water_glass(cv, p.X(140), p.Y(140 - sip * 18), 1.3f);
                arm(p, 144, 128, 144, 150 - sip * 18);
                P::mister(cv, p.X(84), p.Y(140), 1.3f, t);
                arm(p, 106, 128, 90, 146);
                break;
            }
            case Situation::Watering: {
                const float tilt = pulse(ph(t, 5000), 0.25f, 0.85f, 0.15f);
                P::watering_can(cv, p.X(80), p.Y(118), 1.4f, tilt, t);
                arm(p, 106, 128, 90, 118);
                arm(p, 144, 128, 142, 168);
                break;
            }
            default: break;
        }
        desk(cv, room.dark);
        // the laptop stays where it does not hide what the paws hold
        if ((act == Activity::Work || act == Activity::CloseLaptop) && (sit == Situation::Smell || sit == Situation::HotRoom))
            laptop_lid(still, act == Activity::Work ? 1.0f : 0.0f);
        if (act == Activity::Lunch) bowl(still, 125, 175);
        if (act == Activity::Coffee && !party) mug(still, 45, 176, true, t);
    } else {
        switch (act) {
            case Activity::Coffee: {  // holds the mug, a sip every few seconds
                const float sip = pulse(cycle, 0.62f, 0.95f, 0.08f);
                const float my = 140 - sip * 20;
                const std::uint32_t colour = occ == Occasion::Valentine ? 0xF27BA0 : 0xD9534F;
                if (sc.monday) {  // the big Monday mug
                    mug(Pen{cv, p.X(125), p.Y(my) - 4, kScale * 1.4f}, 125, 150, true, t, colour);
                    arm(p, 106, 128, 110, my + 6);
                    arm(p, 144, 128, 140, my + 6);
                } else {
                    mug(p, 125, my, true, t, colour);
                    arm(p, 106, 128, 113, my + 4);
                    arm(p, 144, 128, 137, my + 4);
                }
                desk(cv, room.dark);
                if (sit == Situation::Umbrella) P::closed_umbrella(cv, 44, kDeskTop + 1, 1.8f);
                break;
            }
            case Activity::Work: {  // laptop open, screen light on the face, yesterday's coffee on the desk
                const float glow = 40 + wave(t, 2300) * 10 + ((t / 180) % 7 == 0 ? 14 : 0);
                p.ellipse(125, 106, 30, 26, 0xA8DCFF, 0, static_cast<int>(glow));
                if (reach > 0.05f) {  // a happy stretch at the open window
                    arm(p, 106, 128, 96 - reach * 4, 128 - reach * 58);
                    arm(p, 144, 128, 154 + reach * 4, 128 - reach * 58);
                } else {
                    arm(p, 106, 128, 108, 168);
                    if (gag.gag != fun::Gag::Pen) arm(p, 144, 128, 142, 168);  // with the pen it comes in front of the laptop
                }
                desk(cv, room.dark);
                mug(still, 45, 176, false, t);
                laptop_lid(still, 1.0f);
                if (gag.gag == fun::Gag::Pen && reach <= 0.05f) {  // twirls a pen
                    const float spin = static_cast<float>(t) / 70.0f;
                    P::pen(cv, p.X(152), p.Y(144), 1.1f, spin);
                    arm(p, 144, 128, 150, 150);
                }
                break;
            }
            case Activity::CloseLaptop: {  // the lid swings shut, then a big stretch
                const float open = 1 - smooth(cycle / 0.35f);
                if (stretch) {
                    const float up = pulse(cycle, 0.45f, 1.0f, 0.12f);
                    arm(p, 106, 128, 96 - up * 4, 128 - up * 58);
                    arm(p, 144, 128, 154 + up * 4, 128 - up * 58);
                } else {
                    arm(p, 106, 128, 108, 168);
                    arm(p, 144, 128, 142, 168);
                }
                desk(cv, room.dark);
                laptop_lid(still, open);
                break;
            }
            case Activity::Lunch: {  // spoonful after spoonful, cheeks full while chewing
                const float lift = pulse(ph(t, 3200), 0.15f, 0.55f, 0.12f);
                const bool stuffed = gag.gag == fun::Gag::Cheeks;  // hamster cheeks!
                const float chew = stuffed ? 1.0f : pulse(ph(t, 3200), 0.5f, 1.0f, 0.06f);
                if (chew > 0.05f) {
                    const float puff = stuffed ? 6 + wave(t, 260) * 1.2f : 2 + chew * (2 + wave(t, 300) * 1.0f);
                    for (const float x : {101.0f, 149.0f}) {
                        p.circle(x, 113, 4 + puff, kFur);
                        p.circle(x, 113, 3 + puff, kCheek, 120);
                    }
                }
                desk(cv, room.dark);
                bowl(still, 125, 175);
                const float sx = 132 - lift * 7, sy = 173 - lift * 57;  // spoon bowl
                p.line(sx, sy, sx + 18, sy + 12, 1.6f, 0x7A8494);
                p.ellipse(sx, sy, 5, 3, 0x7A8494);
                p.ellipse(sx, sy - 0.5f, 3.6f, 1.8f, 0xF2A65A, 0, static_cast<int>(256 * lift));  // a spoonful
                arm(p, 144, 128, sx + 18, sy + 12);
                arm(p, 106, 128, 102, 170);
                break;
            }
            case Activity::Read:
            case Activity::Sleep: {  // an open book, a page turns every 8 s
                constexpr float y0 = 122, y1 = 150;
                const Pt left[4] = {{125, y0 + 6}, {100, y0}, {100, y1 - 6}, {125, y1}};
                const Pt right[4] = {{125, y0 + 6}, {150, y0}, {150, y1 - 6}, {125, y1}};
                p.line(101, y0 - 1.5f, 124, y0 + 4.5f, 1.4f, 0xFFF8E8);  // page edges over the cover
                p.line(126, y0 + 4.5f, 149, y0 - 1.5f, 1.4f, 0xFFF8E8);
                p.poly(left, 4, 0x3B6EA5);
                p.poly(right, 4, 0x34639A);
                p.line(125, y0 + 6, 125, y1, 1.0f, 0x24456E);
                p.box(108, y0 + 9, 117, y0 + 12, 0xF2C14E);  // a title on the cover
                const float k = ph(t, 8000);
                if (k > 0.82f) {  // the turning page
                    const float a = kPi * smooth((k - 0.82f) / 0.18f);
                    const Pt pg[3] = {{125, y0 + 5}, {125 + std::cos(a) * 23, y0 + 1 - std::sin(a) * 20}, {125, y0 - 1}};
                    p.poly(pg, 3, 0xFFF8E8);
                }
                if (reach > 0.05f) {
                    arm(p, 106, 128, 96 - reach * 4, 128 - reach * 58);
                    arm(p, 144, 128, 154 + reach * 4, 128 - reach * 58);
                } else {
                    arm(p, 106, 128, 101, 141);
                    arm(p, 144, 128, 149, 141);
                }
                desk(cv, room.dark);
                if (sit == Situation::Cocoa) P::cocoa_mug(cv, 98, kDeskTop - 9, 1.5f, t);
                break;
            }
        }
    }
    if (sit == Situation::Smell) {
        P::stink_lines(cv, 60, kDeskTop - 4, t);
        P::stink_lines(cv, 180, kDeskTop - 2, t + 700);
    }
    if (sit == Situation::Humid) P::sweat_drop(cv, p.X(150), p.Y(84), 1.2f, t);
    if (plant_left) P::plant(cv, 46, kDeskTop + 1, 1.7f, t, sit == Situation::Watering ? ph(t, 5000) > 0.5f : ph(t, 3000) > 0.3f);
    if (lamp_on) lamp_stand(cv);
    desk_decor(cv, sc, in, t);
    if (occ == Occasion::NewYear || occ == Occasion::Birthday) P::confetti(cv, t);
    if (occ == Occasion::Valentine || occ == Occasion::Anniversary) P::hearts_rising(cv, p.X(160), p.Y(96), t);
    switch (vis.who) {  // little visitors
        case fun::Visitor::Ladybug: fun::ladybug(cv, 22, 218, kDeskTop + 3, vis, t); break;
        case fun::Visitor::Moth: fun::moth(cv, 66, 116, vis, t); break;
        case fun::Visitor::Butterfly: fun::butterfly(cv, 20, 40, 220, 160, vis, t); break;
        default: break;
    }
    if (gag.gag == fun::Gag::Plane) {  // a paper plane glides through the room
        const float k = gag.k, y = 64 + 56 * k + std::sin(k * 2 * kPi) * 10;
        P::paper_plane(cv, plane_x, y, 1.3f, 0.35f + std::cos(k * 2 * kPi) * 0.25f);
    }
    if (gag.gag == fun::Gag::Acorn) {  // an acorn rolls off the desk
        const float edge = 226, x = acorn_x;
        const float y = x < edge ? kDeskTop - 4 : kDeskTop - 4 + (x - edge) * (x - edge) * 0.08f;
        rolling_acorn(cv, x, y, (x - 60) / 4.5f);
    }
    const int lvl = level(in.bond);
    const float gift = lvl >= 4 && !bed ? fun::present_for_you(t, in.seed) : -1.0f;  // family: a present for you
    if (gift >= 0) P::present(cv, 64, kDeskTop + 2, 1.6f, t);
    char hi[24] = "";
    if (in.pet_name[0] && lvl >= 2) std::snprintf(hi, sizeof hi, "HI, I'M %s", in.pet_name);
    const char* say = nullptr;
    if (gift >= 0) say = "A GIFT FOR YOU!";
    else if (gag.gag == fun::Gag::ClockWatch) say = "ALMOST WEEKEND";
    else if (gag.gag == fun::Gag::Sigh) say = "SIGH...";
    else if (startled) say = "I'M AWAKE!";
    else if (gag.gag == fun::Gag::Cheeks) say = "NOM NOM NOM";
    else if (sit == Situation::None && occ == Occasion::None && sc.countdown == 0)
        say = fun::pun(t, in.seed, static_cast<int>(act));
    if (say && hi[0] && gift < 0 && gag.gag == fun::Gag::None && (t / 6000u) % 2 == 0 && text_width(hi, 2) <= 184)
        say = hi;  // now and then it says its name instead of a pun
    caption(cv, sc, in, say);
}

}  // namespace screen
