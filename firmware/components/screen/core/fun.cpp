// screen: little surprises, see fun.hpp. Pure C++.
#include "screen/core/fun.hpp"

#include <algorithm>
#include <cmath>

#include "screen/core/day.hpp"
#include "screen/core/draw.hpp"
#include "screen/core/pages.hpp"
#include "screen/core/squirrel.hpp"

namespace screen::fun {
namespace {

using draw::hex;
using draw::Pt;

constexpr float kPi = 3.14159265f;
float smooth(float k) noexcept { k = std::clamp(k, 0.0f, 1.0f); return k * k * (3 - 2 * k); }
float pulse(float k, float a, float b, float e) noexcept { return std::min(smooth((k - a) / e), smooth((b - k) / e)); }
float lerp(float a, float b, float k) noexcept { return a + (b - a) * k; }

// One chance per `slot` ms: with `pct` % probability an event of `dur` ms starts at a random moment in the slot.
// Returns its progress 0..1, or -1 when nothing happens now. `bits` gets random bits for the event.
float event(std::uint32_t t, std::uint32_t seed, std::uint32_t salt, std::uint32_t slot, std::uint32_t pct,
            std::uint32_t dur, std::uint32_t* bits = nullptr) noexcept {
    const std::uint32_t n = t / slot, r = mix(n * 0x9E3779B1u ^ seed ^ (salt * 0x85EBCA77u));
    if (r % 100 >= pct) return -1;
    const std::uint32_t start = n * slot + (r >> 8) % (slot - dur);
    if (t < start || t - start >= dur) return -1;
    if (bits) *bits = mix(r ^ 0x27D4EB2Fu);
    return static_cast<float>(t - start) / static_cast<float>(dur);
}

// A blue tit, feet at (x, y). face +1 looks right, -1 left. flap: wing angle while flying; peck 0..1 dips the head.
void bird(const Canvas& cv, float x, float y, float s, float face, bool flying, float flap, float peck) {
    auto P = [&](float dx, float dy) { return Pt{x + dx * face * s, y + dy * s}; };
    auto circle = [&](float dx, float dy, float r, std::uint32_t c) { const Pt q = P(dx, dy); draw::circle(cv, q.x, q.y, r * s, hex(c)); };
    auto ellipse = [&](float dx, float dy, float rx, float ry, std::uint32_t c, float a) {
        const Pt q = P(dx, dy);
        draw::ellipse(cv, q.x, q.y, rx * s, ry * s, hex(c), a * face);
    };
    constexpr std::uint32_t kBlue = 0x5B8FD6, kDark = 0x2F558F, kBelly = 0xFFD84A;
    const Pt tail[3] = {P(-5, -8), P(-12, -11), P(-11, -6)};
    draw::polygon(cv, tail, 3, hex(kDark));
    if (!flying) {
        const Pt l0 = P(-0.5f, -2), l1 = P(-1, 0), r0 = P(2, -2), r1 = P(2.5f, 0);
        draw::capsule(cv, l0.x, l0.y, l1.x, l1.y, 0.55f * s, hex(0x6B4A3A));
        draw::capsule(cv, r0.x, r0.y, r1.x, r1.y, 0.55f * s, hex(0x6B4A3A));
    }
    ellipse(0, -7, 7.2f, 6.2f, kDark, 0);
    ellipse(0, -7, 6.4f, 5.4f, kBlue, 0);
    ellipse(1.8f, -5.6f, 4.4f, 3.4f, kBelly, 0);
    const float hy = -12.5f + peck * 4.5f, hx = 4 + peck * 2;
    circle(hx, hy, 4.6f, kDark);
    circle(hx, hy, 4.0f, kBlue);
    circle(hx + 0.8f, hy + 1.2f, 2.1f, 0xFFFFFF);
    circle(hx + 1.6f, hy - 0.8f, 0.9f, 0x1A1A22);
    const Pt beak[3] = {P(hx + 3.6f, hy - 0.6f), P(hx + 6.6f, hy + 0.2f), P(hx + 3.6f, hy + 1.2f)};
    draw::polygon(cv, beak, 3, hex(0xF29A2E));
    if (flying) ellipse(-1, -9, 7, 2.6f, kDark, -flap * 0.9f);
    else ellipse(-1.5f, -7.5f, 4.8f, 3.0f, kDark, 0.3f);
}

void note(const Canvas& cv, float x, float y, float s, int alpha) {  // a little music note
    const auto c = hex(0x3A3550);
    draw::ellipse(cv, x, y, 2.0f * s, 1.5f * s, c, -0.4f, alpha);
    draw::capsule(cv, x + 1.7f * s, y, x + 1.7f * s, y - 7 * s, 0.5f * s, c, alpha);
    draw::capsule(cv, x + 1.7f * s, y - 7 * s, x + 4.2f * s, y - 5 * s, 0.5f * s, c, alpha);
}

}  // namespace

std::uint32_t mix(std::uint32_t x) noexcept {  // lowbias32
    x ^= x >> 16; x *= 0x7FEB352Du;
    x ^= x >> 15; x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

Quirks quirks(std::uint32_t t, std::uint32_t seed) noexcept {
    Quirks q;
    std::uint32_t r = 0;
    if (event(t, seed, 1, 3700, 85, 150) >= 0) q.blink = true;
    if (const float k = event(t, seed, 2, 9000, 30, 420); k >= 0) q.blink = k < 0.33f || k > 0.66f;  // double blink
    if (const float k = event(t, seed, 3, 8000, 45, 1600, &r); k >= 0) q.look = ((r & 1) ? 1.0f : -1.0f) * pulse(k, 0, 1, 0.15f);
    if (const float k = event(t, seed, 4, 6500, 35, 280); k >= 0) q.ear = std::sin(kPi * k);
    if (const float k = event(t, seed, 5, 7000, 30, 700); k >= 0) q.tail = std::sin(2 * kPi * k);
    if (const float k = event(t, seed, 6, 19000, 25, 500); k >= 0) q.hop = std::sin(kPi * k);
    if (const float k = event(t, seed, 7, 41000, 30, 2600); k >= 0) q.wave = pulse(k, 0, 1, 0.2f);
    if (const float k = event(t, seed, 8, 23000, 30, 1100); k >= 0) q.toss = k;
    return q;
}

Visit visit(std::uint32_t t, std::uint32_t seed, unsigned allowed) noexcept {
    allowed &= ~bit(Visitor::None);
    if (!allowed) return {};
    std::uint32_t r = 0;
    const float k = event(t, seed, 9, 50000, 50, 12000, &r);
    if (k < 0) return {};
    int n = 0;
    for (int i = 0; i < kVisitorCount; ++i) n += (allowed >> i) & 1;
    int pick = static_cast<int>(r % static_cast<std::uint32_t>(n));
    for (int i = 0; i < kVisitorCount; ++i)
        if (((allowed >> i) & 1) && pick-- == 0) return {static_cast<Visitor>(i), k, mix(r)};
    return {};
}

float golden(std::uint32_t t, std::uint32_t seed, bool lucky) noexcept {
    return event(t, seed, 10, 1200000, lucky ? 2 : 1, kGoldenMs);
}

float hello(std::uint32_t t, std::uint32_t seed) noexcept {
    const float k = event(t, seed, 14, 30000, 30, 2600);
    return k < 0 ? 0.0f : pulse(k, 0, 1, 0.2f);
}

float present_for_you(std::uint32_t t, std::uint32_t seed) noexcept { return event(t, seed, 15, 3600000, 40, 20000); }

float howl(std::uint32_t t, std::uint32_t seed) noexcept {
    const float k = event(t, seed, 11, 25000, 40, 3000);
    return k < 0 ? 0.0f : pulse(k, 0, 1, 0.12f);
}

GagNow gag(std::uint32_t t, std::uint32_t seed, unsigned allowed) noexcept {
    allowed &= ~bit(Gag::None);
    if (!allowed) return {};
    const unsigned window = allowed & kWindowGags;
    std::uint32_t r = 0;
    const float k = event(t, seed, 12, 45000, window ? 45 : 30, 7000, &r);
    if (k < 0) return {};
    const unsigned pool = window ? window : allowed;  // in its time window, the window's mood comes first
    int n = 0;
    for (int i = 0; i < kGagCount; ++i) n += (pool >> i) & 1;
    int pick = static_cast<int>(r % static_cast<std::uint32_t>(n));
    for (int i = 0; i < kGagCount; ++i)
        if (((pool >> i) & 1) && pick-- == 0) return {static_cast<Gag>(i), k, mix(r)};
    return {};
}

const char* pun(std::uint32_t t, std::uint32_t seed, int activity) noexcept {
    std::uint32_t r = 0;
    if (event(t, seed, 13, 150000, 30, 6000, &r) < 0) return nullptr;
    // at most 15 characters, so they fit the caption at the smaller size
    static constexpr const char* kPuns[][3] = {
        {"DREAMING NUTS", "SNORE-NUT", "ZZZ-QUIRREL"},          // sleep
        {"BREW-TIFUL", "BEAN THERE", "NUT LATTE"},              // coffee
        {"NUTS ABOUT WORK", "NUT-WORKING", "SQUIRRELLING"},     // work
        {"NUT-RITIOUS", "SOUP-ER!", "ACORN SOUP?"},             // lunch
        {"TAIL END!", "NUT SO FAST", "LOG OFF, OAK ON"},        // close laptop
        {"A NUT-VEL", "TAIL TIME", "PAWS TO READ"},             // read
    };
    static constexpr const char* kAny[] = {"ACORNY JOKE", "TREE-MENDOUS", "I'M NUTS!", "HAZEL-NICE"};
    const int a = std::clamp(activity, 0, 5);
    return (r >> 3) % 3 == 0 ? kAny[(r >> 5) % 4] : kPuns[a][(r >> 5) % 3];
}

void howl_text(const Canvas& cv, float mx, float my, float tx, float ty, float k, std::uint32_t t) noexcept {
    if (k <= 0.05f) return;
    const int a = static_cast<int>(256 * k);
    for (int i = 0; i < 3; ++i) {  // sound arcs going up and to the right
        const float r = 9 + static_cast<float>(i) * 5 + 2 * std::sin(static_cast<float>(t) / 120.0f + static_cast<float>(i));
        draw::arc(cv, mx, my, r, -kPi * 0.45f, -kPi * 0.05f, 0.9f, hex(0xFFF6D0), a * (3 - i) / 3);
    }
    const int x = static_cast<int>(tx), y = static_cast<int>(ty - 4 * k);
    draw_text(cv, x + 1, y + 1, 2, hex(0x1A1A2E), "AWOO");
    draw_text(cv, x, y, 2, draw::mix(hex(0x1A1A2E), hex(0xFFF6D0), a), "AWOO");
}

int rainbow_alpha(const Inputs& in) noexcept {
    const auto& w = in.weather;
    const weather::Sky now = weather::now_sky(w);
    if (!in.weather_ok || !w.is_day || in.since_rain_min < 0 || in.since_rain_min >= 90) return 0;
    if (now != weather::Sky::Clear && now != weather::Sky::PartlyCloudy) return 0;
    const float m = static_cast<float>(in.since_rain_min);
    return static_cast<int>(210 * std::min({1.0f, (m + 1) / 3, (90 - m) / 20}));
}

unsigned allowed_visitors(Page p, const Inputs& in) noexcept {
    const auto& w = in.weather;
    const bool dry_day = in.weather_ok && w.is_day && !weather::wet(w.sky) && w.wind_kmh < 30;
    const bool clear_night = in.weather_ok && !w.is_day && (w.sky == weather::Sky::Clear || w.sky == weather::Sky::PartlyCloudy);
    const bool after_rain = in.since_rain_min >= 0 && !weather::wet(weather::now_sky(w));
    const int mo = in.month;
    unsigned m = 0;
    if (after_rain && p != Page::Clock) m |= bit(Visitor::Snail);
    switch (p) {
        case Page::Air:
            if (dry_day && !outfit_for(in.weather_ok, w).umbrella) m |= bit(Visitor::Bird);
            if (dry_day && w.temp_c >= 12) m |= bit(Visitor::Butterfly);
            if (clear_night) m |= bit(Visitor::ShootingStar);
            if (in.time_valid && mo >= 9 && mo <= 11 && (in.hour >= 17 || in.hour < 7) && in.weather_ok && !weather::wet(w.sky))
                m |= bit(Visitor::Hedgehog);
            if (dry_day && mo >= 6 && mo <= 8 && w.temp_c >= 18) m |= bit(Visitor::Bee);
            if (dry_day && (mo == 10 || mo == 11)) m |= bit(Visitor::Leaves);
            if (dry_day && mo == 4) m |= bit(Visitor::Petals);
            if (level(in.bond) >= 4 && dry_day) {
                m |= bit(Visitor::Fox);
                const bool off = in.time_valid && (in.weekday == 0 || in.weekday == 6 || cal::public_holiday(in.year, in.month, in.day));
                if (off) m |= bit(Visitor::Friend);
            }
            break;
        case Page::Day: {
            if (!in.time_valid) break;
            const Scene sc = scene_for(in);
            const bool party = sc.situation == Situation::Party;
            const bool bed = (sc.activity == Activity::Sleep || sc.situation == Situation::Storm) && !party;
            if (dry_day) m |= bit(Visitor::Bird);
            if (dry_day && w.temp_c >= 15 && !bed) m |= bit(Visitor::Butterfly);
            if (!bed) m |= bit(Visitor::Ladybug);
            if (reading_lamp(sc, in)) m |= bit(Visitor::Moth);
            if (clear_night && !party) m |= bit(Visitor::ShootingStar);
            if (in.weather_ok && !w.is_day && w.sky != weather::Sky::Thunder && !party) m |= bit(Visitor::Owl);
            break;
        }
        case Page::Clock: break;
    }
    return m;
}

void bird_on_sill(const Canvas& cv, float x0, float x1, float y, const Visit& v, std::uint32_t t) noexcept {
    const float k = v.k, flap = std::sin(static_cast<float>(t) / 45.0f);
    const float land = x1 - 6, step = (x1 - x0 - 12) / 3;
    if (k < 0.12f) {  // flying in from the upper right
        const float e = smooth(k / 0.12f);
        bird(cv, lerp(x1 + 34, land, e), lerp(y - 34, y, e) - std::sin(kPi * e) * 6, 0.85f, -1, true, flap, 0);
        return;
    }
    if (k > 0.86f) {  // and off again
        const float e = smooth((k - 0.86f) / 0.14f);
        const float sx = land - 3 * step;
        bird(cv, lerp(sx, x1 + 50, e), lerp(y, y - 60, e), 0.85f, 1, true, flap, 0);
        return;
    }
    // three hops to the left, pecking between them, then it turns round
    float x = land, hop = 0;
    for (int i = 0; i < 3; ++i) {
        const float a = 0.22f + static_cast<float>(i) * 0.16f;  // hop i between a and a + 0.05
        const float e = smooth((k - a) / 0.05f);
        x -= step * e;
        if (k > a && k < a + 0.05f) hop = std::sin(kPi * (k - a) / 0.05f) * 4;
    }
    const float peck = std::max(pulse(k, 0.30f, 0.36f, 0.015f), pulse(k, 0.62f, 0.72f, 0.015f)) *
                       (0.5f + 0.5f * std::fabs(std::sin(static_cast<float>(t) / 70.0f)));
    bird(cv, x, y - hop, 0.85f, k > 0.76f ? 1.0f : -1.0f, false, 0, peck);
}

void bird_on_head(const Canvas& cv, float hx, float hy, float s, const Visit& v, std::uint32_t t) noexcept {
    const float k = v.k, flap = std::sin(static_cast<float>(t) / 45.0f);
    if (k < 0.14f) {
        const float e = smooth(k / 0.14f);
        bird(cv, lerp(hx - 100, hx, e), lerp(hy - 70, hy, e) - std::sin(kPi * e) * 10, s, 1, true, flap, 0);
        return;
    }
    if (k > 0.86f) {
        const float e = smooth((k - 0.86f) / 0.14f);
        bird(cv, lerp(hx, hx + 110, e), lerp(hy, hy - 90, e), s, 1, true, flap, 0);
        return;
    }
    const float bounce = pulse(k, 0.58f, 0.64f, 0.02f) * 5;
    const float peck = pulse(k, 0.32f, 0.38f, 0.015f);
    bird(cv, hx, hy - bounce, s, (k > 0.45f && k < 0.7f) ? -1.0f : 1.0f, false, 0, peck);
    for (int i = 0; i < 2; ++i) {  // it sings
        const float n = (k - 0.47f - static_cast<float>(i) * 0.06f) / 0.16f;
        if (n > 0 && n < 1) note(cv, hx + 8 * s + n * 10 * s, hy - 16 * s - n * 18 * s, s * 0.9f, static_cast<int>(256 * (1 - n)));
    }
}

void butterfly(const Canvas& cv, float x0, float y0, float x1, float y1, const Visit& v, std::uint32_t t) noexcept {
    static constexpr std::uint32_t kWings[][2] = {{0xFFB347, 0xFFE38A}, {0x7FB8F5, 0xCFE6FF}, {0xFF8FB8, 0xFFD3E2}, {0xFFF2A0, 0xFFFFFF}};
    const auto& c = kWings[v.r % 4];
    const float dir = (v.r >> 4) & 1 ? 1.0f : -1.0f, ph1 = static_cast<float>((v.r >> 8) % 628) / 100, ph2 = static_cast<float>((v.r >> 16) % 628) / 100;
    const float k = dir > 0 ? v.k : 1 - v.k;
    const float x = lerp(x0 - 15, x1 + 15, k) + std::sin(k * 2 * kPi * 1.5f + ph1) * 14;
    const float y = (y0 + y1) / 2 + std::sin(k * 2 * kPi * 2.3f + ph2) * (y1 - y0) / 2.6f + std::sin(static_cast<float>(t) / 380.0f) * 3;
    const float f = 0.25f + 0.75f * std::fabs(std::sin(static_cast<float>(t) / 85.0f));  // wing beat
    constexpr float z = 1.5f;  // size
    for (const float sd : {-1.0f, 1.0f}) {
        draw::ellipse(cv, x + sd * (4.2f * f + 0.6f) * z, y - 2 * z, (4.4f * f + 0.6f) * z, 3.8f * z, hex(draw::lerp_rgb(c[0], 0x000000, 50)), sd * 0.3f);
        draw::ellipse(cv, x + sd * (4.2f * f + 0.6f) * z, y - 2 * z, (3.8f * f + 0.4f) * z, 3.2f * z, hex(c[0]), sd * 0.3f);
        draw::ellipse(cv, x + sd * (3.2f * f + 0.6f) * z, y + 2.6f * z, (3.0f * f + 0.4f) * z, 2.4f * z, hex(c[1]), -sd * 0.3f);
        draw::capsule(cv, x, y - 3 * z, x + sd * 2.4f * z, y - 7 * z, 0.45f, hex(0x3A2E2A));
    }
    draw::capsule(cv, x, y - 3 * z, x, y + 3.5f * z, 1.2f, hex(0x3A2E2A));
}

void ladybug(const Canvas& cv, float x0, float x1, float y, const Visit& v, std::uint32_t t) noexcept {
    const float dir = (v.r & 1) ? 1.0f : -1.0f;
    const float k = v.k - std::sin(2 * kPi * v.k * 5) / (2 * kPi * 5) * 0.9f;  // walks, pauses, walks
    const float x = dir > 0 ? lerp(x0, x1, k) : lerp(x1, x0, k);
    const float leg = std::sin(static_cast<float>(t) / 60.0f) * 0.8f;
    for (int i = -1; i <= 1; ++i)
        draw::capsule(cv, x + static_cast<float>(i) * 2.2f, y, x + static_cast<float>(i) * 2.2f + leg, y + 2.4f, 0.4f, hex(0x1A1A1A));
    draw::circle(cv, x + dir * 3.8f, y - 1, 2.0f, hex(0x1A1A1A));  // head
    draw::ellipse(cv, x, y - 1.6f, 4.6f, 3.6f, hex(0x7A1010));
    draw::ellipse(cv, x, y - 1.8f, 4.0f, 3.1f, hex(0xE0302A));
    draw::capsule(cv, x, y - 4.6f, x, y + 1, 0.35f, hex(0x1A1A1A));
    for (const float sx : {-2.0f, 2.0f}) {
        draw::circle(cv, x + sx, y - 2.8f, 0.8f, hex(0x1A1A1A));
        draw::circle(cv, x + sx * 0.9f, y - 0.4f, 0.7f, hex(0x1A1A1A));
    }
    draw::circle(cv, x - 1.4f, y - 3.6f, 0.8f, hex(0xFFFFFF), 160);
}

void moth(const Canvas& cv, float cx, float cy, const Visit& v, std::uint32_t t) noexcept {
    const float a = static_cast<float>(t) / 330.0f + static_cast<float>(v.r % 628) / 100;
    const float R = 15 + 5 * std::sin(static_cast<float>(t) / 700.0f);
    float x = cx + std::cos(a) * R + std::sin(static_cast<float>(t) / 97.0f) * 2, y = cy + std::sin(a) * R * 0.6f;
    if (v.k < 0.12f) { const float e = smooth(v.k / 0.12f); x = lerp(-10, x, e); y = lerp(cy - 50, y, e); }
    if (v.k > 0.88f) { const float e = smooth((v.k - 0.88f) / 0.12f); x = lerp(x, cx + 40, e); y = lerp(y, cy - 90, e); }
    const float f = 0.3f + 0.7f * std::fabs(std::sin(static_cast<float>(t) / 30.0f));
    for (const float sd : {-1.0f, 1.0f}) {
        const Pt w[3] = {{x, y - 1}, {x + sd * 6 * f, y - 3}, {x + sd * 4.5f * f, y + 2.5f}};
        draw::polygon(cv, w, 3, hex(0xD9D2C4));
    }
    draw::capsule(cv, x, y - 2, x, y + 2.5f, 0.9f, hex(0x8C8072));
}

void shooting_star(const Canvas& cv, float x0, float y0, float x1, float y1, const Visit& v) noexcept {
    const float q = (v.k - 0.3f) / 0.07f;
    if (q < 0 || q > 1) return;
    const float sx = lerp(x0, x1, 0.55f + static_cast<float>(v.r % 40) / 100), sy = lerp(y0, y1, 0.05f + static_cast<float>((v.r >> 8) % 25) / 100);
    const float ex = sx - (x1 - x0) * 0.6f, ey = sy + (y1 - y0) * 0.55f;
    const float head = smooth(q), fade = 1 - smooth((q - 0.7f) / 0.3f);
    auto inside = [&](float x, float y) { return x > x0 + 1 && x < x1 - 1 && y > y0 + 1 && y < y1 - 1; };
    constexpr int kSeg = 8;
    for (int i = 0; i < kSeg; ++i) {
        const float a = head - 0.35f * static_cast<float>(i + 1) / kSeg, b = head - 0.35f * static_cast<float>(i) / kSeg;
        if (a < 0) break;
        const float ax = lerp(sx, ex, a), ay = lerp(sy, ey, a), bx = lerp(sx, ex, b), by = lerp(sy, ey, b);
        if (!inside(ax, ay) || !inside(bx, by)) continue;
        draw::capsule(cv, ax, ay, bx, by, 1.1f - 0.1f * static_cast<float>(i), hex(0xFFF6D0),
                      static_cast<int>(fade * 230 * (1 - static_cast<float>(i) / kSeg)));
    }
    const float hx = lerp(sx, ex, head), hy = lerp(sy, ey, head);
    const float room = std::min({hx - x0, x1 - hx, hy - y0, y1 - hy});  // keep the twinkle inside the box
    if (inside(hx, hy)) draw::sparkle(cv, hx, hy, std::min(4.5f * fade, room), hex(0xFFFFFF), static_cast<int>(256 * fade));
}

}  // namespace screen::fun
