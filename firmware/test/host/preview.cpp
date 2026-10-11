// Renders the screen pages on the host, for looking at them without a board.
// Usage: page_preview <out_dir> [scene ...]   writes <scene>.rgb (frames of 240x240 RGB888, 10 fps)
// tools/preview_pages.py turns them into round GIFs.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "screen/core/draw.hpp"
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

screen::Inputs base_inputs() {
    screen::Inputs in;
    in.time_valid = true; in.hour = 9; in.minute = 41; in.weekday = 0; in.day = 11; in.month = 10;
    in.sensor_ok = true;
    in.reading = {22.4f, 46, 1013, 90000, true};
    in.quality = {84, sensor::Level::Good};
    in.weather_ok = true;
    in.weather = {12.4f, true, 3, weather::Sky::Cloudy, 15.2f, 6.1f, 20};
    return in;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: page_preview <out_dir> [scene...]\n"); return 2; }
    struct Scene { const char* name; screen::Page page; int frames; void (*setup)(screen::Inputs&); };
    static const Scene kScenes[] = {
        {"clock", screen::Page::Clock, 20, [](screen::Inputs&) {}},
        {"air_very_good", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {88, sensor::Level::Good}; }},
        {"air_good", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {62, sensor::Level::Fair}; }},
        {"air_okay", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {41, sensor::Level::Stale}; }},
        {"air_bad", screen::Page::Air, 50, [](screen::Inputs& i) { i.quality = {22, sensor::Level::Poor}; i.alert = true; }},
        {"air_sleepy", screen::Page::Air, 40, [](screen::Inputs& i) { i.quality = {0, sensor::Level::WarmingUp}; }},
        {"wx_sunny", screen::Page::Weather, 40, [](screen::Inputs& i) { i.weather = {18.6f, true, 0, weather::Sky::Clear, 21, 9, 0}; }},
        {"wx_partly", screen::Page::Weather, 40, [](screen::Inputs& i) { i.weather = {14.2f, true, 2, weather::Sky::PartlyCloudy, 16, 7, 10}; }},
        {"wx_cloudy", screen::Page::Weather, 40, [](screen::Inputs&) {}},
        {"wx_rain", screen::Page::Weather, 40, [](screen::Inputs& i) { i.weather = {9.1f, true, 63, weather::Sky::Rain, 11, 6, 80}; }},
        {"wx_snow", screen::Page::Weather, 40, [](screen::Inputs& i) { i.weather = {-2.3f, true, 73, weather::Sky::Snow, 0, -5, 70}; }},
        {"wx_thunder", screen::Page::Weather, 40, [](screen::Inputs& i) { i.weather = {21.0f, true, 95, weather::Sky::Thunder, 26, 15, 90}; }},
        {"wx_fog", screen::Page::Weather, 40, [](screen::Inputs& i) { i.weather = {4.0f, true, 45, weather::Sky::Fog, 8, 2, 5}; }},
        {"wx_night", screen::Page::Weather, 40, [](screen::Inputs& i) { i.weather = {7.5f, false, 0, weather::Sky::Clear, 15, 5, 0}; }},
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
    for (const Scene& s : kScenes) {
        bool want = argc == 2;
        for (int a = 2; a < argc; ++a) want |= std::strcmp(argv[a], s.name) == 0;
        if (!want) continue;
        std::FILE* f = std::fopen((std::string(argv[1]) + "/" + s.name + ".rgb").c_str(), "wb");
        if (!f) return 1;
        screen::Inputs in = base_inputs();
        s.setup(in);
        for (int i = 0; i < s.frames; ++i) {
            std::fill(fb.begin(), fb.end(), gfx::Rgb565be{0});
            in.t_ms = 100000u + static_cast<std::uint32_t>(i) * 100u;
            in.second = i / 10;
            screen::render_page({fb}, s.page, in);
            write_frame(f, fb);
        }
        std::fclose(f);
    }
    return 0;
}
