// screen: the weather page. Animated sky for today's forecast plus the outdoor temperature. Pure C++.
#include <cmath>
#include <cstdio>

#include "screen/core/draw.hpp"
#include "screen/core/pages.hpp"
#include "screen/core/sky.hpp"
#include "screen/core/squirrel.hpp"

namespace screen {

void render_weather(const Canvas& cv, const Inputs& in) noexcept {
    const std::uint32_t t = in.t_ms;
    const auto& w = in.weather;
    const weather::Sky s = in.weather_ok ? w.sky : weather::Sky::Cloudy;
    const bool day = !in.weather_ok || w.is_day;
    sky::draw(cv, s, day, t, 150, 70, 0.8f, sky::colors(s, day, t));
    // with an umbrella the squirrel stands lower and smaller so the canopy stays clear of the place name
    const Outfit o = outfit_for(in.weather_ok, w);
    draw_squirrel(cv, o.windy ? 90 : (o.umbrella ? 76 : 74), o.umbrella ? 112 : 92, o.umbrella ? 0.5f : 0.55f,
                  in.sensor_ok ? mood_for(in.quality.level) : Mood::Sleepy, t, o);

    const auto white = draw::hex(0xFFFFFF);
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
        draw::label(cv, 214, 2, draw::hex(0xDDF0FF), buf);
    }
}

}  // namespace screen
