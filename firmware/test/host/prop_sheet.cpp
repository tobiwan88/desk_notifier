// Renders every day-page prop on a plain room, at desk size and enlarged, for checking the drawings.
// Usage: prop_sheet <out_dir> [prop ...]   writes <prop>.rgb (30 frames of 240x240 RGB888, 10 fps)
// tools/prop_sheet.py turns them into build/props/sheet.png.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "screen/core/draw.hpp"
#include "screen/core/props.hpp"

namespace {

using screen::Canvas;
using screen::props::Win;
namespace draw = screen::draw;

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

void room(const Canvas& cv) {
    draw::vgradient(cv, 0xF6F0DA, 0xE8DCB6, 0, 172);
    draw::vgradient(cv, 0xD39A62, 0xD39A62, 172, 178);
    draw::vgradient(cv, 0xA8714A, 0x7A5236, 178, 240);
}

void window_sky(const Canvas& cv, const Win& w) {
    for (int y = static_cast<int>(w.y0); y < static_cast<int>(w.y1); ++y) {
        const auto c = draw::hex(draw::lerp_rgb(0x8FC6F0, 0xD8ECFA, (y - static_cast<int>(w.y0)) * 256 / static_cast<int>(w.y1 - w.y0)));
        for (int x = static_cast<int>(w.x0); x < static_cast<int>(w.x1); ++x) cv.set(x, y, c);
    }
}

void frame(const Canvas& cv, const Win& w) {
    const auto c = draw::hex(0xF7F3EC);
    draw::capsule(cv, w.x0, w.y0, w.x1, w.y0, 2.5f, c);
    draw::capsule(cv, w.x0, w.y1, w.x1, w.y1, 2.5f, c);
    draw::capsule(cv, w.x0, w.y0, w.x0, w.y1, 2.5f, c);
    draw::capsule(cv, w.x1, w.y0, w.x1, w.y1, 2.5f, c);
}

void head(const Canvas& cv, float cx, float cy, float r) {  // a stand-in squirrel head for the hats
    draw::circle(cv, cx, cy, r + 2, draw::hex(0xB0602A));
    draw::circle(cv, cx, cy, r, draw::hex(0xE0873A));
}

struct Prop {
    const char* name;
    // big = false: real size in the scene; true: enlarged for detail
    std::function<void(const Canvas&, std::uint32_t, bool)> draw;
};

}  // namespace

