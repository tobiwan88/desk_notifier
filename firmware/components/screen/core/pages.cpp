#include "screen/core/pages.hpp"

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
        case Page::Weather: return "weather";
    }
    return "";
}

std::optional<Page> page_from_name(const char* s) noexcept {
    for (int i = 0; i < kPageCount; ++i)
        if (std::strcmp(s, page_name(static_cast<Page>(i))) == 0) return static_cast<Page>(i);
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
    draw::label(cv, 72, 7, white, buf);
    if (in.time_valid) {
        std::snprintf(buf, sizeof buf, "%s %d %s", kDays[in.weekday % 7], in.day, kMonths[(in.month + 11) % 12]);
        draw::label(cv, 130, 2, soft, buf);
    }
    // small summary line: inside and outside temperature
    char inside[12] = "--", outside[12] = "--";
    if (in.sensor_ok) std::snprintf(inside, sizeof inside, "%.0f^", static_cast<double>(in.reading.temp_c));
    if (in.weather_ok) std::snprintf(outside, sizeof outside, "%.0f^", static_cast<double>(in.weather.temp_c));
    std::snprintf(buf, sizeof buf, "IN %s OUT %s", inside, outside);
    draw::label(cv, 162, 2, soft, buf);
    // a tiny squirrel head peeking from the bottom edge
    if (in.sensor_ok) draw_squirrel(cv, 120, 270, 0.75f, mood_for(in.quality.level), in.t_ms, outfit_for(in.weather_ok, in.weather.temp_c));
}

void render_page(const Canvas& cv, Page p, const Inputs& in) noexcept {
    switch (p) {
        case Page::Clock: render_clock(cv, in); return;
        case Page::Weather: render_weather(cv, in); return;
        case Page::Air: {
            AirData a;
            a.sensor_ok = in.sensor_ok; a.reading = in.reading; a.quality = in.quality; a.alert = in.alert; a.t_ms = in.t_ms;
            a.weather_ok = in.weather_ok; a.weather = in.weather;
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
