// Renders the screen pages on the host, for looking at them without a board.
// Usage: page_preview <out_dir> [scene ...]   writes <scene>.rgb (frames of 240x240 RGB888, 10 fps)
// tools/preview_pages.py turns them into round GIFs.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "screen/core/day.hpp"
#include "screen/core/draw.hpp"
#include "screen/core/fun.hpp"
#include "screen/core/pages.hpp"

namespace {

void write_frame(std::FILE* f, const std::vector<gfx::Rgb565be>& fb) {
    std::vector<unsigned char> rgb(240 * 240 * 3);
    for (std::size_t i = 0; i < fb.size(); ++i) {
        const unsigned v = static_cast<std::uint16_t>((fb[i] << 8) | (fb[i] >> 8));
        rgb[i * 3] = static_cast<unsigned char>(((v >> 11) & 31) * 255 / 31);
        rgb[i * 3 + 1] = static_cast<unsigned char>(((v >> 5) & 63) * 255 / 63);
        rgb[i * 3 + 2] = static_cast<unsigned char>((v & 31) * 255 / 31);
    }
    std::fwrite(rgb.data(), 1, rgb.size(), f);
}

// Sets the date and time (and the matching weekday) of a scene.
void at(screen::Inputs& i, int y, int m, int d, int h, int mi) {
    i.year = y; i.month = m; i.day = d; i.hour = h; i.minute = mi;
    i.weekday = screen::cal::weekday(y, m, d);
}

screen::Inputs base_inputs() {
    screen::Inputs in;
    in.time_valid = true; in.hour = 9; in.minute = 41; in.weekday = 0; in.day = 11; in.month = 10;
    in.sensor_ok = true;
    in.reading = {22.4f, 46, 1013, 90000, true};
    in.quality = {84, sensor::Level::Good};
    in.weather_ok = true;
    in.weather = {12.4f, true, 3, weather::Sky::Cloudy, 15.2f, 6.1f, 20};
    in.schedule.birthday = 202;
    std::strcpy(in.schedule.birthday_name, "ALEX");
    in.schedule.anniversary = 1023;
    return in;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: page_preview <out_dir> [scene...]\n"); return 2; }
    // `want`: start the clip where that visitor is about `at_k` into its visit; `wave`: where the squirrel waves.
    struct Scene {
        const char* name; screen::Page page; int frames; void (*setup)(screen::Inputs&);
        screen::fun::Visitor want = screen::fun::Visitor::None; float at_k = 0.2f; bool wave = false;
    };
    using V = screen::fun::Visitor;
    static const Scene kScenes[] = {
        {"clock", screen::Page::Clock, 20, [](screen::Inputs&) {}},
        {"air_very_good", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {88, sensor::Level::Good}; }},
        {"air_good", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {62, sensor::Level::Fair}; }},
        {"air_okay", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {41, sensor::Level::Stale}; }},
        {"air_bad", screen::Page::Air, 50, [](screen::Inputs& i) { i.quality = {22, sensor::Level::Poor}; i.alert = true; }},
        {"air_sleepy", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {0, sensor::Level::WarmingUp}; }},
        {"mix_cold_snow", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {88, sensor::Level::Good}; i.weather = {-3.0f, true, 73, weather::Sky::Snow, 0, -6, 70}; }},
        {"mix_hot_sunny", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {62, sensor::Level::Fair}; i.weather = {29.5f, true, 0, weather::Sky::Clear, 31, 18, 0}; }},
        {"mix_hot_okay", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {40, sensor::Level::Stale}; i.weather = {27.0f, true, 2, weather::Sky::PartlyCloudy, 29, 17, 10}; }},
        {"mix_bad_rain", screen::Page::Air, 50, [](screen::Inputs& i) { i.quality = {22, sensor::Level::Poor}; i.alert = true; i.weather = {9.1f, true, 63, weather::Sky::Rain, 11, 6, 80}; }},
        {"mix_bad_cold", screen::Page::Air, 50, [](screen::Inputs& i) { i.quality = {22, sensor::Level::Poor}; i.weather = {2.0f, true, 3, weather::Sky::Cloudy, 4, -1, 20}; }},
        {"mix_good_thunder", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {62, sensor::Level::Fair}; i.weather = {21.0f, true, 95, weather::Sky::Thunder, 26, 15, 90}; }},
        {"mix_night_sleepy", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {0, sensor::Level::WarmingUp}; i.weather = {5.0f, false, 0, weather::Sky::Clear, 12, 3, 0}; }},
        {"mix_fog", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {88, sensor::Level::Good}; i.weather = {10.0f, true, 45, weather::Sky::Fog, 12, 6, 5}; }},
        {"day_coffee", screen::Page::Day, 60, [](screen::Inputs& i) { i.weekday = 1; i.hour = 7; i.minute = 45; }},
        {"day_work", screen::Page::Day, 40, [](screen::Inputs& i) { i.weekday = 1; i.hour = 10; i.minute = 10; }},
        {"day_lunch", screen::Page::Day, 40, [](screen::Inputs& i) { i.weekday = 1; i.hour = 12; i.minute = 25; i.weather = {9.1f, true, 63, weather::Sky::Rain, 11, 6, 80}; }},
        {"day_close", screen::Page::Day, 60, [](screen::Inputs& i) { i.weekday = 5; i.hour = 18; i.minute = 5; i.weather = {14.0f, true, 2, weather::Sky::PartlyCloudy, 16, 9, 10}; }},
        {"day_read", screen::Page::Day, 90, [](screen::Inputs& i) { i.weekday = 3; i.hour = 20; i.minute = 30; i.weather = {6.0f, false, 0, weather::Sky::Clear, 12, 3, 0}; }},
        {"day_sleep", screen::Page::Day, 40, [](screen::Inputs& i) { i.weekday = 3; i.hour = 2; i.minute = 50; i.weather = {-2.0f, false, 73, weather::Sky::Snow, 0, -5, 70}; }},
        {"day_work_bad_air", screen::Page::Day, 40, [](screen::Inputs& i) { i.weekday = 2; i.hour = 15; i.minute = 0; i.quality = {22, sensor::Level::Poor}; i.weather = {21.0f, true, 95, weather::Sky::Thunder, 26, 15, 90}; }},
        {"day_weekend_coffee", screen::Page::Day, 60, [](screen::Inputs& i) { i.weekday = 6; i.hour = 9; i.minute = 0; i.weather = {19.0f, true, 0, weather::Sky::Clear, 22, 10, 0, 8}; }},
        // little surprises (random moments, the clip starts where one happens)
        {"fun_air_bird", screen::Page::Air, 90, [](screen::Inputs& i) { i.weather = {16.0f, true, 2, weather::Sky::PartlyCloudy, 18, 8, 10}; }, V::Bird, 0.08f},
        {"fun_air_butterfly", screen::Page::Air, 60, [](screen::Inputs& i) { i.weather = {19.0f, true, 0, weather::Sky::Clear, 22, 10, 0}; }, V::Butterfly, 0.35f},
        {"fun_air_star", screen::Page::Air, 30, [](screen::Inputs& i) { i.weather = {9.0f, false, 0, weather::Sky::Clear, 14, 5, 0}; }, V::ShootingStar, 0.295f},
        {"fun_air_wave", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {62, sensor::Level::Fair}; }, V::None, 0, true},
        {"fun_day_bird", screen::Page::Day, 120, [](screen::Inputs& i) { at(i, 2026, 10, 13, 10, 0); i.weather = {15.0f, true, 2, weather::Sky::PartlyCloudy, 17, 8, 10}; }, V::Bird, 0.08f},
        {"fun_day_ladybug", screen::Page::Day, 60, [](screen::Inputs& i) { at(i, 2026, 10, 13, 12, 30); i.weather = {9.0f, true, 63, weather::Sky::Rain, 11, 6, 80}; }, V::Ladybug, 0.3f},
        {"fun_day_moth", screen::Page::Day, 50, [](screen::Inputs& i) { at(i, 2026, 10, 14, 21, 0); i.weather = {9.0f, false, 3, weather::Sky::Cloudy, 12, 6, 20}; }, V::Moth, 0.4f},
        {"fun_day_star", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2026, 10, 14, 23, 30); i.weather = {7.0f, false, 0, weather::Sky::Clear, 12, 4, 0}; }, V::ShootingStar, 0.295f},
        {"fun_day_butterfly", screen::Page::Day, 60, [](screen::Inputs& i) { at(i, 2026, 10, 17, 11, 0); i.weather = {20.0f, true, 0, weather::Sky::Clear, 22, 10, 0}; }, V::Butterfly, 0.4f},
        {"fun_clock_hour", screen::Page::Clock, 120, [](screen::Inputs& i) { i.hour = 10; i.minute = 0; }},
        // situations: the indoor climate and the weather change what the squirrel does
        {"sit_party", screen::Page::Day, 60, [](screen::Inputs& i) { at(i, 2026, 12, 31, 23, 40); i.weather = {-1.0f, false, 0, weather::Sky::Clear, 2, -4, 0}; }},
        {"sit_smell", screen::Page::Day, 40, [](screen::Inputs& i) { at(i, 2026, 10, 12, 10, 0); i.air_drop = true; i.quality = {50, sensor::Level::Fair}; }},
        {"sit_storm", screen::Page::Day, 60, [](screen::Inputs& i) { at(i, 2026, 10, 14, 21, 0); i.weather = {16.0f, false, 95, weather::Sky::Thunder, 20, 12, 90}; }},
        {"sit_air_out", screen::Page::Day, 40, [](screen::Inputs& i) { at(i, 2026, 10, 12, 10, 0); i.quality = {40, sensor::Level::Stale}; i.weather = {18.0f, true, 2, weather::Sky::PartlyCloudy, 20, 9, 10}; }},
        {"sit_cool_down", screen::Page::Day, 40, [](screen::Inputs& i) { at(i, 2026, 7, 15, 20, 30); i.reading.temp_c = 27.5f; i.weather = {19.0f, false, 0, weather::Sky::Clear, 31, 17, 0}; }},
        {"sit_yawn", screen::Page::Day, 60, [](screen::Inputs& i) { at(i, 2026, 10, 12, 10, 0); i.quality = {40, sensor::Level::Stale}; i.weather = {9.0f, true, 63, weather::Sky::Rain, 11, 6, 80}; }},
        {"sit_hot_room", screen::Page::Day, 40, [](screen::Inputs& i) { at(i, 2026, 10, 12, 14, 0); i.reading.temp_c = 27.5f; }},
        {"sit_cold_room", screen::Page::Day, 60, [](screen::Inputs& i) { at(i, 2026, 10, 12, 10, 0); i.reading.temp_c = 16.5f; }},
        {"sit_humid", screen::Page::Day, 40, [](screen::Inputs& i) { at(i, 2026, 10, 12, 12, 30); i.reading.rh = 72; }},
        {"sit_dry_air", screen::Page::Day, 60, [](screen::Inputs& i) { at(i, 2026, 10, 12, 10, 0); i.reading.rh = 28; }},
        {"sit_umbrella", screen::Page::Day, 40, [](screen::Inputs& i) { at(i, 2026, 10, 13, 7, 30); i.weather = {10.0f, true, 61, weather::Sky::Cloudy, 12, 7, 80}; }},
        {"sit_cocoa", screen::Page::Day, 40, [](screen::Inputs& i) { at(i, 2026, 10, 14, 20, 30); i.weather = {8.0f, false, 63, weather::Sky::Rain, 11, 6, 85}; }},
        {"sit_watering", screen::Page::Day, 60, [](screen::Inputs& i) { at(i, 2026, 10, 17, 15, 0); i.weather = {20.0f, true, 0, weather::Sky::Clear, 22, 10, 0}; }},
        {"glass_fog", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2026, 10, 12, 10, 0); i.weather = {6.0f, true, 45, weather::Sky::Fog, 9, 3, 5}; }},
        {"glass_frost", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2026, 12, 15, 7, 30); i.weather = {-4.0f, true, 0, weather::Sky::Clear, 1, -6, 0}; }},
        {"glass_windy", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2026, 10, 12, 10, 0); i.weather = {12.0f, true, 3, weather::Sky::Cloudy, 14, 8, 20, 45}; }},
        {"glass_snowman", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2026, 12, 19, 11, 0); i.weather = {-2.0f, true, 73, weather::Sky::Snow, 0, -5, 70}; }},
        // special days
        {"occ_birthday", screen::Page::Day, 60, [](screen::Inputs& i) { at(i, 2027, 2, 2, 10, 0); i.weather = {1.0f, true, 3, weather::Sky::Cloudy, 3, -2, 20}; }},
        {"occ_anniversary", screen::Page::Day, 60, [](screen::Inputs& i) { at(i, 2026, 10, 23, 19, 30); i.weather = {9.0f, false, 0, weather::Sky::Clear, 13, 5, 0}; }},
        {"occ_christmas", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2026, 12, 25, 9, 0); i.weather = {-1.0f, true, 73, weather::Sky::Snow, 1, -3, 60}; }},
        {"occ_new_years_eve", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2026, 12, 31, 16, 0); i.weather = {1.0f, true, 3, weather::Sky::Cloudy, 3, -2, 10}; }},
        {"occ_new_year", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2027, 1, 1, 11, 0); i.weather = {2.0f, true, 3, weather::Sky::Cloudy, 4, -1, 10}; }},
        {"occ_easter", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2027, 3, 28, 10, 0); i.weather = {14.0f, true, 2, weather::Sky::PartlyCloudy, 16, 5, 10}; }},
        {"occ_nikolaus", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2026, 12, 6, 9, 0); i.weather = {0.0f, true, 3, weather::Sky::Cloudy, 2, -3, 20}; }},
        {"occ_advent", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2026, 12, 13, 19, 0); i.weather = {-1.0f, false, 0, weather::Sky::Clear, 2, -5, 0}; }},
        {"occ_st_martin", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2026, 11, 11, 18, 45); i.weather = {5.0f, false, 0, weather::Sky::Clear, 9, 2, 0}; }},
        {"occ_halloween", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2026, 10, 31, 19, 30); i.weather = {8.0f, false, 2, weather::Sky::PartlyCloudy, 12, 4, 10}; }},
        {"occ_clock_change", screen::Page::Day, 60, [](screen::Inputs& i) { at(i, 2026, 10, 25, 9, 0); }},
        {"occ_oktoberfest", screen::Page::Day, 40, [](screen::Inputs& i) { at(i, 2026, 9, 25, 12, 30); i.weather = {18.0f, true, 0, weather::Sky::Clear, 21, 9, 0}; }},
        {"occ_squirrel_day", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2027, 1, 21, 10, 0); i.weather = {-3.0f, true, 0, weather::Sky::Clear, 0, -8, 0}; }},
        {"occ_valentine", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2027, 2, 14, 9, 0); i.weather = {2.0f, true, 3, weather::Sky::Cloudy, 5, -1, 10}; }},
        {"occ_april_fool", screen::Page::Day, 40, [](screen::Inputs& i) { at(i, 2027, 4, 1, 10, 0); i.weather = {12.0f, true, 2, weather::Sky::PartlyCloudy, 15, 4, 20}; }},
        {"occ_friday_party", screen::Page::Day, 60, [](screen::Inputs& i) { at(i, 2026, 10, 16, 18, 10); i.weather = {11.0f, true, 2, weather::Sky::PartlyCloudy, 14, 6, 10}; }},
        {"occ_monday", screen::Page::Day, 30, [](screen::Inputs& i) { at(i, 2026, 10, 12, 7, 45); }},
        {"fx_air_glider", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {88, sensor::Level::Good}; i.weather = {14.0f, true, 3, weather::Sky::Cloudy, 16, 9, 20, 42}; }},
        {"fx_air_rain", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {62, sensor::Level::Fair}; i.weather = {11.0f, true, 63, weather::Sky::Rain, 12, 7, 85, 12}; }},
        {"fx_air_sunny", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {88, sensor::Level::Good}; i.weather = {19.0f, true, 0, weather::Sky::Clear, 22, 10, 0, 8}; }},
    };
    std::vector<gfx::Rgb565be> fb(240 * 240);
    bool want_tour = argc == 2;
    for (int a = 2; a < argc; ++a) want_tour |= std::strcmp(argv[a], "tour") == 0;
    if (want_tour) {  // the real rotation with fades, shortened to 3 s per page
        std::FILE* f = std::fopen((std::string(argv[1]) + "/tour.rgb").c_str(), "wb");
        if (!f) return 1;
        screen::Rotator::Config c;
        c.dwell_ms[0] = c.dwell_ms[1] = c.dwell_ms[2] = 3000;
        screen::Rotator rot(c);
        screen::Inputs in = base_inputs();
        in.weather = {6.5f, true, 73, weather::Sky::Snow, 8, 1, 60};
        for (int i = 0; i < 100; ++i) {
            in.t_ms = 100000u + static_cast<std::uint32_t>(i) * 100u;
            in.second = i / 10;
            const auto fr = rot.tick(in.t_ms, 0b111, std::nullopt);
            std::fill(fb.begin(), fb.end(), gfx::Rgb565be{0});
            screen::render_page({fb}, fr.page, in);
            screen::draw::dim({fb}, fr.brightness);
            write_frame(f, fb);
        }
        std::fclose(f);
    }
    bool want_matrix = false;
    for (int a = 2; a < argc; ++a) want_matrix |= std::strcmp(argv[a], "matrix") == 0;
    if (want_matrix) {  // one frame per combination: sky x mood x day/night x cold/normal/hot (+ no weather)
        std::FILE* f = std::fopen((std::string(argv[1]) + "/matrix.rgb").c_str(), "wb");
        if (!f) return 1;
        static const sensor::Level kLevels[] = {sensor::Level::Good, sensor::Level::Fair, sensor::Level::Stale,
                                                sensor::Level::Poor, sensor::Level::WarmingUp};
        static const float kTemps[] = {-2.0f, 15.0f, 29.0f};
        for (int sky = -1; sky <= static_cast<int>(weather::Sky::Thunder); ++sky)
            for (const sensor::Level lvl : kLevels)
                for (int day = 1; day >= 0; --day)
                    for (const float temp : kTemps) {
                        screen::Inputs in = base_inputs();
                        in.quality = {lvl == sensor::Level::Poor ? 22 : 70, lvl};
                        in.alert = lvl == sensor::Level::Poor;
                        in.weather_ok = sky >= 0;
                        in.weather = {temp, day == 1, 0, static_cast<weather::Sky>(sky < 0 ? 0 : sky), temp + 3, temp - 5, 40};
                        in.t_ms = 100300;
                        std::fill(fb.begin(), fb.end(), gfx::Rgb565be{0});
                        screen::render_page({fb}, screen::Page::Air, in);
                        write_frame(f, fb);
                    }
        std::fclose(f);
    }
    for (const Scene& s : kScenes) {
        bool want = argc == 2;
        for (int a = 2; a < argc; ++a) want |= std::strcmp(argv[a], s.name) == 0;
        if (!want) continue;
        std::FILE* f = std::fopen((std::string(argv[1]) + "/" + s.name + ".rgb").c_str(), "wb");
        if (!f) return 1;
        screen::Inputs in = base_inputs();
        s.setup(in);
        std::uint32_t t0 = 100000u;  // find the moment the clip wants to show
        for (std::uint32_t t = t0; (s.want != V::None || s.wave) && t < t0 + 36000000u; t += 100) {
            if (s.wave ? screen::fun::quirks(t, in.seed).wave > 0.02f
                       : [&] { const auto v = screen::fun::visit(t, in.seed, screen::fun::allowed_visitors(s.page, in));
                               return v.who == s.want && v.k >= s.at_k; }()) {
                t0 = t - (s.wave ? 300u : 0u);
                break;
            }
        }
        for (int i = 0; i < s.frames; ++i) {
            std::fill(fb.begin(), fb.end(), gfx::Rgb565be{0});
            in.t_ms = t0 + static_cast<std::uint32_t>(i) * 100u;
            in.second = i / 10;
            in.millis = (i % 10) * 100;
            screen::render_page({fb}, s.page, in);
            write_frame(f, fb);
        }
        std::fclose(f);
    }
    return 0;
}
