// screen: the day page. The squirrel at its desk at home, doing what fits the time of day, with the real weather
// in the window and the real time on the wall clock. Pure C++.
#include "screen/core/day.hpp"

#include <algorithm>
#include <cmath>

#include "screen/core/draw.hpp"
#include "screen/core/pages.hpp"
#include "screen/core/sky.hpp"
#include "screen/core/squirrel.hpp"

namespace screen {

// ---- the schedule ----------------------------------------------------------------------------------------------

namespace {

int minutes(int hhmm) noexcept { return (hhmm / 100) * 60 + hhmm % 100; }

// m in [from, to) on a 24 h circle (to may be past midnight)
bool within(int m, int from, int to) noexcept {
    from = (from % 1440 + 1440) % 1440;
    to = (to % 1440 + 1440) % 1440;
    return from <= to ? (m >= from && m < to) : (m >= from || m < to);
}

}  // namespace

Activity activity_for(bool time_valid, int weekday, int hour, int minute, const Schedule& s) noexcept {
    if (!time_valid) return Activity::Sleep;
    const int m = hour * 60 + minute;
    const bool weekend = weekday == 0 || weekday == 6;
    const int wake = minutes(s.wake) + (weekend ? kWeekendLateMin : 0), lunch = minutes(s.lunch);
    const int end = minutes(s.end), bed = minutes(s.bed);
    if (!within(m, wake, bed)) return Activity::Sleep;
    if (within(m, lunch, lunch + kLunchMin)) return Activity::Lunch;
    if (weekend) return within(m, wake, wake + kWeekendCoffeeMin) ? Activity::Coffee : Activity::Read;
    if (within(m, wake, minutes(s.work))) return Activity::Coffee;
    if (within(m, minutes(s.work), end)) return Activity::Work;
    if (within(m, end, end + kCloseMin)) return Activity::CloseLaptop;
    return Activity::Read;
}

const char* activity_label(Activity a) noexcept {
    switch (a) {
        case Activity::Sleep: return "GOOD NIGHT";
        case Activity::Coffee: return "COFFEE TIME";
        case Activity::Work: return "BUSY";
        case Activity::Lunch: return "LUNCH";
        case Activity::CloseLaptop: return "WORK DONE";
        case Activity::Read: return "READING";
    }
    return "";
}

const char* activity_name(Activity a) noexcept {
    switch (a) {
        case Activity::Sleep: return "sleep";
        case Activity::Coffee: return "coffee";
        case Activity::Work: return "work";
        case Activity::Lunch: return "lunch";
        case Activity::CloseLaptop: return "close_laptop";
        case Activity::Read: return "read";
    }
    return "";
}

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

Room room_for(Activity a) noexcept {
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

// The wall clock shows the real time.
void wall_clock(const Canvas& cv, const Inputs& in, int dark) {
    constexpr float x = 208, y = 128, r = 14;
    draw::circle(cv, x, y, r + 2.5f, hex(draw::lerp_rgb(0x8A5A36, 0x2A2238, dark)));
    draw::circle(cv, x, y, r, hex(draw::lerp_rgb(0xFFF9EE, 0x5C5A78, dark)));
    const auto ink = hex(draw::lerp_rgb(0x3A2A20, 0x1A1828, dark));
    for (int i = 0; i < 12; i += 3) {
        const float a = static_cast<float>(i) * kPi / 6;
        draw::circle(cv, x + std::sin(a) * (r - 3.5f), y - std::cos(a) * (r - 3.5f), 1.2f, ink);
    }
    if (!in.time_valid) return;
    const float hm = static_cast<float>(in.hour % 12) + static_cast<float>(in.minute) / 60;
    const float ah = hm * kPi / 6, am = static_cast<float>(in.minute) * kPi / 30;
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
void mug(const Pen& p, float x, float y, bool steam, std::uint32_t t) {
    p.arc(x + 10, y, 5.5f, -kPi / 2, kPi / 2, 1.8f, 0xB8403C);  // handle
    p.box(x - 10, y - 10, x + 10, y + 11, 0xD9534F);
    p.ellipse(x, y + 11, 10, 2.5f, 0xD9534F);
    p.ellipse(x, y - 10, 10, 2.8f, 0xB8403C);
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

void blanket(const Canvas& cv, std::uint32_t t) {  // tucked in up to the chest
    const float breathe = wave(t, 3500) * 1.5f;  // rises and falls with the sleeping squirrel
    draw::ellipse(cv, 120, 262 + breathe, 176, 124, hex(0xC9CEEA));
    draw::ellipse(cv, 120, 262 + breathe, 174, 122, hex(0xEDEFFB));  // the folded-over sheet
    draw::ellipse(cv, 120, 272 + breathe, 174, 122, hex(0x3E4A8C));
    draw::ellipse(cv, 120, 274 + breathe, 172, 120, hex(0x5868B8));
    for (int i = 0; i < 4; ++i)  // quilt stitching
        draw::capsule(cv, 48 + i * 48.0f, 160 + breathe, 48 + i * 48.0f, 240, 0.8f, hex(0x3E4A8C), 160);
    draw::capsule(cv, 0, 196 + breathe, 240, 196 + breathe, 0.8f, hex(0x3E4A8C), 160);
}

}  // namespace

void render_day(const Canvas& cv, const Inputs& in) noexcept {
    const std::uint32_t t = in.t_ms;
    const Activity act = activity_for(in.time_valid, in.weekday, in.hour, in.minute, in.schedule);
    const Room room = room_for(act);
    const bool night_room = act == Activity::Sleep || act == Activity::Read;

    // the real sky in the window
    const weather::Sky sk = in.weather_ok ? in.weather.sky : weather::Sky::Clear;
    const bool day = in.weather_ok ? in.weather.is_day : !night_room;
    sky::draw(cv, sk, day, t, (kWinX0 + kWinX1) / 2, (kWinY0 + kWinY1) / 2 - 2, 0.42f, sky::colors(sk, day, t));
    wall(cv, room);
    window_frame(cv, room.dark);
    wall_clock(cv, in, room.dark);
    if (act == Activity::Read) lamp(cv, t);

    // the face follows the air, but calmer than on the air page (no hopping at the desk)
    Mood mood = in.sensor_ok ? mood_for(in.quality.level) : Mood::Sleepy;
    if (mood == Mood::VeryGood) mood = Mood::Good;
    const float cycle = ph(t, 6000);
    if (act == Activity::Sleep) mood = Mood::Sleepy;
    const bool stretch = act == Activity::CloseLaptop && cycle > 0.45f && mood != Mood::Bad;
    if (stretch) mood = Mood::VeryGood;

    Outfit o;
    o.paws_free = true;
    if (act == Activity::Sleep) {  // pillow behind, in bed
        draw::ellipse(cv, kCx, kCy - 30, 50, 28, hex(0xAEB4D8));
        draw::ellipse(cv, kCx, kCy - 32, 47, 25, hex(0xC9CEEA));
    }
    draw_squirrel(cv, kCx, kCy, kScale, mood, t, o);
    const Bob b = squirrel_bob(mood, t);
    const Pen p{cv, kCx + b.dx * kScale, kCy + b.dy * kScale, kScale};  // moves with the squirrel
    const Pen still{cv, kCx, kCy, kScale};                               // the desk does not

    switch (act) {
        case Activity::Coffee: {  // holds the mug, a sip every few seconds
            const float sip = pulse(cycle, 0.62f, 0.95f, 0.08f);
            const float my = 140 - sip * 20;
            mug(p, 125, my, true, t);
            arm(p, 106, 128, 113, my + 4);
            arm(p, 144, 128, 137, my + 4);
            desk(cv, room.dark);
            break;
        }
        case Activity::Work: {  // laptop open, screen light on the face, a sip from the cold coffee now and then
            const float glow = 40 + wave(t, 2300) * 10 + ((t / 180) % 7 == 0 ? 14 : 0);
            p.ellipse(125, 106, 30, 26, 0xA8DCFF, 0, static_cast<int>(glow));
            arm(p, 106, 128, 108, 168);
            arm(p, 144, 128, 142, 168);
            desk(cv, room.dark);
            mug(still, 45, 176, false, t);
            laptop_lid(still, 1.0f);
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
            const float chew = pulse(ph(t, 3200), 0.5f, 1.0f, 0.06f);
            if (chew > 0.05f) {
                const float puff = 2 + chew * (2 + wave(t, 300) * 1.0f);
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
        case Activity::Read: {  // an open book, a page turns every 8 s
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
            arm(p, 106, 128, 101, 141);
            arm(p, 144, 128, 149, 141);
            desk(cv, room.dark);
            lamp_stand(cv);
            break;
        }
        case Activity::Sleep: {
            nightcap(p);
            blanket(cv, t);
            for (int i = 0; i < 3; ++i) {  // Zzz
                const float k = ph(t + static_cast<std::uint32_t>(i) * 1000u, 3000);
                if (k > 0.85f) continue;
                draw_text(cv, static_cast<int>(184 + k * 12), static_cast<int>(112 - k * 30), 1 + (k > 0.3f) + (k > 0.6f),
                          hex(0xE8E8FF), "Z");
            }
            break;
        }
    }

    const char* lbl = activity_label(act);
    const bool big = text_width(lbl, 3) <= 150;
    draw::capsule(cv, 64, 205, 176, 205, 14, hex(0x1A2233), 60);
    draw::label(cv, big ? 194 : 198, big ? 3 : 2, hex(kWhite), lbl);
}

}  // namespace screen
