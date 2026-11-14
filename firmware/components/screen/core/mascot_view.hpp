// screen: drawing the new mascots (bunny, stag, lucky cat, penguin, and the cat's small black dog), and their
// two pages: the home page (clock + air) and the day page. Pure C++.
#pragma once

#include <cstdint>

#include "screen/core/font.hpp"
#include "screen/core/mascot.hpp"
#include "screen/core/squirrel.hpp"
#include "weather/core/forecast.hpp"

namespace screen {

struct Inputs;

// How the face looks. Mood maps to one (face_for); the day scenes pick their own.
enum class Face : std::uint8_t { Open, Happy, Half, Worried, Asleep };
[[nodiscard]] Face face_for(Mood m) noexcept;

// What it wears for the weather outside: a knitted hat and scarf below kColdC, its summer thing from kHotC
// (bunny: straw hat, stag: sunglasses, cat: paper fan, penguin: ice lolly).
struct Wear {
    bool hat = false, scarf = false, summer = false;
};
[[nodiscard]] Wear wear_for(bool weather_ok, const weather::Forecast& w) noexcept;

struct MascotLook {
    Mascot who = Mascot::Bunny;
    Face face = Face::Open;
    Wear wear{};
    Coat coat = Coat::Calico;  // the cat only
    bool wave = false;         // the cat's raised paw beckons
    bool blink = true;         // open eyes blink now and then
    bool glasses = false;      // reading glasses (penguin)
    bool glasses_up = false;   // pushed up on the forehead
};

// Draws the mascot. The design is 240 units tall with the head at (120, 104) and the body at (120, 158);
// (cx, cy) is where the design point (120, 120) lands on screen, `s` the scale.
void draw_mascot(const Canvas& cv, float cx, float cy, float s, const MascotLook& l, std::uint32_t t) noexcept;

// The cat's small black dog. (x, y) is the middle of its body, `face` +1 looks right, -1 left.
enum class DogPose : std::uint8_t { Stand, Sit, Sleep };
void draw_dog(const Canvas& cv, float x, float y, float s, float face, DogPose pose, std::uint32_t t) noexcept;

// The pages of the new mascots (Inputs::mascot != Squirrel).
void render_mascot_home(const Canvas& cv, const Inputs& in) noexcept;  // clock, temperatures, air level, the mascot peeking in
void render_mascot_day(const Canvas& cv, const Inputs& in) noexcept;   // the mascot's day (screen/core/routine)

}  // namespace screen
