// screen: the weather page. Animated sky for today's forecast plus the outdoor temperature. Pure C++.
#include <cmath>
#include <cstdio>

#include "screen/core/draw.hpp"
#include "screen/core/pages.hpp"

namespace screen {
namespace {

using draw::hex;
using draw::Pt;
using weather::Sky;

constexpr float kPi = 3.14159265f;
float ph(std::uint32_t t, std::uint32_t period) noexcept {
    return static_cast<float>(t % period) / static_cast<float>(period);
}
float wave(std::uint32_t t, std::uint32_t period) noexcept { return std::sin(2 * kPi * ph(t, period)); }
std::uint32_t tt(std::uint32_t t, int i, std::uint32_t step) noexcept { return t + static_cast<std::uint32_t>(i) * step; }

struct Palette { std::uint32_t top, bottom; };

Palette sky_colors(Sky s, bool day) noexcept {
    switch (s) {
        case Sky::Clear: return day ? Palette{0x3FA9F5, 0xBDE8FF} : Palette{0x0B1A3A, 0x2E3570};
        case Sky::PartlyCloudy: return day ? Palette{0x5AB4EE, 0xCDEBFA} : Palette{0x14234A, 0x353C6E};
        case Sky::Cloudy: return day ? Palette{0x8A9DB2, 0xCBD5DF} : Palette{0x343E52, 0x5D6779};
        case Sky::Fog: return day ? Palette{0xA3ADB8, 0xDDE2E6} : Palette{0x4A5260, 0x737B88};
        case Sky::Drizzle:
        case Sky::Rain: return day ? Palette{0x56687D, 0x93A2B2} : Palette{0x2A3546, 0x4C5868};
        case Sky::Snow: return day ? Palette{0x93AAC2, 0xE6EDF5} : Palette{0x3A4A63, 0x7C8DA6};
        case Sky::Thunder: return {0x262B38, 0x535B73};
    }
    return {0x3FA9F5, 0xBDE8FF};
}

void sun(const Canvas& cv, float cx, float cy, float r, std::uint32_t t) {
    draw::circle(cv, cx, cy, r + 13, hex(0xFFE680), 60);
    const float rot = ph(t, 12000) * 2 * kPi;
    for (int i = 0; i < 10; ++i) {
        const float a = rot + static_cast<float>(i) * 2 * kPi / 10;
        const float len = (i & 1) ? 10.0f : 14.0f;
        draw::capsule(cv, cx + std::cos(a) * (r + 5), cy + std::sin(a) * (r + 5), cx + std::cos(a) * (r + 5 + len),
                      cy + std::sin(a) * (r + 5 + len), 2.4f, hex(0xFFC93C));
    }
    draw::circle(cv, cx, cy, r, hex(0xFFD23F));
    draw::circle(cv, cx - r * 0.3f, cy - r * 0.3f, r * 0.45f, hex(0xFFF0A0), 140);
    // a happy little face
    const float e = r * 0.33f;
    const bool blink = ph(t, 4100) > 0.96f;
    if (blink) {
        draw::capsule(cv, cx - e - 2.5f, cy - 2, cx - e + 2.5f, cy - 2, 1.2f, hex(0x6B3A12));
        draw::capsule(cv, cx + e - 2.5f, cy - 2, cx + e + 2.5f, cy - 2, 1.2f, hex(0x6B3A12));
    } else {
        draw::ellipse(cv, cx - e, cy - 3, 2.4f, 3.2f, hex(0x6B3A12));
        draw::ellipse(cv, cx + e, cy - 3, 2.4f, 3.2f, hex(0x6B3A12));
    }
    draw::arc(cv, cx, cy + 1, r * 0.38f, 0.5f, kPi - 0.5f, 1.3f, hex(0x6B3A12));
    draw::circle(cv, cx - r * 0.55f, cy + 3, 3.2f, hex(0xFF9A6B), 150);
    draw::circle(cv, cx + r * 0.55f, cy + 3, 3.2f, hex(0xFF9A6B), 150);
}

void moon(const Canvas& cv, float cx, float cy, float r, std::uint32_t sky_at_moon) {
    draw::circle(cv, cx, cy, r + 10, hex(0xFFF1B0), 30);
    draw::circle(cv, cx, cy, r, hex(0xFFF1B0));
    draw::circle(cv, cx + r * 0.45f, cy - r * 0.3f, r * 0.82f, hex(sky_at_moon));
}

void stars(const Canvas& cv, std::uint32_t t) {
    static constexpr Pt kStars[] = {{48, 70}, {76, 40}, {178, 36}, {204, 92}, {36, 116}, {150, 24}, {200, 140}, {64, 150}};
    for (int i = 0; i < 8; ++i) {
        const float tw = (wave(tt(t, i, 517), 2600) + 1) * 0.5f;
        draw::sparkle(cv, kStars[i].x, kStars[i].y, 2 + tw * 2.5f, hex(0xFFF6D0), 100 + static_cast<int>(tw * 150));
    }
}

void cloud(const Canvas& cv, float cx, float cy, float k, std::uint32_t color, std::uint32_t shade) {
    struct C { float x, y, r; };
    static constexpr C kPuffs[] = {{-22, 4, 14}, {-5, -8, 19}, {15, -2, 15}, {27, 6, 11}};
    for (int pass = 0; pass < 2; ++pass) {
        const float off = pass == 0 ? 3.0f : 0.0f;
        const auto c = hex(pass == 0 ? shade : color);
        for (const C& p : kPuffs) draw::circle(cv, cx + p.x * k, cy + p.y * k + off, p.r * k, c);
        draw::capsule(cv, cx - 23 * k, cy + 10 * k + off, cx + 27 * k, cy + 10 * k + off, 9 * k, c);
    }
    draw::circle(cv, cx - 9 * k, cy - 12 * k, 7 * k, hex(0xFFFFFF), 110);
}

void rain(const Canvas& cv, std::uint32_t t, int drops, float len, std::uint32_t color) {
    for (int i = 0; i < drops; ++i) {
        const std::uint32_t period = 650u + static_cast<std::uint32_t>(i % 4) * 90u;
        const float p = ph(tt(t, i, 211), period);
        const float x0 = 28 + static_cast<float>((i * 53) % 190);
        const float y = 70 + p * 180, x = x0 - p * 18;
        draw::capsule(cv, x, y, x - len * 0.25f, y + len, 1.3f, hex(color), 200);
    }
}

void snow(const Canvas& cv, std::uint32_t t) {
    for (int i = 0; i < 26; ++i) {
        const std::uint32_t period = 4200u + static_cast<std::uint32_t>(i % 5) * 800u;
        const float p = ph(tt(t, i, 389), period);
        const float x = 14 + static_cast<float>((i * 97) % 212) + wave(tt(t, i, 300), 2200) * 6;
        const float y = 30 + p * 220;
        const float r = 1.8f + static_cast<float>(i % 3) * 0.9f;
        if (i % 6 == 0) draw::sparkle(cv, x, y, r * 2.4f, hex(0xFFFFFF), 230);
        else draw::circle(cv, x, y, r, hex(0xFFFFFF), 235);
    }
}

void bolt(const Canvas& cv, float x, float y, int alpha) {
    const Pt a[4] = {{x + 4, y}, {x + 13, y}, {x + 5, y + 16}, {x - 4, y + 16}};
    const Pt b[3] = {{x - 4, y + 14}, {x + 12, y + 14}, {x - 2, y + 36}};
    draw::polygon(cv, a, 4, hex(0xFFE14D), alpha);
    draw::polygon(cv, b, 3, hex(0xFFE14D), alpha);
}

}  // namespace

void render_weather(const Canvas& cv, const Inputs& in) noexcept {
    const std::uint32_t t = in.t_ms;
    const auto& w = in.weather;
    const Sky s = in.weather_ok ? w.sky : Sky::Cloudy;
    const bool day = !in.weather_ok || w.is_day;
    Palette pal = sky_colors(s, day);
    const bool flash = s == Sky::Thunder && (ph(t, 3200) < 0.06f || (ph(t, 3200) > 0.1f && ph(t, 3200) < 0.13f));
    if (flash) pal = {draw::lerp_rgb(pal.top, 0xFFFFFF, 140), draw::lerp_rgb(pal.bottom, 0xFFFFFF, 110)};
    draw::vgradient(cv, pal.top, pal.bottom);

    const float drift = wave(t, 7000) * 5;
    const float icx = 120, icy = 74;
    switch (s) {
        case Sky::Clear:
            if (day) sun(cv, icx, icy, 26, t);
            else { stars(cv, t); moon(cv, icx, icy, 24, draw::lerp_rgb(pal.top, pal.bottom, 70)); }
            break;
        case Sky::PartlyCloudy:
            if (day) sun(cv, icx + 20, icy - 12, 21, t);
            else { stars(cv, t); moon(cv, icx + 20, icy - 12, 19, draw::lerp_rgb(pal.top, pal.bottom, 50)); }
            cloud(cv, icx - 10 + drift, icy + 12, 1.0f, 0xFFFFFF, 0xC9D6E3);
            break;
        case Sky::Cloudy:
            cloud(cv, icx + 22 - drift, icy - 12, 0.8f, 0xD5DDE6, 0xA9B6C4);
            cloud(cv, icx - 8 + drift, icy + 8, 1.05f, 0xF4F7FA, 0xBCC8D4);
            break;
        case Sky::Fog: {
            cloud(cv, icx + drift, icy, 1.0f, 0xE9EDF1, 0xB6C0CA);
            for (int i = 0; i < 4; ++i) {
                const float y = 92 + static_cast<float>(i) * 13, dx = wave(tt(t, i, 900), 5000) * 12;
                draw::capsule(cv, 50 + dx + static_cast<float>(i % 2) * 14, y, 190 + dx - static_cast<float>(i % 2) * 20, y, 4,
                              hex(0xF2F4F6), 150);
            }
            break;
        }
        case Sky::Drizzle:
            rain(cv, t, 10, 5, 0xBFE3FF);
            cloud(cv, icx + drift, icy, 1.05f, 0xE4EAF0, 0xA6B3C1);
            break;
        case Sky::Rain:
            rain(cv, t, 22, 9, 0xA8D8FF);
            cloud(cv, icx + drift, icy, 1.1f, 0xC9D2DC, 0x8E9BAA);
            break;
        case Sky::Snow:
            snow(cv, t);
            cloud(cv, icx + drift, icy - 4, 1.0f, 0xF4F7FA, 0xC2CEDA);
            break;
        case Sky::Thunder:
            rain(cv, t, 16, 9, 0x9CC7F0);
            if (flash) bolt(cv, icx - 4, icy + 18, 256);
            cloud(cv, icx + drift, icy, 1.15f, 0x7D8798, 0x555E6E);
            break;
    }

    const auto white = hex(0xFFFFFF);
    draw::label(cv, 18, 2, white, in.place);
    char buf[24];
    if (!in.weather_ok) {
        draw::label(cv, 130, 3, white, "NO WEATHER");
        draw::label(cv, 166, 2, white, "WAITING FOR WIFI");
        return;
    }
    auto deg = [](float c) { const long v = std::lround(c); return v == 0 ? 0L : v; };
    std::snprintf(buf, sizeof buf, "%ld^C", deg(w.temp_c));
    draw::label(cv, 122, 6, white, buf);
    draw::label(cv, 172, 2, white, weather::sky_name(s, w.is_day));
    std::snprintf(buf, sizeof buf, "HI %ld LO %ld", deg(w.max_c), deg(w.min_c));
    draw::label(cv, 194, 2, white, buf);
    if (w.precip_pct >= 0) {
        std::snprintf(buf, sizeof buf, "RAIN %d%%", w.precip_pct);
        draw::label(cv, 214, 2, hex(0xDDF0FF), buf);
    }
}

}  // namespace screen
