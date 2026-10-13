#include "screen/core/pages.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "screen/core/draw.hpp"
#include "screen/core/squirrel.hpp"
#include "screen/core/view.hpp"

namespace screen {

const char* page_name(Page p) noexcept {
    switch (p) {
        case Page::Clock: return "clock";
        case Page::Air: return "air";
        case Page::Day: return "day";
    }
    return "";
}

std::optional<Page> page_from_name(const char* s) noexcept {
    for (int i = 0; i < kPageCount; ++i)
        if (std::strcmp(s, page_name(static_cast<Page>(i))) == 0) return static_cast<Page>(i);
    if (std::strcmp(s, "weather") == 0) return Page::Day;
    return std::nullopt;
}

void render_clock(const Canvas& cv, const Inputs& in) noexcept {
    static constexpr const char* kDays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    static constexpr const char* kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                              "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    HomeData h;
    h.t_ms = in.t_ms;
    h.alert = in.alert;
    render_background(cv, h);

    const auto white = draw::hex(0xFFFFFF), soft = draw::hex(0xE6ECFF);
    char buf[32];
    if (in.time_valid) std::snprintf(buf, sizeof buf, (in.second & 1) ? "%02d:%02d" : "%02d %02d", in.hour, in.minute);
    else std::snprintf(buf, sizeof buf, "--:--");
    draw::label(cv, 62, 7, white, buf);
    if (in.time_valid) {
        std::snprintf(buf, sizeof buf, "%s %d %s", kDays[in.weekday % 7], in.day, kMonths[(in.month + 11) % 12]);
        draw::label(cv, 118, 2, soft, buf);
    }
    // summary lines: inside and outside temperature, then today's forecast (the weather page used to show it)
    char inside[12] = "--", outside[12] = "--";
    if (in.sensor_ok) std::snprintf(inside, sizeof inside, "%.0f^", static_cast<double>(in.reading.temp_c));
    if (in.weather_ok) std::snprintf(outside, sizeof outside, "%.0f^", static_cast<double>(in.weather.temp_c));
    std::snprintf(buf, sizeof buf, "IN %s OUT %s", inside, outside);
    draw::label(cv, 140, 2, soft, buf);
    if (in.weather_ok) {
        const auto& w = in.weather;
        if (w.precip_pct >= 20)
            std::snprintf(buf, sizeof buf, "%.0f-%.0f^ RAIN %d%%", static_cast<double>(w.min_c), static_cast<double>(w.max_c), w.precip_pct);
        else
            std::snprintf(buf, sizeof buf, "LO %.0f^ HI %.0f^", static_cast<double>(w.min_c), static_cast<double>(w.max_c));
        draw::label(cv, 162, 2, draw::hex(0xDDF0FF), buf);
    }
    // the squirrel sits at the bottom, bigger than a peek
    Outfit o = outfit_for(in.weather_ok, in.weather);
    o.umbrella = o.windy = false;  // too tall for the peeking squirrel
    fun::Quirks q = fun::quirks(in.t_ms, in.seed);
    // on the full hour it pops up and waves for ten seconds
    float pop = 0;
    if (in.time_valid && in.minute == 0 && in.second < 12) {
        const float s = static_cast<float>(in.second) + static_cast<float>(in.millis) / 1000.0f;
        pop = std::clamp(std::min(s / 0.6f, (12.0f - s) / 1.0f), 0.0f, 1.0f);
        pop = pop * pop * (3 - 2 * pop);
        q.wave = std::max(q.wave, std::clamp(std::min((s - 0.6f) / 0.4f, (11.0f - s) / 0.5f), 0.0f, 1.0f));
        q.hop = 0;
    }
    Mood mood = in.sensor_ok ? mood_for(in.quality.level) : Mood::Sleepy;
    if (pop > 0 && mood == Mood::Okay) mood = Mood::Good;  // it is always glad to say hello
    draw_squirrel(cv, 120, 248 - 11 * pop, 0.62f, mood, in.t_ms, o, q);
}

void render_page(const Canvas& cv, Page p, const Inputs& in) noexcept {
    switch (p) {
        case Page::Clock: render_clock(cv, in); return;
        case Page::Day: render_day(cv, in); return;
        case Page::Air: {
            AirData a;
            a.sensor_ok = in.sensor_ok; a.reading = in.reading; a.quality = in.quality; a.alert = in.alert; a.t_ms = in.t_ms;
            a.weather_ok = in.weather_ok; a.weather = in.weather;
            a.quirks = fun::quirks(in.t_ms, in.seed);
            a.visit = fun::visit(in.t_ms, in.seed, fun::allowed_visitors(Page::Air, in));
            a.rainbow = fun::rainbow_alpha(in);
            a.golden = fun::golden(in.t_ms, in.seed);
            a.golden_count = in.golden_count;
            if (in.time_valid) {
                a.moon_phase = cal::moon_phase(in.year, in.month, in.day, in.hour);
                const bool night = in.weather_ok ? !in.weather.is_day : (in.hour >= 19 || in.hour < 5);
                const bool halloween = occasion_for(in.year, in.month, in.day, in.schedule) == Occasion::Halloween;
                a.wolf = halloween && night;
                if (night && in.sensor_ok && (halloween || cal::full_moon(in.year, in.month, in.day, in.hour)))
                    a.quirks.howl = fun::howl(in.t_ms, in.seed);
            }
            render_air(cv, a);
            return;
        }
    }
}

// ---- rotation --------------------------------------------------------------------------------------------------

Page Rotator::following(Page p, unsigned available) const noexcept {
    for (int i = 1; i <= kPageCount; ++i) {
        const auto q = static_cast<Page>((static_cast<int>(p) + i) % kPageCount);
        if (available & (1u << static_cast<int>(q))) return q;
    }
    return p;
}

void Rotator::start_switch(Page to, std::uint32_t now) noexcept {
    if (to == cur_) { since_ = now; return; }
    to_ = to;
    fading_ = true;
    fade_start_ = now;
}

Rotator::Frame Rotator::tick(std::uint32_t now, unsigned available, std::optional<Page> pin) noexcept {
    available |= 1u << static_cast<int>(Page::Clock);
    if (!started_) { started_ = true; since_ = now; }
    if (!fading_) {
        const Request req = request_;
        request_ = Request::None;
        if (pin) {
            if (*pin != cur_) start_switch(*pin, now);
            else since_ = now;  // dwell restarts once the pin is gone
        } else if (req == Request::Show) {
            start_switch(wanted_, now);
        } else if (req == Request::Next) {
            start_switch(following(cur_, available), now);
        } else if (!(available & (1u << static_cast<int>(cur_))) ||
                   now - since_ >= cfg_.dwell_ms[static_cast<int>(cur_)]) {
            start_switch(following(cur_, available), now);
        }
    }
    if (!fading_) return {cur_, 256};
    const std::uint32_t fade = cfg_.fade_ms > 0 ? cfg_.fade_ms : 1;
    const std::uint32_t e = now - fade_start_;
    if (e < fade) return {cur_, 256 - static_cast<int>(e * 256 / fade)};
    cur_ = to_;
    if (e < 2 * fade) return {cur_, static_cast<int>((e - fade) * 256 / fade)};
    fading_ = false;
    since_ = now;
    return {cur_, 256};
}

}  // namespace screen
