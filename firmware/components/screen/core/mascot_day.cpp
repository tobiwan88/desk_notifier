// screen: the day page of the new mascots. The mascot at home or out, doing what routine_for() picked, with the
// real weather in the window (or all around it outdoors), a few reactions to the room's climate, and the special
// days. Pure C++.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "screen/core/day.hpp"
#include "screen/core/draw.hpp"
#include "screen/core/mascot_view.hpp"
#include "screen/core/pages.hpp"
#include "screen/core/props.hpp"
#include "screen/core/routine.hpp"
#include "screen/core/sky.hpp"

namespace screen {

namespace {

using draw::hex;
using draw::Pt;

constexpr float kPi = 3.14159265f;
float ph(std::uint32_t t, std::uint32_t period) noexcept { return static_cast<float>(t % period) / static_cast<float>(period); }
float wave(std::uint32_t t, std::uint32_t period) noexcept { return std::sin(2 * kPi * ph(t, period)); }

constexpr props::Win kWin{160, 34, 208, 88};  // the window (screen pixels)
constexpr float kTable = 170;                  // screen y of the table top

// What the room's climate or the weather adds to an at-home scene. The first match wins.
enum class Sit : std::uint8_t { None, Party, Storm, AirOut, Hot, Cold, Cocoa };

void box(const Canvas& cv, float x0, float y0, float x1, float y1, std::uint32_t c, int a = 256) {
    const Pt q[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    draw::polygon(cv, q, 4, hex(c), a);
}
void line(const Canvas& cv, float x0, float y0, float x1, float y1, float r, std::uint32_t c, int a = 256) {
    draw::capsule(cv, x0, y0, x1, y1, r, hex(c), a);
}
void dome(const Canvas& cv, float x, float y, float rx, float ry, std::uint32_t c) {
    Pt q[13];
    for (int i = 0; i < 13; ++i) {
        const float a = kPi + kPi * static_cast<float>(i) / 12.0f;
        q[i] = {x + std::cos(a) * rx, y + std::sin(a) * ry};
    }
    draw::polygon(cv, q, 13, hex(c));
}

bool is_day(const Inputs& in) { return in.weather_ok ? in.weather.is_day : (in.hour >= 7 && in.hour < 19); }
weather::Sky sky_now(const Inputs& in) { return weather::now_sky(in.weather); }

// The whole screen as sky: the real weather, or a plain day/night gradient without a forecast.
void sky_back(const Canvas& cv, const Inputs& in, std::uint32_t t, float sun_x, float sun_y, float scale) {
    if (in.weather_ok) {
        const weather::Sky s = sky_now(in);
        const float moon = in.time_valid ? cal::moon_phase(in.year, in.month, in.day, in.hour) : -1.0f;
        sky::draw(cv, s, in.weather.is_day, t, sun_x, sun_y, scale, sky::colors(s, in.weather.is_day, t), false, moon);
    } else if (is_day(in)) {
        draw::vgradient(cv, 0xA6D3EE, 0xEEF7EA);
    } else {
        draw::vgradient(cv, 0x162447, 0x3B3D74);
    }
}

// The wall with a hole for the window (the sky was drawn before).
void wall(const Canvas& cv, std::uint32_t top, std::uint32_t bottom, bool window) {
    for (int y = 0; y < gfx::kScreen; ++y) {
        const gfx::Chord ch = gfx::kChord[static_cast<std::size_t>(y)];
        const auto c = hex(draw::lerp_rgb(top, bottom, y * 256 / (gfx::kScreen - 1)));
        gfx::Rgb565be* row = cv.px.data() + static_cast<std::size_t>(y) * gfx::kScreen;
        if (window && y >= static_cast<int>(kWin.y0) && y < static_cast<int>(kWin.y1)) {
            const int wa = std::clamp(static_cast<int>(kWin.x0), static_cast<int>(ch.x0), static_cast<int>(ch.x1));
            const int wb = std::clamp(static_cast<int>(kWin.x1), static_cast<int>(ch.x0), static_cast<int>(ch.x1));
            fill_px(row + ch.x0, wa - ch.x0, c);
            fill_px(row + wb, ch.x1 - wb, c);
        } else {
            fill_px(row + ch.x0, ch.x1 - ch.x0, c);
        }
    }
}

void window_frame(const Canvas& cv, int dark) {
    const std::uint32_t frame = draw::lerp_rgb(0xF7F3EC, 0x2A2C48, dark), sill = draw::lerp_rgb(0xC9BFB2, 0x1A1C30, dark);
    const float mx = (kWin.x0 + kWin.x1) / 2, my = (kWin.y0 + kWin.y1) / 2;
    line(cv, kWin.x0, kWin.y0, kWin.x1, kWin.y0, 2.5f, frame);
    line(cv, kWin.x0, kWin.y1, kWin.x1, kWin.y1, 2.5f, frame);
    line(cv, kWin.x0, kWin.y0, kWin.x0, kWin.y1, 2.5f, frame);
    line(cv, kWin.x1, kWin.y0, kWin.x1, kWin.y1, 2.5f, frame);
    line(cv, mx, kWin.y0, mx, kWin.y1, 1.5f, frame);
    line(cv, kWin.x0, my, kWin.x1, my, 1.5f, frame);
    line(cv, kWin.x0 - 5, kWin.y1 + 4, kWin.x1 + 5, kWin.y1 + 4, 2.5f, sill);
}

// Wall colours (top, bottom) and how dark the room is (0..256).
struct Room { std::uint32_t top, bottom; int dark; };
Room room(const Inputs& in, Act a) {
    if (a == Act::Sleep || a == Act::OutWithFriends) return {0x1C2242, 0x2B3060, 200};
    const int h = in.hour;
    if (h >= 20 || h < 6) return {0x4A4266, 0x6A5878, 130};
    if (h >= 18) return {0xF0C8A8, 0xD8A07E, 50};
    if (h < 8) return {0xFFE9CF, 0xF7D2AA, 20};
    return {0xF6EEDC, 0xE8DCC0, 0};
}

void indoor(const Canvas& cv, const Inputs& in, Act a, std::uint32_t t, int& dark) {
    sky_back(cv, in, t, 192, 58, 0.32f);
    if (in.weather_ok) sky::precipitation(cv, sky_now(in), t);
    const Room r = room(in, a);
    dark = r.dark;
    wall(cv, r.top, r.bottom, true);
    window_frame(cv, r.dark);
    draw::vgradient(cv, draw::lerp_rgb(0xB89470, 0x2A2236, r.dark), draw::lerp_rgb(0x9A7650, 0x1E1828, r.dark), 196);
}

void table(const Canvas& cv, int dark) {
    const std::uint32_t top = draw::lerp_rgb(0xD39A62, 0x3A2C3A, dark), front = draw::lerp_rgb(0xA8714A, 0x2C2230, dark);
    draw::vgradient(cv, front, draw::lerp_rgb(front, 0x000000, 70), static_cast<int>(kTable) + 6);
    draw::vgradient(cv, top, top, static_cast<int>(kTable), static_cast<int>(kTable) + 6);
}

// Outside: the real sky, a hill, snow on it on snowy days.
void outdoor(const Canvas& cv, const Inputs& in, std::uint32_t t) {
    sky_back(cv, in, t, 196, 50, 0.5f);
    const bool snowy = in.weather_ok && sky_now(in) == weather::Sky::Snow;
    std::uint32_t hill = snowy ? 0xE4ECF4 : 0x8CC07A, back = snowy ? 0xC9D8E8 : 0x6FAF68;
    if (in.weather_ok) {
        const int gl = sky::gloom(sky_now(in));
        hill = draw::lerp_rgb(hill, 0x4A5A55, gl); back = draw::lerp_rgb(back, 0x4A5A55, gl);
    }
    if (!is_day(in)) { hill = draw::lerp_rgb(hill, 0x16302A, 130); back = draw::lerp_rgb(back, 0x16302A, 150); }
    draw::circle(cv, 30, 300, 140, hex(back));
    draw::circle(cv, 150, 330, 160, hex(hill));
}

void sofa(const Canvas& cv, std::uint32_t c, int dark) {
    const std::uint32_t back = draw::lerp_rgb(c, 0x000000, 40 + dark / 4), seat = draw::lerp_rgb(c, 0x000000, dark / 4);
    line(cv, 30, 120, 210, 120, 22, back);
    box(cv, 8, 150, 232, 200, seat);
    line(cv, 20, 140, 20, 196, 15, back);
    line(cv, 220, 140, 220, 196, 15, back);
}

void armchair(const Canvas& cv, std::uint32_t c, int dark) {
    const std::uint32_t back = draw::lerp_rgb(c, 0x000000, 30 + dark / 4), seat = draw::lerp_rgb(c, 0x000000, dark / 4);
    line(cv, 62, 92, 178, 92, 28, back);
    box(cv, 34, 120, 206, 200, back);
    line(cv, 44, 132, 44, 198, 16, seat);
    line(cv, 196, 132, 196, 198, 16, seat);
}

// The bed's headboard behind the pillow: a wooden panel with a rounded top.
void headboard(const Canvas& cv, std::uint32_t c) {
    box(cv, 26, 100, 214, 156, c);
    line(cv, 34, 100, 206, 100, 8, c);
    line(cv, 26, 100, 26, 156, 5, draw::lerp_rgb(c, 0x000000, 40));
    line(cv, 214, 100, 214, 156, 5, draw::lerp_rgb(c, 0x000000, 40));
}

void zzz(const Canvas& cv, float x, float y, std::uint32_t t, std::uint32_t c = 0xE8E8FF) {
    for (int i = 0; i < 3; ++i) {
        const float k = ph(t + static_cast<std::uint32_t>(i) * 1000u, 3000);
        if (k > 0.85f) continue;
        draw_text(cv, static_cast<int>(x + k * 12), static_cast<int>(y - k * 34), 1 + (k > 0.3f) + (k > 0.6f), hex(c), "Z");
    }
}

void mug(const Canvas& cv, float x, float y, std::uint32_t c, std::uint32_t t) {
    draw::arc(cv, x + 8, y, 5, -kPi / 2, kPi / 2, 1.8f, hex(draw::lerp_rgb(c, 0, 50)));
    box(cv, x - 8, y - 9, x + 8, y + 10, c);
    for (int i = 0; i < 2; ++i) {
        const float k = ph(t + static_cast<std::uint32_t>(i) * 900u, 1800), sx = x - 3 + static_cast<float>(i) * 6;
        line(cv, sx + std::sin(k * 6) * 2, y - 13 - k * 12, sx + std::sin(k * 6 + 1) * 2, y - 17 - k * 12, 1.2f, 0xFFFFFF, static_cast<int>(200 * (1 - k)));
    }
}

void plate(const Canvas& cv, float x, float y) {
    draw::ellipse(cv, x, y, 26, 6, hex(0xFFFFFF));
    draw::ellipse(cv, x, y - 0.5f, 18, 3.6f, hex(0xE9E2D2));
}

void newspaper(const Canvas& cv, float x0, float y0) {
    box(cv, x0, y0, x0 + 56, y0 + 46, 0xF7F4EC);
    box(cv, x0 + 5, y0 + 5, x0 + 51, y0 + 11, 0x3A3A44);
    for (int i = 0; i < 4; ++i) line(cv, x0 + 6, y0 + 18 + static_cast<float>(i) * 6, x0 + 28, y0 + 18 + static_cast<float>(i) * 6, 0.9f, 0xB8B0A2);
    box(cv, x0 + 33, y0 + 16, x0 + 51, y0 + 38, 0xC9D6E6);
}

void book(const Canvas& cv, float x, float y, std::uint32_t c) {
    const Pt l[4] = {{x, y - 8}, {x - 24, y - 13}, {x - 24, y + 11}, {x, y + 16}};
    const Pt r[4] = {{x, y - 8}, {x + 24, y - 13}, {x + 24, y + 11}, {x, y + 16}};
    draw::polygon(cv, l, 4, hex(0xF7F4EC));
    draw::polygon(cv, r, 4, hex(0xEFE9DC));
    line(cv, x, y - 8, x, y + 16, 1.2f, c);
}

void cake_slice(const Canvas& cv, float x, float y) {
    const Pt s[3] = {{x - 16, y}, {x + 16, y}, {x + 16, y - 20}};
    draw::polygon(cv, s, 3, hex(0xF7E3B4));
    box(cv, x - 4, y - 9, x + 16, y - 6, 0xE86A8A);
    draw::circle(cv, x + 13, y - 22, 3.5f, hex(0xD9433A));
}

// The headset (bunny gaming, stag backstage): a band over the head, ear cups, optional mic.
void headset(const Canvas& cv, float hx, float hy, float r, bool mic, std::uint32_t cup) {
    draw::arc(cv, hx, hy, r, kPi, 2 * kPi, 2.2f, hex(0x2A2A30), 256, 8);
    draw::ellipse(cv, hx - r, hy, 4, 8, hex(cup));
    draw::ellipse(cv, hx + r, hy, 4, 8, hex(cup));
    if (mic) {
        line(cv, hx - r, hy + 6, hx - r * 0.5f, hy + r * 0.7f, 1.4f, 0x2A2A30);
        draw::circle(cv, hx - r * 0.45f, hy + r * 0.72f, 2.6f, hex(0x2A2A30));
    }
}

void dog_bone(const Canvas& cv, float x, float y) {
    line(cv, x - 6, y, x + 6, y, 2.2f, 0xF3E3C0);
    for (const float dx : {-7.0f, 7.0f}) for (const float dy : {-2.5f, 2.5f}) draw::circle(cv, x + dx, y + dy, 2.6f, hex(0xF3E3C0));
}

void flowers(const Canvas& cv, float x0, float y, int n, std::uint32_t t) {
    static constexpr std::uint32_t kPetal[] = {0xF27B9B, 0xF2C14E, 0xB98AE0, 0xFFFFFF};
    for (int i = 0; i < n; ++i) {
        const float x = x0 + static_cast<float>(i) * 14 + wave(t + static_cast<std::uint32_t>(i) * 400u, 2600) * 1.2f;
        line(cv, x, y, x, y - 16, 1.2f, 0x4F8F4A);
        draw::circle(cv, x, y - 18, 5, hex(kPetal[i % 4]));
        draw::circle(cv, x, y - 18, 1.8f, hex(0xFFE27A));
    }
}

void birdhouse(const Canvas& cv, float x, float y, std::uint32_t t) {
    box(cv, x - 3, y, x + 3, y + 90, 0x8B5E3C);
    box(cv, x - 18, y - 30, x + 18, y, 0xC98F5E);
    const Pt roof[3] = {{x - 23, y - 29}, {x + 23, y - 29}, {x, y - 50}};
    draw::polygon(cv, roof, 3, hex(0xB3463A));
    draw::circle(cv, x, y - 15, 5.5f, hex(0x4A3020));
    box(cv, x - 26, y, x + 26, y + 4, 0x8B5E3C);
    for (int i = 0; i < 2; ++i) {  // two birds pecking
        const float bx = x - 14 + static_cast<float>(i) * 28, hop = std::max(0.0f, wave(t + static_cast<std::uint32_t>(i) * 700u, 1400)) * 3;
        draw::circle(cv, bx, y - 6 - hop, 6, hex(i ? 0x5A7AB8 : 0x8A6A4E));
        draw::circle(cv, bx, y - 3 - hop, 3.6f, hex(i ? 0xF2D04E : 0xE8603C));
        const float d = i ? 1.0f : -1.0f;
        const Pt beak[3] = {{bx + d * 5, y - 8 - hop}, {bx + d * 10, y - 6 - hop}, {bx + d * 5, y - 5 - hop}};
        draw::polygon(cv, beak, 3, hex(0xE8A23A));
        draw::circle(cv, bx + d * 2, y - 8 - hop, 1.1f, hex(0x000000));
    }
}

void bike(const Canvas& cv, std::uint32_t t) {
    for (const float wx : {64.0f, 176.0f}) {
        draw::arc(cv, wx, 192, 24, 0, 2 * kPi, 2.5f, hex(0x2E2E36), 256, 16);
        const float a = static_cast<float>(t % 1000u) / 1000.0f * 2 * kPi;
        line(cv, wx - std::cos(a) * 22, 192 - std::sin(a) * 22, wx + std::cos(a) * 22, 192 + std::sin(a) * 22, 0.7f, 0x9A9AA6);
    }
    constexpr std::uint32_t R = 0xD9584A;
    line(cv, 64, 192, 104, 192, 2.4f, R); line(cv, 104, 192, 140, 162, 2.4f, R); line(cv, 140, 162, 176, 192, 2.4f, R);
    line(cv, 104, 192, 94, 156, 2.4f, R); line(cv, 94, 156, 140, 162, 2.4f, R);
    line(cv, 140, 162, 148, 146, 2.4f, R); line(cv, 148, 146, 160, 146, 2.4f, 0x2E2E36);
    line(cv, 84, 152, 104, 152, 3, 0x2E2E36);
}

void stage(const Canvas& cv, std::uint32_t t) {
    draw::vgradient(cv, 0x2A1830, 0x3A2040);
    const Pt beam[4] = {{100, 20}, {140, 20}, {196, 200}, {44, 200}};
    draw::polygon(cv, beam, 4, hex(0xFFF3C4), 50);
    box(cv, 0, 196, 240, 240, 0x6B4A30);
    box(cv, 0, 196, 240, 200, 0x8A6444);
    const Pt cl[4] = {{0, 0}, {62, 0}, {50, 100}, {0, 200}}, cr[4] = {{240, 0}, {178, 0}, {190, 100}, {240, 200}};
    draw::polygon(cv, cl, 4, hex(0xB3263A));
    draw::polygon(cv, cr, 4, hex(0xB3263A));
    box(cv, 0, 0, 240, 24, 0x8E1C2E);
    for (int i = 0; i < 10; ++i) draw::circle(cv, 12 + static_cast<float>(i) * 24, 26, 5, hex(0xE8B84A));
    // the cue light: red while waiting, green for go
    const bool go = ph(t, 6000) > 0.7f;
    box(cv, 180, 98, 196, 124, 0x2E2E36);
    draw::circle(cv, 188, 106, 10, hex(go ? 0x5FC46B : 0xFF4A4A), 70);
    draw::circle(cv, 188, 106, 4.5f, hex(go ? 0x3A4A3A : 0xFF4A4A));
    draw::circle(cv, 188, 117, 3, hex(go ? 0x5FC46B : 0x3A4A3A));
}

Sit situation(const Inputs& in, Act a) {
    if (!in.time_valid) return Sit::None;
    const int m = in.hour * 60 + in.minute;
    if ((in.month == 12 && in.day == 31 && m >= 22 * 60) || (in.month == 1 && in.day == 1 && m < 30)) return Sit::Party;
    const bool evening = in.hour >= 18 || in.hour < 6;
    const auto& w = in.weather;
    const bool home = !act_outdoors(a) && a != Act::Backstage && a != Act::OutWithFriends;
    if (in.weather_ok && w.sky == weather::Sky::Thunder && evening && home) return Sit::Storm;
    if (!home || a == Act::Sleep || a == Act::Alarm) return Sit::None;
    if (!in.sensor_ok) return Sit::None;
    const bool stale = in.quality.level == sensor::Level::Stale || in.quality.level == sensor::Level::Poor;
    const bool nice_out = in.weather_ok && w.temp_c >= 10 && w.temp_c <= 24 && !weather::wet(w.sky) && w.wind_kmh < 30;
    if (stale && nice_out) return Sit::AirOut;
    if (in.reading.temp_c >= kHotRoomC) return Sit::Hot;
    if (in.reading.temp_c < kColdRoomC) return Sit::Cold;
    if (in.weather_ok && weather::wet(w.sky) && in.hour >= 17) return Sit::Cocoa;
    return Sit::None;
}

const char* sit_label(Sit s, const Inputs& in) {
    switch (s) {
        case Sit::None: return nullptr;
        case Sit::Party: return in.month == 1 ? "HAPPY NEW YEAR" : "COUNTDOWN";
        case Sit::Storm: return "THUNDER!";
        case Sit::AirOut: return "AIR OUT";
        case Sit::Hot: return "DRINK WATER";
        case Sit::Cold: return "CHILLY";
        case Sit::Cocoa: return "COSY EVENING";
    }
    return nullptr;
}

const char* occasion_text(Occasion o) {
    switch (o) {
        case Occasion::Birthday: return "HAPPY BIRTHDAY";
        case Occasion::Christmas: return "MERRY XMAS";
        case Occasion::Easter: return "HAPPY EASTER";
        case Occasion::NewYear: return "HAPPY NEW YEAR";
        default: return nullptr;
    }
}

void caption(const Canvas& cv, const char* const* lines, int n, std::uint32_t t) {
    if (n <= 0) return;
    const char* lbl = lines[(t / 4000u) % static_cast<unsigned>(n)];
    const bool big = text_width(lbl, 3) <= 150;
    const float half = static_cast<float>(text_width(lbl, big ? 3 : 2)) / 2 - 6;
    draw::capsule(cv, 120 - half, 197, 120 + half, 197, 14, hex(0x1A2233), 70);  // where the squirrel's caption sits
    draw::label(cv, big ? 187 : 190, big ? 3 : 2, hex(0xFFFFFF), lbl);
}

}  // namespace

void render_mascot_day(const Canvas& cv, const Inputs& in) noexcept {
    const std::uint32_t t = in.t_ms;
    const bool day_off = in.time_valid && (in.weekday == 0 || in.weekday == 6 || cal::public_holiday(in.year, in.month, in.day));
    const bool wet = in.weather_ok && weather::wet(sky_now(in));
    const Act act = routine_for(in.mascot, in.profile, in.time_valid, in.weekday, in.hour, in.minute, day_off, !wet);
    const Sit sit = situation(in, act);
    const Occasion occ = in.time_valid ? occasion_for(in.year, in.month, in.day, in.schedule) : Occasion::None;
    const Mascot who = in.mascot;

    MascotLook l;
    l.who = who;
    l.coat = in.time_valid ? cat_coat(cal::day_number(in.year, in.month, in.day)) : Coat::Calico;
    if (act_outdoors(act)) l.wear = wear_for(in.weather_ok, in.weather);
    const char* say = nullptr;  // a label of the scene instead of the routine's

    // the mascot's place on screen: (cx, cy) = design point (120, 120), s = scale
    float cx = 110, cy = 122, s = 0.8f;
    int dark = 0;
    bool at_table = false, decor = true;

    switch (act) {
        case Act::Sleep:
        case Act::Alarm: {
            indoor(cv, in, Act::Sleep, t, dark);
            if (act == Act::Alarm) dark = 110;
            headboard(cv, draw::lerp_rgb(0x8A6A4E, 0x000000, dark / 3));
            draw::ellipse(cv, 112, 124, 46, 15, hex(draw::lerp_rgb(0xE8ECF5, 0x2B3060, dark / 2)));
            cx = 112; cy = 136; s = 0.76f;
            l.face = act == Act::Alarm ? Face::Half : sit == Sit::Storm ? Face::Worried : Face::Asleep;
            draw_mascot(cv, cx, cy, s, l, t);
            static constexpr std::uint32_t kBlanket[] = {0x5C7BC0, 0x5C7BC0, 0x5E8C6A, 0xC27BA0, 0x7E8FC0};
            const std::uint32_t bc = draw::lerp_rgb(kBlanket[static_cast<int>(who)], 0x000000, dark / 3);
            const float lift = wave(t, 3600) * 1.5f + (sit == Sit::Storm ? -12.0f : 0.0f);
            box(cv, 6, 150 - lift, 234, 240, bc);
            line(cv, 14, 150 - lift, 226, 150 - lift, 4, draw::lerp_rgb(bc, 0xFFFFFF, 50));
            if (who == Mascot::Bunny && act == Act::Sleep) {  // the controller still in the paw
                line(cv, 146, 150 - lift, 164, 150 - lift, 5, 0x3A3A44);
                draw::ellipse(cv, 148, 151 - lift, 6, 5, hex(0xFBFAF7));
            }
            if (who == Mascot::Cat) draw_dog(cv, 64, 168 - lift, 0.95f, 1, DogPose::Sleep, t);
            if (act == Act::Alarm) {  // the alarm clock rings on the nightstand
                const float sh = wave(t, 120) * 2;
                box(cv, 178, 160, 222, 240, 0x6A5040);
                draw::circle(cv, 198 + sh, 146, 13, hex(0xD9433A));
                draw::circle(cv, 198 + sh, 146, 10, hex(0xFFF9EE));
                line(cv, 198 + sh, 146, 198 + sh, 139, 1.2f, 0x3A2A20);
                line(cv, 198 + sh, 146, 203 + sh, 146, 1.2f, 0x3A2A20);
                draw::circle(cv, 189 + sh, 133, 4.5f, hex(0xD9433A));
                draw::circle(cv, 207 + sh, 133, 4.5f, hex(0xD9433A));
                if (ph(t, 600) < 0.5f) { line(cv, 176, 128, 170, 122, 1.2f, 0xFFE27A); line(cv, 220, 128, 226, 122, 1.2f, 0xFFE27A); }
            } else if (sit != Sit::Storm) {
                zzz(cv, 150, 96, t);
            }
            decor = false;
            break;
        }
        case Act::OutWithFriends: {
            indoor(cv, in, act, t, dark);
            line(cv, 52, 70, 92, 70, 2.5f, 0x8A6A4E);  // the coat hook, the jacket is gone
            for (const float hx : {60.0f, 74.0f, 88.0f}) { line(cv, hx, 70, hx, 80, 1.6f, 0xB8B0A2); draw::circle(cv, hx + 2, 81, 2, hex(0xB8B0A2)); }
            line(cv, 60, 70, 60, 104, 7, 0x3E7CB1);  // only the cap hangs there
            draw::circle(cv, 60, 70, 7, hex(0x3E7CB1));
            line(cv, 80, 196, 128, 196, 6, 0x3A3A44);  // the console, switched off
            draw::circle(cv, 122, 194, 1.5f, hex(0xD9433A));
            decor = false;
            break;
        }
        case Act::Gaming:
        case Act::Tv: {
            indoor(cv, in, Act::Gaming, t, dark);
            sofa(cv, who == Mascot::Stag ? 0x8E5A3A : 0x5B6BA8, dark);
            cx = 120; cy = 124; s = 0.74f;
            l.face = (act == Act::Gaming && ph(t, 7000) > 0.8f) ? Face::Happy : Face::Open;
            draw_mascot(cv, cx, cy, s, l, t);
            // the screen's flicker on the face
            const int glow = 40 + static_cast<int>(wave(t, act == Act::Tv ? 700 : 1300) * 14);
            draw::ellipse(cv, cx, cy + (104 - 120) * s, 30 * s, 26 * s, hex(act == Act::Tv ? 0xB8D8FF : 0x7FD3FF), 0, glow);
            if (act == Act::Gaming) {
                headset(cv, 120, cy + (104 - 120) * s, 34 * s, false, 0xE0544A);
                line(cv, 106, 152, 134, 152, 7, 0x3A3A44);
                draw::circle(cv, 112, 152, 2.5f, hex(0x9FA6B8));
                draw::circle(cv, 128, 150, 2, hex(0x5FC46B));
                draw::circle(cv, 131, 154, 2, hex(0xE0544A));
                if (ph(t, 7000) > 0.8f) draw_text(cv, 162, 62, 3, hex(0x7FD3FF), "GG!");
            } else {
                line(cv, 132, 154, 146, 150, 3, 0x3A3A44);
                draw::circle(cv, 143, 150, 1.4f, hex(0xD9433A));
            }
            if (who == Mascot::Bunny && act == Act::Gaming) {
                box(cv, 204, 128, 214, 146, 0x6CCF8A);
                box(cv, 204, 131, 214, 135, 0x3E8F5A);
            }
            decor = false;
            break;
        }
        case Act::Gardening:
        case Act::Garden:
        case Act::Birds:
        case Act::Church: {
            outdoor(cv, in, t);
            cx = 96; cy = 114; s = 0.72f;
            l.face = Face::Happy;
            const bool snowy = in.weather_ok && sky_now(in) == weather::Sky::Snow;
            if (act == Act::Church) {  // the village church, bells ringing
                box(cv, 150, 96, 206, 182, 0xF3EDE4);
                const Pt roof[3] = {{144, 98}, {212, 98}, {178, 70}};
                draw::polygon(cv, roof, 3, hex(0x8E3B3A));
                box(cv, 194, 40, 214, 120, 0xEDE4D6);
                const Pt spire[3] = {{190, 42}, {218, 42}, {204, 12}};
                draw::polygon(cv, spire, 3, hex(0x6A7A8A));
                draw::circle(cv, 204, 58, 5, hex(0xE8B84A));
                box(cv, 170, 150, 186, 182, 0x8B5E3C);
                if (ph(t, 1600) < 0.5f) { line(cv, 222, 50, 230, 44, 1.2f, 0x3A3A44); line(cv, 186, 50, 178, 44, 1.2f, 0x3A3A44); }
            }
            if (act == Act::Birds) birdhouse(cv, 184, 112, t);
            if (act == Act::Garden || act == Act::Gardening) flowers(cv, who == Mascot::Penguin ? 14 : 168, 196, 4, t);
            draw_mascot(cv, cx, cy, s, l, t);
            if (who == Mascot::Bunny && act == Act::Gardening) {
                if (wet && !snowy) {  // rain jacket
                    draw::ellipse(cv, cx, cy + (158 - 120) * s, 43 * s, 34 * s, hex(0xF2C14E), 0, 235);
                    line(cv, cx, cy + 16 * s, cx, cy + 60 * s, 0.8f, 0xC99A2E);
                }
                if (snowy) {  // winter duty: clearing snow
                    say = "WINTER DUTY";
                    line(cv, 112, 140, 168, 194, 2.4f, 0x8B5E3C);
                    const Pt blade[4] = {{160, 192}, {184, 186}, {190, 202}, {166, 208}};
                    draw::polygon(cv, blade, 4, hex(0x6A7A8A));
                } else if (in.month >= 9 || in.month <= 2) {  // rake and leaves
                    line(cv, 112, 140, 182, 200, 2.2f, 0x8B5E3C);
                    line(cv, 170, 208, 194, 190, 2.2f, 0x6B6B70);
                    for (int i = 0; i < 4; ++i) line(cv, 173 + static_cast<float>(i) * 6, 206 - static_cast<float>(i) * 4.5f, 177 + static_cast<float>(i) * 6, 212 - static_cast<float>(i) * 4.5f, 0.9f, 0x6B6B70);
                    static constexpr std::uint32_t kLeaf[] = {0xE8833A, 0xD9584A, 0xF2C14E};
                    for (int i = 0; i < 5; ++i)
                        draw::ellipse(cv, 196 + static_cast<float>(i % 3) * 8, 212 - static_cast<float>(i / 3) * 5, 6, 3, hex(kLeaf[i % 3]), static_cast<float>(i) * 0.7f);
                } else {  // planting with a spade
                    line(cv, 124, 136, 160, 200, 2.2f, 0x8B5E3C);
                    const Pt sp[4] = {{154, 196}, {166, 192}, {170, 208}, {158, 212}};
                    draw::polygon(cv, sp, 4, hex(0x9AA0A8));
                    draw::ellipse(cv, 196, 206, 14, 5, hex(0x6B4A30));
                    for (int i = 0; i < 3; ++i) draw::ellipse(cv, 188 + static_cast<float>(i) * 8, 198, 3, 5, hex(0x5FA84E), 0.3f);
                }
            }
            if (who == Mascot::Penguin && act == Act::Gardening) props::watering_can(cv, 50, 148, 1.3f, 0.8f, t);
            if (who == Mascot::Stag && act == Act::Garden) {  // a wheelbarrow of soil
                const Pt tray[4] = {{150, 172}, {200, 172}, {192, 192}, {158, 192}};
                draw::polygon(cv, tray, 4, hex(0x4F8F4A));
                dome(cv, 175, 174, 22, 8, 0x6B4A30);
                draw::circle(cv, 196, 200, 7, hex(0x2E2E36));
                line(cv, 150, 176, 136, 168, 2, 0x8B5E3C);
            }
            if (act == Act::Birds) {  // seeds flying
                for (int i = 0; i < 5; ++i) {
                    const float k = ph(t + static_cast<std::uint32_t>(i) * 240u, 1200);
                    draw::circle(cv, 132 + k * 30 + static_cast<float>(i) * 3, 120 - std::sin(k * kPi) * 14 + k * 30, 1.6f, hex(0xC9A26A));
                }
            }
            decor = false;
            break;
        }
        case Act::LunchBreak: {
            outdoor(cv, in, t);
            const Pt bucket[4] = {{90, 210}, {150, 210}, {144, 168}, {96, 168}};
            draw::polygon(cv, bucket, 4, hex(0x5E8C9A));
            line(cv, 94, 168, 146, 168, 3, 0x4A7482);
            cx = 120; cy = 114; s = 0.7f;
            l.face = Face::Happy;
            draw_mascot(cv, cx, cy, s, l, t);
            box(cv, 106, 140, 134, 146, 0xE8C27A);
            box(cv, 105, 146, 135, 149, 0x7FBF5A);
            box(cv, 106, 149, 134, 155, 0xE8C27A);
            const Pt carrot[3] = {{170, 204}, {200, 196}, {200, 204}};
            draw::polygon(cv, carrot, 3, hex(0xF28C38));
            line(cv, 200, 198, 210, 190, 1.4f, 0x5FA84E); line(cv, 200, 201, 212, 199, 1.4f, 0x5FA84E);
            box(cv, 36, 190, 70, 210, 0x3E7CB1);
            decor = false;
            break;
        }
        case Act::BikeRide: {
            outdoor(cv, in, t);
            for (int i = 0; i < 3; ++i) line(cv, 6, 150 + static_cast<float>(i) * 14, 28 - static_cast<float>(i) * 4, 150 + static_cast<float>(i) * 14, 1.4f, 0xFFFFFF, 180);
            bike(cv, t);
            cx = 110; cy = 102; s = 0.6f;
            l.face = Face::Happy;
            draw_mascot(cv, cx, cy, s, l, t);
            const float hx = cx, hy = cy + (104 - 120) * s - 6;  // the helmet over the head, antlers poking through
            dome(cv, hx, hy, 21, 15, 0x3E7CB1);
            line(cv, hx - 22, hy, hx + 22, hy, 2.4f, 0x2E5F8A);
            line(cv, 128, 120, 150, 146, 3.4f, 0xB5734A);
            draw::ellipse(cv, 152, 146, 4, 3, hex(0x9A5F3B));
            decor = false;
            break;
        }
        case Act::DogWalk: {
            outdoor(cv, in, t);
            draw::ellipse(cv, 130, 214, 140, 14, hex(0xE2D2B0));
            cx = 92; cy = 120; s = 0.66f;
            l.face = Face::Happy;
            draw_mascot(cv, cx, cy, s, l, t);
            const float px = cx + (170 - 120) * s, py = cy + (96 - 120) * s;
            draw_dog(cv, 180, 196, 1.15f, 1, DogPose::Stand, t);
            draw::capsule(cv, px, py, 168, 140, 1.1f, hex(0xD8463A));
            draw::capsule(cv, 168, 140, 189, 192, 1.1f, hex(0xD8463A));
            if (wet) {  // an umbrella
                dome(cv, cx, cy - 76 * s, 46, 24, 0x3E7CB1);
                line(cv, cx, cy - 76 * s, cx + 4, cy + 20 * s, 1.6f, 0x3A3A44);
            }
            decor = false;
            break;
        }
        case Act::Backstage: {
            stage(cv, t);
            cx = 116; cy = 128; s = 0.74f;
            l.face = Face::Open;
            draw_mascot(cv, cx, cy, s, l, t);
            headset(cv, cx, cy + (104 - 120) * s, 33 * s, true, 0x2A2A30);
            box(cv, 100, 146, 126, 178, 0x8B5E3C);  // the clipboard with the cue list
            box(cv, 104, 151, 122, 175, 0xF7F4EC);
            box(cv, 107, 147, 119, 152, 0x9FA6B8);
            for (int i = 0; i < 3; ++i) line(cv, 107, 158 + static_cast<float>(i) * 5.5f, 119, 158 + static_cast<float>(i) * 5.5f, 0.8f, i == 0 ? 0x5FA84E : 0x9A9488);
            say = nullptr;
            decor = false;
            break;
        }
        case Act::Rest:
        case Act::Nap:
        case Act::Knitting:
        case Act::Cosy: {
            indoor(cv, in, act, t, dark);
            if (act == Act::Cosy) {  // the floor lamp
                draw::circle(cv, 212, 70, 40, hex(0xFFD98A), 50);
                line(cv, 212, 70, 212, 200, 1.8f, 0x8A7A6A);
                const Pt shade[4] = {{198, 70}, {226, 70}, {220, 50}, {204, 50}};
                draw::polygon(cv, shade, 4, hex(0xF2C97A));
            }
            armchair(cv, who == Mascot::Penguin ? 0x7E8FC0 : 0x8E5A7A, dark);
            cx = 120; cy = 122; s = 0.72f;
            l.face = (act == Act::Nap || act == Act::Rest) ? Face::Asleep : Face::Open;
            l.glasses = who == Mascot::Penguin && (act == Act::Knitting || act == Act::Nap);
            l.glasses_up = act == Act::Nap;
            draw_mascot(cv, cx, cy, s, l, t);
            if (act == Act::Nap || act == Act::Rest) {
                box(cv, 70, 150, 170, 192, 0xE8B4C4);
                for (const float y : {162.0f, 176.0f}) line(cv, 70, y, 170, y, 1.4f, 0xD394A8);
                zzz(cv, 150, 96, t, 0x4A4A70);
            }
            if (act == Act::Knitting) {
                box(cv, 104, 146, 136, 166, 0xD9584A);
                line(cv, 104, 154, 136, 154, 1, 0xF3EDE4);
                const float k = wave(t, 800) * 2;
                line(cv, 96, 132 + k, 132, 150, 1.2f, 0xC9A77A);
                line(cv, 144, 132 - k, 108, 150, 1.2f, 0xC9A77A);
                draw::arc(cv, 160, 196, 30, kPi * 1.05f, kPi * 1.6f, 0.8f, hex(0xD9584A));
                draw::circle(cv, 192, 198, 11, hex(0xD9584A));
            }
            if (act == Act::Cosy) book(cv, 120, 150, 0xB8A0A8);
            if (who == Mascot::Cat) {
                if (act == Act::Rest) draw_dog(cv, 180, 186, 0.8f, -1, DogPose::Sleep, t);
                else draw_dog(cv, 70, 204, 0.85f, 1, DogPose::Sleep, t);
            }
            break;
        }
        case Act::HomeFromWork:
        case Act::GetReady:
        case Act::WakeUp:
        case Act::Errands:
        case Act::Workshop: {
            indoor(cv, in, act, t, dark);
            if (act == Act::WakeUp) {  // still in bed, the dog jumps up
                headboard(cv, 0x8A6A4E);
                draw::ellipse(cv, 112, 124, 46, 15, hex(0xE8ECF5));
                cx = 112; cy = 136; s = 0.76f;
                l.face = Face::Happy;
                draw_mascot(cv, cx, cy, s, l, t);
                box(cv, 6, 150, 234, 240, 0xC27BA0);
                line(cv, 14, 150, 226, 150, 4, 0xD8A0BC);
                draw_dog(cv, 186, 152 - std::fabs(wave(t, 900)) * 10, 1.0f, -1, DogPose::Stand, t);
                decor = false;
                break;
            }
            if (act == Act::Workshop) {
                cx = 112; cy = 124; s = 0.78f;
                l.face = Face::Open;
                for (int i = 0; i < 3; ++i) line(cv, 40 + static_cast<float>(i) * 16, 60, 40 + static_cast<float>(i) * 16, 92, 1.6f, 0x6B6B70);  // tools on the wall
                draw::ellipse(cv, 40, 58, 6, 3, hex(0x6B6B70));
                draw_mascot(cv, cx, cy, s, l, t);
                table(cv, dark);
                box(cv, 60, 160, 170, 170, 0xE2C49A);  // a plank on the bench
                const float sx = wave(t, 500) * 6;
                const Pt saw[4] = {{130 + sx, 150}, {176 + sx, 156}, {172 + sx, 166}, {126 + sx, 162}};
                draw::polygon(cv, saw, 4, hex(0xB8BEC8));
                box(cv, 114 + sx, 148, 132 + sx, 162, 0x8B5E3C);
                for (int i = 0; i < 4; ++i) draw::circle(cv, 96 + static_cast<float>(i) * 9, 176 + std::fabs(wave(t + static_cast<std::uint32_t>(i) * 200u, 700)) * 4, 1.4f, hex(0xE2C49A));
                at_table = true;
                break;
            }
            cx = 108; cy = 120; s = 0.78f;
            l.face = Face::Open;
            draw_mascot(cv, cx, cy, s, l, t);
            if (act == Act::HomeFromWork) {  // towel on the head, muddy boots by the door
                draw::ellipse(cv, cx, cy + (82 - 120) * s, 30, 12, hex(0x7FC8F0));
                for (const float bx : {30.0f, 52.0f}) {
                    box(cv, bx, 178, bx + 14, 200, 0x6B4A30);
                    draw::ellipse(cv, bx + 10, 200, 12, 5, hex(0x6B4A30));
                }
                for (int i = 0; i < 4; ++i) draw::circle(cv, 34 + static_cast<float>(i) * 9, 208 - static_cast<float>(i % 2) * 3, 2, hex(0x4A3020));
            }
            if (act == Act::GetReady) {
                box(cv, 166, 172, 200, 192, 0x3E7CB1);
                box(cv, 166, 170, 200, 176, 0x5C9AD0);
                for (const float bx : {188.0f, 206.0f}) {
                    box(cv, bx, 178, bx + 12, 200, 0x6B4A30);
                    draw::ellipse(cv, bx + 8, 200, 10, 4, hex(0x6B4A30));
                }
            }
            if (act == Act::Errands) {
                if (in.weekday % 2 == 0) {  // the shopping basket
                    box(cv, 150, 170, 196, 196, 0xC98F5E);
                    draw::arc(cv, 173, 170, 18, kPi, 2 * kPi, 1.6f, hex(0x8B5E3C));
                    line(cv, 158, 166, 186, 150, 4, 0xE8C27A);
                    draw::circle(cv, 182, 166, 6, hex(0xD9433A));
                } else {  // dusting
                    const float k = wave(t, 600) * 6;
                    line(cv, 150, 150, 168 + k, 120, 1.4f, 0x8B5E3C);
                    draw::circle(cv, 170 + k, 116, 8, hex(0xE8A0BC));
                }
                if (who == Mascot::Cat) draw_dog(cv, 46, 190, 0.9f, 1, DogPose::Sit, t);
            }
            break;
        }
        case Act::Breakfast:
        case Act::Morning:
        case Act::Lunch:
        case Act::Dinner:
        case Act::Supper:
        case Act::CoffeeCake:
        case Act::Roast: {
            indoor(cv, in, act, t, dark);
            cx = 112; cy = 124; s = 0.8f;
            l.face = (act == Act::CoffeeCake || act == Act::Roast || act == Act::Dinner) ? Face::Happy : Face::Open;
            if (who == Mascot::Penguin && act == Act::Morning) l.glasses = true;
            draw_mascot(cv, cx, cy, s, l, t);
            table(cv, dark);
            at_table = true;
            if (act == Act::Morning) {
                newspaper(cv, 26, 122);
                draw::ellipse(cv, 84, 146, 7, 8, hex(who == Mascot::Stag ? 0x9A5F3B : 0x2B3145));
                mug(cv, 166, 160, 0x3E7CB1, t);
            }
            if (act == Act::Breakfast) {
                plate(cv, 96, 176);
                if (who == Mascot::Bunny) { draw::ellipse(cv, 96, 172, 16, 6, hex(0xF2C14E)); }
                else { const Pt cr[3] = {{80, 174}, {112, 174}, {96, 160}}; draw::polygon(cv, cr, 3, hex(0xE0A04A)); }
                mug(cv, 154, 160, 0xD9534F, t);
                if (who == Mascot::Cat) {
                    draw_dog(cv, 204, 182, 1.0f, -1, DogPose::Sit, t);
                    dog_bone(cv, 184, 150 + wave(t, 800) * 2);
                }
            }
            if (act == Act::Lunch || act == Act::Dinner || act == Act::Supper || act == Act::Roast) {
                plate(cv, 104, 176);
                if (act == Act::Dinner && who == Mascot::Bunny) {  // pizza
                    const Pt pz[3] = {{88, 176}, {120, 176}, {104, 160}};
                    draw::polygon(cv, pz, 3, hex(0xF2C14E));
                    for (const Pt d : {Pt{100, 170}, Pt{108, 172}, Pt{104, 166}}) draw::circle(cv, d.x, d.y, 2, hex(0xD9433A));
                } else if (act == Act::Supper) {  // bread and cheese
                    draw::ellipse(cv, 98, 172, 12, 6, hex(0xC98F5E));
                    const Pt ch[3] = {{108, 175}, {124, 175}, {120, 164}};
                    draw::polygon(cv, ch, 3, hex(0xF2D04E));
                } else if (act == Act::Roast) {
                    draw::ellipse(cv, 104, 170, 18, 9, hex(0x8E4A2A));
                    draw::ellipse(cv, 100, 166, 8, 3, hex(0xB8683A));
                    draw::circle(cv, 120, 172, 3, hex(0x5FA84E));
                } else {
                    dome(cv, 104, 175, 16, 9, 0xE8C27A);
                    draw::circle(cv, 100, 168, 2.5f, hex(0x5FA84E));
                }
                if (who == Mascot::Stag && act == Act::Lunch) {  // an apple for dessert, one bite gone
                    draw::circle(cv, 152, 136, 10, hex(0xD9433A));
                    draw::circle(cv, 160, 130, 4.5f, hex(0xF6E7C8));
                    line(cv, 152, 126, 154, 120, 1.2f, 0x6B4A30);
                    draw::ellipse(cv, 159, 121, 5, 2.5f, hex(0x5FA84E), -0.3f);
                    draw::ellipse(cv, 146, 146, 6, 5, hex(0x9A5F3B));
                }
                if (who == Mascot::Cat) draw_dog(cv, 202, 188, 0.9f, -1, DogPose::Sit, t);
            }
            if (act == Act::CoffeeCake) {
                plate(cv, 92, 176);
                cake_slice(cv, 92, 174);
                props::tea_cup(cv, 160, 162, 1.2f, t);
            }
            break;
        }
    }

    // the room's climate (at home only)
    if (sit == Sit::AirOut) props::window_open(cv, kWin, t, dark);
    if (sit == Sit::Party) {
        props::fireworks(cv, kWin, t);
        props::party_hat(cv, cx, cy + (70 - 120) * s, 30 * s, t);
    }
    const float sx = at_table ? 196.0f : 206.0f, sy = at_table ? kTable - 12 : 186.0f;
    if (sit == Sit::Hot) props::water_glass(cv, sx, sy, 1.3f);
    if (sit == Sit::Cold) props::tea_cup(cv, sx, sy, 1.2f, t);
    if (sit == Sit::Cocoa) props::cocoa_mug(cv, sx, sy, 1.2f, t);

    // special days: a little something in the corner
    if (decor && sit == Sit::None) {
        const float y = at_table ? kTable + 1 : 200.0f;
        switch (occ) {
            case Occasion::Christmas: props::xmas_tree(cv, 206, y, 1.5f, t); break;
            case Occasion::Easter: props::egg_basket(cv, 206, y, 1.4f); break;
            case Occasion::Birthday: props::cake(cv, 204, y, 1.4f, t, false); break;
            default: break;
        }
    }

    const char* lines[4] = {};
    int n = 0;
    const char* main = sit_label(sit, in);
    if (!main) main = say ? say : act_label(act);
    lines[n++] = main;
    if (const char* o = occasion_text(occ); o && sit != Sit::Party) lines[n++] = o;
    if (occ == Occasion::Birthday && in.schedule.birthday_name[0]) lines[n++] = in.schedule.birthday_name;
    caption(cv, lines, n, t);
}

}  // namespace screen