int main(int argc, char** argv) {
    namespace P = screen::props;
    if (argc < 2) { std::fprintf(stderr, "usage: prop_sheet <out_dir> [prop...]\n"); return 2; }
    const Win small{160, 38, 206, 94}, large{40, 40, 200, 200};
    auto stand = [](auto fn) {
        return [fn](const Canvas& cv, std::uint32_t t, bool big) { room(cv); fn(cv, 120.0f, big ? 215.0f : 172.0f, big ? 3.0f : 1.0f, t); };
    };
    auto glass = [&](auto fn) {
        return [fn, small, large](const Canvas& cv, std::uint32_t t, bool big) {
            room(cv);
            const Win w = big ? large : small;
            window_sky(cv, w);
            fn(cv, w, t);
            frame(cv, w);
        };
    };
    auto hat = [](auto fn) {
        return [fn](const Canvas& cv, std::uint32_t t, bool big) {
            room(cv);
            const float r = big ? 70 : 26;
            head(cv, 120, 140, r);
            fn(cv, 120.0f, 140 - r * 0.75f, r * 1.7f, t);
        };
    };
    const float pi = 3.14159265f;
    const Prop kProps[] = {
        {"pumpkin", stand([](const Canvas& c, float x, float y, float s, std::uint32_t t) { P::pumpkin(c, x, y, s, t, true); })},
        {"advent_wreath", stand([](const Canvas& c, float x, float y, float s, std::uint32_t t) { P::advent_wreath(c, x, y, s, t, 3); })},
        {"boot", stand([](const Canvas& c, float x, float y, float s, std::uint32_t) { P::boot(c, x, y, s); })},
        {"xmas_tree", stand([](const Canvas& c, float x, float y, float s, std::uint32_t t) { P::xmas_tree(c, x, y, s, t); })},
        {"cake", stand([](const Canvas& c, float x, float y, float s, std::uint32_t t) { P::cake(c, x, y, s, t, false); })},
        {"cake_acorn", stand([](const Canvas& c, float x, float y, float s, std::uint32_t t) { P::cake(c, x, y, s, t, true); })},
        {"pretzel", stand([](const Canvas& c, float x, float y, float s, std::uint32_t) { P::pretzel(c, x, y, s); })},
        {"egg_basket", stand([](const Canvas& c, float x, float y, float s, std::uint32_t) { P::egg_basket(c, x, y, s); })},
        {"vase_rose", stand([](const Canvas& c, float x, float y, float s, std::uint32_t t) { P::vase_rose(c, x, y, s, t); })},
        {"lantern", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::lantern(c, 120, big ? 30 : 60, big ? 3 : 1, t); }},
        {"bunting", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::bunting(c, 30, t, big); }},
        {"confetti", [](const Canvas& c, std::uint32_t t, bool) { room(c); P::confetti(c, t); }},
        {"hearts_rising", [](const Canvas& c, std::uint32_t t, bool) { room(c); P::hearts_rising(c, 120, 160, t); }},
        {"fireworks", glass([](const Canvas& c, const Win& w, std::uint32_t t) { P::fireworks(c, w, t); })},
        {"party_hat", hat([](const Canvas& c, float x, float y, float w, std::uint32_t t) { P::party_hat(c, x, y, w, t); })},
        {"santa_hat", hat([](const Canvas& c, float x, float y, float w, std::uint32_t) { P::santa_hat(c, x, y, w); })},
        {"crown", hat([](const Canvas& c, float x, float y, float w, std::uint32_t t) { P::crown(c, x, y, w, t); })},
        {"plant", stand([](const Canvas& c, float x, float y, float s, std::uint32_t t) { P::plant(c, x, y, s, t, (t / 1500) % 2 == 0); })},
        {"mister", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::mister(c, 120, 120, big ? 3 : 1, t); }},
        {"watering_can", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::watering_can(c, 130, 120, big ? 3 : 1, (std::sin(static_cast<float>(t) / 600.0f) + 1) / 2, t); }},
        {"water_glass", [](const Canvas& c, std::uint32_t, bool big) { room(c); P::water_glass(c, 120, 140, big ? 3 : 1); }},
        {"tea_cup", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::tea_cup(c, 120, 140, big ? 3 : 1, t); }},
        {"cocoa_mug", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::cocoa_mug(c, 120, 140, big ? 3 : 1, t); }},
        {"leaf_fan", [pi](const Canvas& c, std::uint32_t t, bool big) { room(c); P::leaf_fan(c, 120, big ? 210 : 150, big ? 3 : 1, std::sin(static_cast<float>(t) / 150.0f) * 0.4f - pi * 0.05f); }},
        {"mushroom", stand([](const Canvas& c, float x, float y, float s, std::uint32_t) { P::mushroom(c, x, y, s); })},
        {"closed_umbrella", stand([](const Canvas& c, float x, float y, float s, std::uint32_t) { P::closed_umbrella(c, x, y, s); })},
        {"stink_lines", [](const Canvas& c, std::uint32_t t, bool) { room(c); P::stink_lines(c, 120, 150, t); }},
        {"thermometer", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::thermometer(c, 90, 120, big ? 3 : 1, 0.85f, true); P::thermometer(c, 150, 120, big ? 3 : 1, 0.15f, false); (void)t; }},
        {"sweat_drop", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::sweat_drop(c, 120, 100, big ? 3 : 1, t); }},
        {"glass_rain", glass([](const Canvas& c, const Win& w, std::uint32_t t) { P::glass_rain(c, w, t); })},
        {"glass_frost", glass([](const Canvas& c, const Win& w, std::uint32_t t) { P::glass_frost(c, w, t); })},
        {"glass_fog", glass([](const Canvas& c, const Win& w, std::uint32_t t) { P::glass_fog(c, w, t); })},
        {"glass_steam", glass([](const Canvas& c, const Win& w, std::uint32_t t) { P::glass_steam(c, w, t); })},
        {"leaves_flying", glass([](const Canvas& c, const Win& w, std::uint32_t t) { P::leaves_flying(c, w, t); })},
        {"snowman", glass([](const Canvas& c, const Win& w, std::uint32_t t) { P::snowman(c, w, t); })},
        {"owl", glass([](const Canvas& c, const Win& w, std::uint32_t t) { const float k = (w.x1 - w.x0) / 46; P::owl(c, (w.x0 + w.x1) / 2, w.y1 - 2 * k, k, t); })},
        {"hedgehog", stand([](const Canvas& c, float x, float y, float s, std::uint32_t t) { P::hedgehog(c, x - 30 * s + static_cast<float>(t % 3000) / 3000 * 60 * s, y, s, 1, t); })},
        {"snail", stand([](const Canvas& c, float x, float y, float s, std::uint32_t t) { P::snail(c, x - 10 * s + static_cast<float>(t % 3000) / 3000 * 20 * s, y, s, -1, t); })},
        {"bee", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::bee(c, 120 + std::sin(static_cast<float>(t) / 300.0f) * 20, 120, big ? 4 : 1, t); }},
        {"paper_plane", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::paper_plane(c, 120, 120, big ? 4 : 1, -0.2f + std::sin(static_cast<float>(t) / 500.0f) * 0.2f); }},
        {"golden_acorn", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::golden_acorn(c, 120, 130, big ? 4 : 1, t); }},
        {"rainbow", [](const Canvas& c, std::uint32_t t, bool big) { draw::vgradient(c, 0x8FC6F0, 0xD8ECFA); P::rainbow(c, 60, big ? 300 : 260, big ? 240 : 200, big ? 9 : 6, static_cast<int>(128 + 128 * std::sin(static_cast<float>(t) / 800.0f))); }},
        {"falling_leaves", [](const Canvas& c, std::uint32_t t, bool) { draw::vgradient(c, 0x8FC6F0, 0xD8ECFA); P::falling_leaves(c, static_cast<float>(t % 12000) / 12000, t, 7); }},
        {"petals", [](const Canvas& c, std::uint32_t t, bool) { draw::vgradient(c, 0x8FC6F0, 0xD8ECFA); P::petals(c, static_cast<float>(t % 12000) / 12000, t, 7); }},
        {"wall_calendar", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::wall_calendar(c, 120, 110, big ? 3.5f : 1, 3 + static_cast<int>(t / 1000 % 3), big); }},
        {"pen", [](const Canvas& c, std::uint32_t t, bool big) { room(c); P::pen(c, 120, 120, big ? 4 : 1, static_cast<float>(t) / 120.0f); }},
        {"memory_plant", [](const Canvas& c, std::uint32_t t, bool big) {
             room(c);
             const int step = static_cast<int>(t / 1000 % 3);  // three states across the clip
             const float s = big ? 2.6f : 1, y = big ? 205.0f : 172.0f;
             if (step == 0) P::memory_plant(c, 120, y, s, t, 1, 0, false);
             if (step == 1) P::memory_plant(c, 120, y, s, t, 9, 0, true);
             if (step == 2) P::memory_plant(c, 120, y, s, t, 6, 3, false);
         }},
        {"window_open", [small, large](const Canvas& c, std::uint32_t t, bool big) {
             room(c);
             const Win w = big ? large : small;
             window_sky(c, w);
             P::window_open(c, w, t, 0);
         }},
    };
    std::vector<gfx::Rgb565be> fb(240 * 240);
    for (const Prop& p : kProps) {
        bool want = argc == 2;
        for (int a = 2; a < argc; ++a) want |= std::strcmp(argv[a], p.name) == 0;
        if (!want) continue;
        for (const bool big : {false, true}) {
            const std::string path = std::string(argv[1]) + "/" + p.name + (big ? "_big" : "") + ".rgb";
            std::FILE* f = std::fopen(path.c_str(), "wb");
            if (!f) return 1;
            for (int i = 0; i < 30; ++i) {
                std::fill(fb.begin(), fb.end(), gfx::Rgb565be{0});
                p.draw({fb}, 100000u + static_cast<std::uint32_t>(i) * 100u, big);
                write_frame(f, fb);
            }
            std::fclose(f);
        }
    }
    return 0;
}
