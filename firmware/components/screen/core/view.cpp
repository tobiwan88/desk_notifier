#include "screen/core/view.hpp"

#include <cstdio>

namespace screen {
namespace {

constexpr std::uint32_t kDriftPeriodMs = 20u * 60u * 1000u;  // one full hue cycle in 20 minutes
const gfx::Rgb565be kWhite = gfx::rgb565_rt(255, 255, 255);

gfx::Rgb565be level_color(sensor::Level l) {
    switch (l) {
        case sensor::Level::Good: return gfx::rgb565_rt(120, 255, 140);
        case sensor::Level::Fair: return gfx::rgb565_rt(230, 240, 120);
        case sensor::Level::Stale: return gfx::rgb565_rt(255, 190, 80);
        case sensor::Level::Poor: return gfx::rgb565_rt(255, 90, 80);
        default: return gfx::rgb565_rt(190, 190, 190);
    }
}

}  // namespace

gfx::Rgb565be hue_color(int h, int v) noexcept {
    h = ((h % 1536) + 1536) % 1536;
    const int seg = h / 256, f = h % 256;
    int r = 0, g = 0, b = 0;
    switch (seg) {
        case 0: r = 255; g = f; b = 0; break;
        case 1: r = 255 - f; g = 255; b = 0; break;
        case 2: r = 0; g = 255; b = f; break;
        case 3: r = 0; g = 255 - f; b = 255; break;
        case 4: r = f; g = 0; b = 255; break;
        default: r = 255; g = 0; b = 255 - f; break;
    }
    return gfx::rgb565_rt(static_cast<std::uint8_t>(r * v / 255), static_cast<std::uint8_t>(g * v / 255),
                          static_cast<std::uint8_t>(b * v / 255));
}

void render_home(const Canvas& cv, const HomeData& d) noexcept {
    // Background: vertical gradient between two hues that rotate slowly. Alert: warm red tones.
    const int base = static_cast<int>(static_cast<std::uint64_t>(d.t_ms % kDriftPeriodMs) * 1536 / kDriftPeriodMs);
    const int h0 = d.alert ? 0 : base, h1 = d.alert ? 60 : base + 220;
    for (int y = 0; y < gfx::kScreen; ++y) {
        const int h = h0 + (h1 - h0) * y / (gfx::kScreen - 1);
        const gfx::Rgb565be c = hue_color(h, 70 + 35 * y / gfx::kScreen);
        for (int x = 0; x < gfx::kScreen; ++x) cv.px[static_cast<std::size_t>(y) * gfx::kScreen + x] = c;
    }

    char buf[24];
    if (d.time_valid) std::snprintf(buf, sizeof buf, (d.second & 1) ? "%02d:%02d" : "%02d %02d", d.hour, d.minute);
    else std::snprintf(buf, sizeof buf, "--:--");
    draw_text_centered(cv, 34, 6, kWhite, buf);

    if (d.sensor_ok) {
        std::snprintf(buf, sizeof buf, "%.1f^C", static_cast<double>(d.reading.temp_c));
        draw_text_centered(cv, 90, 4, kWhite, buf);
        std::snprintf(buf, sizeof buf, "%.0f%% %.0fHPA", static_cast<double>(d.reading.rh), static_cast<double>(d.reading.hpa));
        draw_text_centered(cv, 132, 2, kWhite, buf);
        const gfx::Rgb565be lc = level_color(d.quality.level);
        draw_text_centered(cv, 160, 3, lc, sensor::level_name(d.quality.level));
        if (d.quality.level != sensor::Level::WarmingUp) {
            std::snprintf(buf, sizeof buf, "IAQ %d", d.quality.score);
            draw_text_centered(cv, 196, 2, kWhite, buf);
        }
    } else {
        draw_text_centered(cv, 100, 3, kWhite, "NO SENSOR");
    }
}

}  // namespace screen
