// screen: small drawings for the day page (decorations for special days, things for the indoor climate, what
// happens on the window glass). All coordinates are screen pixels (240x240, round panel). `s` scales a prop:
// 1.0 is its natural size, about 30 px tall for things that stand on the desk. `t` is the monotonic time in ms,
// for animation. Pure C++, drawn with screen/core/draw.hpp only. Keep per-pixel loops integer (no FPU on the C61).
#pragma once

#include <cstdint>

#include "screen/core/font.hpp"

namespace screen::props {

struct Win { float x0, y0, x1, y1; };  // the window opening (glass area) in screen pixels

// ---- special days (props_calendar.cpp) ----------------------------------------------------------------------
// Standing props: (x, base_y) is the middle of the bottom edge, where it stands on the desk.
void pumpkin(const Canvas& cv, float x, float base_y, float s, std::uint32_t t, bool lit) noexcept;    // carved, candle glow when lit
void advent_wreath(const Canvas& cv, float x, float base_y, float s, std::uint32_t t, int lit) noexcept;  // 0..4 candles burning
void boot(const Canvas& cv, float x, float base_y, float s) noexcept;                // Nikolaus boot, orange and nuts in it
void xmas_tree(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept;  // twinkling lights, star, a present
void cake(const Canvas& cv, float x, float base_y, float s, std::uint32_t t, bool acorn) noexcept;  // candle flame; acorn on top instead of candle when `acorn`
void pretzel(const Canvas& cv, float x, float base_y, float s) noexcept;             // Bavarian Brezn with salt
void egg_basket(const Canvas& cv, float x, float base_y, float s) noexcept;          // Easter eggs in a basket
void vase_rose(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept;  // a red rose in a small vase
// Hanging / free props
void lantern(const Canvas& cv, float x, float top_y, float s, std::uint32_t t) noexcept;  // St. Martin paper lantern on a stick, top_y = stick
void bunting(const Canvas& cv, float y, std::uint32_t t, bool hearts) noexcept;      // a garland across the top of the wall, flags or hearts
void confetti(const Canvas& cv, std::uint32_t t) noexcept;                           // confetti lying on the desk (y 172..240), a few pieces falling
void hearts_rising(const Canvas& cv, float x, float y, std::uint32_t t) noexcept;    // small hearts floating up from (x, y), fading
void fireworks(const Canvas& cv, const Win& w, std::uint32_t t) noexcept;            // bursts inside the window, never outside it
// Hats: (cx, brim_y) is the middle of the brim, `width` the brim width in px.
void party_hat(const Canvas& cv, float cx, float brim_y, float width, std::uint32_t t) noexcept;
void santa_hat(const Canvas& cv, float cx, float brim_y, float width) noexcept;
void crown(const Canvas& cv, float cx, float brim_y, float width, std::uint32_t t) noexcept;

// ---- indoor climate (props_env.cpp) ------------------------------------------------------------------------
void plant(const Canvas& cv, float x, float base_y, float s, std::uint32_t t, bool wet) noexcept;  // potted houseplant; droplets on the leaves when wet
void mister(const Canvas& cv, float x, float y, float s, std::uint32_t t) noexcept;  // spray bottle, (x, y) its middle, nozzle points left, puffs of mist
void watering_can(const Canvas& cv, float x, float y, float s, float tilt, std::uint32_t t) noexcept;  // spout to the left; tilt 0..1, water pours when > 0.5
void water_glass(const Canvas& cv, float x, float y, float s) noexcept;              // glass of water, (x, y) its middle
void tea_cup(const Canvas& cv, float x, float y, float s, std::uint32_t t) noexcept; // cup on a saucer, tea-bag tag, steam; (x, y) cup middle
void cocoa_mug(const Canvas& cv, float x, float y, float s, std::uint32_t t) noexcept;  // mug with cocoa and marshmallows, steam; (x, y) its middle
void leaf_fan(const Canvas& cv, float x, float y, float s, float angle) noexcept;    // a big green leaf held at its stem (x, y), pointing up, rotated by angle (rad)
void mushroom(const Canvas& cv, float x, float base_y, float s) noexcept;            // small mushroom (damp corner warning)
void closed_umbrella(const Canvas& cv, float x, float base_y, float s) noexcept;     // furled umbrella leaning to the right
void stink_lines(const Canvas& cv, float x, float y, std::uint32_t t) noexcept;      // three wavy green lines rising from (x, y)
void thermometer(const Canvas& cv, float x, float y, float s, float level, bool hot) noexcept;  // wall thermometer, (x, y) its middle, level 0..1, red when hot, blue when cold
void sweat_drop(const Canvas& cv, float x, float y, float s, std::uint32_t t) noexcept;  // a drop sliding down from (x, y)

// ---- the window glass (props_window.cpp) -------------------------------------------------------------------
// Drawn after the sky and before the window frame. Must stay inside the window rectangle.
void glass_rain(const Canvas& cv, const Win& w, std::uint32_t t) noexcept;           // drops on the glass, some running down
void glass_frost(const Canvas& cv, const Win& w, std::uint32_t t) noexcept;          // ice crystals growing in the corners
void glass_fog(const Canvas& cv, const Win& w, std::uint32_t t) noexcept;            // milky glass with a wiped clear peephole
void glass_steam(const Canvas& cv, const Win& w, std::uint32_t t) noexcept;          // condensation: misty lower half, droplets
void leaves_flying(const Canvas& cv, const Win& w, std::uint32_t t) noexcept;        // autumn leaves blown across
void snowman(const Canvas& cv, const Win& w, std::uint32_t t) noexcept;              // a small snowman outside, lower left of the view
// An opened window: replaces the normal frame. The sash swings into the room on the right, a curtain blows,
// a few breeze lines drift into the room. `dark` 0..256 darkens the frame for night rooms.
void window_open(const Canvas& cv, const Win& w, std::uint32_t t, int dark) noexcept;

}  // namespace screen::props
