// screen: the squirrel reacting to a visit from the pet page, see friend_view.hpp. Pure C++.
#include "screen/core/friend_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "screen/core/draw.hpp"
#include "screen/core/props.hpp"
#include "screen/core/shop.hpp"

namespace screen::friends {
namespace {

using draw::hex;
constexpr float kPi = 3.14159265f;
constexpr std::uint32_t kFur = 0xE0873A, kFurDark = 0xB0602A, kBelly = 0xFBE4C4, kCheek = 0xFF8C9A, kWhite = 0xFFFFFF;

float smooth(float k) noexcept { k = std::clamp(k, 0.0f, 1.0f); return k * k * (3 - 2 * k); }
float pulse(float k, float a, float b, float e) noexcept { return std::min(smooth((k - a) / e), smooth((b - k) / e)); }
float ph(std::uint32_t t, std::uint32_t period) noexcept { return static_cast<float>(t % period) / static_cast<float>(period); }

// Squirrel design coordinates (body centre at 125,150) to the screen, following the body's bob and hop.
struct Map {
    float cx, cy, s, dx, dy;
    float X(float x) const { return cx + (x - 125 + dx) * s; }
    float Y(float y) const { return cy + (y - 150 + dy) * s; }
};

Map map_for(const AirData& d, const Pose& p, float cx, float cy, float s) {
    const Bob b = squirrel_bob(p.face, d.t_ms);
    const bool hops = p.face == Mood::VeryGood || p.face == Mood::Good || p.face == Mood::Okay;
    return {cx, cy, s, b.dx, b.dy - (hops ? p.q.hop * 6 : 0.0f)};
}

void arm(const Canvas& cv, const Map& m, float sx, float sy, float px, float py) {
    draw::capsule(cv, m.X(sx), m.Y(sy), m.X(px), m.Y(py), 5.5f * m.s, hex(kFurDark));
    draw::capsule(cv, m.X(sx), m.Y(sy), m.X(px), m.Y(py), 4.4f * m.s, hex(kFur));
    draw::circle(cv, m.X(px), m.Y(py), 4.6f * m.s, hex(kFur));
}

bool holds(Action a) { return a == Action::Feed || a == Action::Coffee || a == Action::Gift || a == Action::Water; }

}  // namespace

Pose pose(const AirData& d, Mood mood, Outfit outfit, fun::Quirks q) noexcept {
    const Reaction& r = d.react.r;
    const float k = d.react.k;
    const std::uint32_t t = d.t_ms;
    outfit.umbrella = outfit.windy = outfit.ice_cream = false;  // paws free for the visit
    outfit.paws_free = holds(r.action);
    q.wave = q.toss = q.howl = 0;
    if (r.unbox) {  // a new thing from the shop: holds the box, then shows it off
        outfit.paws_free = k < 0.4f;
        if (k < 0.4f) return {Mood::Good, outfit, q};
        q.hop = std::fabs(std::sin(static_cast<float>(t) / 230.0f)) * (k < 0.8f ? 1.0f : 0.0f);
        return {Mood::VeryGood, outfit, q};
    }
    Mood face = Mood::VeryGood;
    switch (r.action) {
        case Action::Feed:
            face = r.too_full ? Mood::Good : Mood::VeryGood;
            break;
        case Action::Coffee:
        case Action::Water:
        case Action::Gift:
            face = Mood::Good;
            if (r.action == Action::Gift && k > 0.2f && k < 0.6f) face = Mood::VeryGood;  // admires it
            break;
        case Action::Party:
            q.hop = std::fabs(std::sin(static_cast<float>(t) / (d.level >= 3 ? 170.0f : 220.0f)));
            break;
        case Action::Pet:
            q.ear = std::fabs(std::sin(static_cast<float>(t) / 300.0f));
            q.tail = std::sin(static_cast<float>(t) / 260.0f);
            break;
        case Action::Play:
            q.toss = std::fmod(k * 4, 1.0f);  // four catches
            break;
        case Action::GoodNight:
            face = k < 0.25f ? Mood::Good : Mood::Sleepy;
            break;
    }
    (void)mood;
    return {face, outfit, q};
}

void back(const Canvas& cv, const AirData& d) noexcept {
    const Reaction& r = d.react.r;
    if (r.action == Action::Party) props::bunting(cv, 30, d.t_ms, false);
    if (r.action == Action::Water) {
        const float x = 64, y = 312 - std::sqrt(143.0f * 143.0f - (x - 125) * (x - 125)) + 2;
        props::memory_plant(cv, x, y, 2.2f, d.t_ms, d.plant.leaves, d.plant.droop, d.plant.flower);
    }
}

void front(const Canvas& cv, const AirData& d, const Pose& p, float cx, float cy, float s) noexcept {
    const Reaction& r = d.react.r;
    const float k = d.react.k;
    const std::uint32_t t = d.t_ms;
    const Map m = map_for(d, p, cx, cy, s);
    const float hx = m.X(125), head_top = m.Y(p.outfit.hat ? 60 : 76);
    if (r.unbox) {
        if (k < 0.4f) {  // the present, shaken a little, the lid pops off
            const float shake = k > 0.25f ? std::sin(static_cast<float>(t) / 40.0f) * 2 : 0.0f;
            props::present(cv, m.X(125) + shake, m.Y(150), 2.0f, t);
            arm(cv, m, 106, 128, 110, 140);
            arm(cv, m, 144, 128, 140, 140);
        } else {
            const ShopItem& it = shop_item(r.item);
            if (it.shelf == Shelf::Decor && it.spot == Spot::Desk) {  // holds up the new thing for the room
                const float iy = m.Y(146);
                switch (it.art) {
                    case 4: props::aquarium(cv, m.X(125), iy, 1.3f, t); break;
                    case 5: props::lava_lamp(cv, m.X(125), iy, 1.2f, t); break;
                    case 6: props::record_player(cv, m.X(125), iy, 1.2f, t); break;
                    case 7: props::cactus(cv, m.X(125), iy, 1.4f, t); break;
                    default: break;
                }
            }
            if (it.shelf == Shelf::Decor && it.spot == Spot::Wall) {
                if (it.art == 2) props::photo_frame(cv, m.X(125), m.Y(136), 1.3f);
                else props::poster(cv, m.X(125), m.Y(134), 1.2f);
            }
            if (it.shelf == Shelf::Decor && it.spot == Spot::Window && it.art == 0) props::fairy_lights(cv, m.X(95), m.X(155), m.Y(132), t);
            for (int i = 0; i < 6; ++i) {  // sparkles all around
                const float a = static_cast<float>(i) * kPi / 3 + static_cast<float>(t) / 500.0f;
                const float tw = std::fabs(std::sin(static_cast<float>(t) / 180.0f + static_cast<float>(i)));
                draw::sparkle(cv, hx + std::cos(a) * 52, m.Y(118) + std::sin(a) * 46, 3 + 4 * tw, hex(i & 1 ? 0xFFE27A : kWhite));
            }
        }
        return;
    }
    switch (r.action) {
        case Action::Feed: {
            if (r.too_full) {  // a round belly, patted happily
                draw::ellipse(cv, m.X(126), m.Y(158), 25 * s, 28 * s, hex(kFurDark));
                draw::ellipse(cv, m.X(126), m.Y(158), 23 * s, 26 * s, hex(kBelly));
                draw::circle(cv, m.X(118), m.Y(152), 3 * s, hex(kWhite), 120);
            }
            const float catch_k = 0.22f;
            if (k < catch_k) {  // the treat flies in
                const float e = k / catch_k;
                const float x = 24 + (m.X(125) - 24) * e, y = 20 + (m.Y(138) - 20) * e - std::sin(kPi * e) * 40;
                props::treat(cv, x, y, 1.6f, r.item, e * 8);
                arm(cv, m, 106, 128, 112, 132);
                arm(cv, m, 144, 128, 138, 132);
                break;
            }
            const float bite = r.too_full ? 0.0f : pulse(ph(t, 1600), 0.3f, 0.6f, 0.08f);
            const float ty = 138 - bite * 18;
            props::treat(cv, m.X(125), m.Y(ty), 1.6f, r.item, 0);
            arm(cv, m, 106, 128, 115, ty + 3);
            arm(cv, m, 144, 128, 135, ty + 3);
            if (!r.too_full && k > 0.3f) {  // full cheeks
                const float puff = 5 + std::sin(static_cast<float>(t) / 140.0f);
                for (const float x : {101.0f, 149.0f}) {
                    draw::circle(cv, m.X(x), m.Y(112), (4 + puff) * s, hex(kFur));
                    draw::circle(cv, m.X(x), m.Y(112), (3 + puff) * s, hex(kCheek), 110);
                }
            }
            if (k > 0.3f) props::hearts_rising(cv, hx + 20, head_top, t);
            break;
        }
        case Action::Coffee: {
            const float sip = pulse(ph(t, 4000), 0.55f, 0.9f, 0.08f), my = 140 - sip * 16;
            props::cocoa_mug(cv, m.X(125), m.Y(my), 1.2f, t);
            arm(cv, m, 106, 128, 113, my + 3);
            arm(cv, m, 144, 128, 137, my + 3);
            if (k < 0.3f) draw::sparkle(cv, m.X(140), m.Y(my - 14), 5, hex(kWhite), static_cast<int>(256 * (1 - k / 0.3f)));  // clink
            break;
        }
        case Action::Gift: {
            const float up = pulse(k, 0.2f, 0.6f, 0.1f), gy = 140 - up * 12;
            arm(cv, m, 106, 128, 114, gy + 6);
            arm(cv, m, 144, 128, 136, gy + 6);
            props::gift_icon(cv, m.X(125), m.Y(gy), 2.4f, r.item);  // held up in front of the paws
            props::hearts_rising(cv, hx + 22, head_top, t);
            if (up > 0.5f) draw::sparkle(cv, m.X(140), m.Y(gy - 10), 4 + 3 * up, hex(0xFFF7B0));
            break;
        }
        case Action::Water: {
            const float tilt = pulse(ph(t, 5000), 0.2f, 0.85f, 0.15f);
            props::watering_can(cv, m.X(86), m.Y(126), 1.4f, tilt, t);
            arm(cv, m, 106, 128, 92, 126);
            arm(cv, m, 144, 128, 140, 150);
            break;
        }
        case Action::Party: {
            props::party_hat(cv, hx, m.Y(81), 44, t);
            props::confetti(cv, t);
            if (d.level >= 3) {  // best friends get the fireworks of sparkles
                for (int i = 0; i < 4; ++i) {
                    const float a = static_cast<float>(i) * kPi / 2 + static_cast<float>(t) / 400.0f;
                    draw::sparkle(cv, hx + std::cos(a) * 46, m.Y(110) + std::sin(a) * 40, 4 + 3 * std::fabs(std::sin(static_cast<float>(t) / 200.0f + static_cast<float>(i))),
                                  hex(i & 1 ? 0xFFE27A : kWhite));
                }
            }
            break;
        }
        case Action::Pet:
            props::hearts_rising(cv, hx + 18, head_top, t);
            props::hearts_rising(cv, hx - 24, head_top + 6, t + 900);
            break;
        case Action::Play:
            if (std::fmod(k * 4, 1.0f) > 0.9f) draw::sparkle(cv, m.X(125), m.Y(132), 6, hex(0xFFF7B0));  // caught!
            break;
        case Action::GoodNight:
            props::nightcap(cv, hx, m.Y(80), 38);
            for (int i = 0; i < 3; ++i) {  // Zzz
                const float z = ph(t + static_cast<std::uint32_t>(i) * 1000u, 3000);
                if (z > 0.85f || k < 0.25f) continue;
                draw_text(cv, static_cast<int>(hx + 30 + z * 12), static_cast<int>(head_top + 10 - z * 30), 1 + (z > 0.3f) + (z > 0.6f), hex(0xE8E8FF), "Z");
            }
            break;
    }
}

void labels(const Canvas& cv, const AirData& d) noexcept {
    const Reaction& r = d.react.r;
    const float k = d.react.k;
    if (r.unbox) {
        const ShopItem& it = shop_item(r.item);
        const char* top = "WHAT'S INSIDE?";
        if (k >= 0.4f) {
            switch (it.shelf) {
                case Shelf::Hat: case Shelf::Face: case Shelf::Body: top = "LOOK AT ME!"; break;
                case Shelf::Place: top = "A NEW PLACE!"; break;
                case Shelf::Friend: top = "A NEW FRIEND!"; break;
                case Shelf::Decor: top = "FOR MY ROOM!"; break;
            }
        }
        const bool big = text_width(top, 3) <= 140;
        draw::label(cv, big ? 180 : 186, big ? 3 : 2, hex(0xFFF2B8), top);
        if (k >= 0.4f) draw::label(cv, 208, 2, hex(0xFF8FA8), it.name);
        return;
    }
    const char* word = "";
    switch (r.action) {
        case Action::Feed: word = r.too_full ? "TOO FULL!" : "YUM!"; break;
        case Action::Coffee: word = "CHEERS!"; break;
        case Action::Party: word = "PARTY!"; break;
        case Action::Pet: word = "PURRR"; break;
        case Action::Play: word = "CATCH!"; break;
        case Action::Water: word = "FRESH WATER!"; break;
        case Action::GoodNight: word = "NIGHT NIGHT"; break;
        case Action::Gift: word = r.new_gift ? "FOR ME? THANKS!" : "THANK YOU!"; break;
    }
    // the first line: a welcome back or a new level first, then the reaction itself
    char top[24];
    if (r.welcome_back && k < 0.4f) std::snprintf(top, sizeof top, "YOU'RE BACK!");
    else if (r.level_up && k < 0.7f) std::snprintf(top, sizeof top, "%s!", level_name(r.level));
    else std::snprintf(top, sizeof top, "%s", word);
    const bool big = text_width(top, 3) <= 140;
    draw::label(cv, big ? 180 : 186, big ? 3 : 2, hex(0xFFF2B8), top);
    // the second line: the hearts, now and then the squirrel's name
    char bottom[24];
    if (d.pet_name[0] && d.level >= 2 && ph(d.t_ms, 6000) > 0.5f) std::snprintf(bottom, sizeof bottom, "I'M %s", d.pet_name);
    else {
        int i = 0;
        for (; i <= std::clamp(d.level, 0, kLevels - 1); ++i) bottom[i] = '#';
        bottom[i] = 0;
    }
    draw::label(cv, 208, 2, hex(0xFF8FA8), bottom);
}

}  // namespace screen::friends
