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

// ---- creatures (props_critters.cpp) ------------------------------------------------------------------------
// (x, base_y) is where the feet touch the ground; `face` +1 looks/walks right, -1 left; `s` 1.0 is natural size.
void owl(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept;  // round brown owl on a branch stub, big eyes that blink, head tilts now and then; ~22 px tall at s = 1
void hedgehog(const Canvas& cv, float x, float base_y, float s, float face, std::uint32_t t) noexcept;  // walking hedgehog, spiky back, little pointed snout, tiny legs moving; ~14 px tall, 24 wide
void snail(const Canvas& cv, float x, float base_y, float s, float face, std::uint32_t t) noexcept;     // snail with a spiral shell, body stretching and contracting as it crawls, eye stalks; ~12 px tall
void bee(const Canvas& cv, float x, float y, float s, std::uint32_t t) noexcept;      // a round striped bee centred at (x, y), wings buzzing (blurred, flicker); ~10 px
void paper_plane(const Canvas& cv, float x, float y, float s, float angle) noexcept;  // white folded paper plane centred at (x, y), nose pointing along angle (rad, 0 = right); ~18 px long
void golden_acorn(const Canvas& cv, float x, float y, float s, std::uint32_t t) noexcept;  // a shining golden acorn centred at (x, y), glow halo and twinkling sparkles around it; ~16 px tall

// ---- scenery (props_scenery.cpp) ---------------------------------------------------------------------------
// A rainbow arc with 6 soft bands, centre (cx, cy) usually below the visible area, outer radius R, band width w
// each; `alpha` 0..256 for fading in and out. Only the upper half of the circle is drawn.
void rainbow(const Canvas& cv, float cx, float cy, float R, float w, int alpha) noexcept;
// Drifting things over the whole round screen for a visit, progress k 0..1 (enter at the start, gone at the end).
void falling_leaves(const Canvas& cv, float k, std::uint32_t t, std::uint32_t seed) noexcept;  // 6-8 autumn leaves tumbling down and sideways
void petals(const Canvas& cv, float k, std::uint32_t t, std::uint32_t seed) noexcept;          // 10-14 pale pink blossom petals drifting
// A tear-off wall calendar centred at (x, y), about 28 x 32 px at s = 1: red top band with two rings, white page,
// `days` (1..99) big in the middle, a little heart under it when `heart`.
void wall_calendar(const Canvas& cv, float x, float y, float s, int days, bool heart) noexcept;
// A pen spinning around its middle at (x, y), angle in radians; ~20 px long.
void pen(const Canvas& cv, float x, float y, float s, float angle) noexcept;
// A potted plant that shows how good the room's air has been: `leaves` 0..9 leaves (0 = a bare sprout),
// `droop` 0..3 (0 = upright and fresh green, 3 = hanging and yellowish), `flower` adds a blossom on top.
// Small, for the window sill: about 24 px tall at s = 1 with 9 leaves.
void memory_plant(const Canvas& cv, float x, float base_y, float s, std::uint32_t t, int leaves, int droop, bool flower) noexcept;

// ---- the friendship (props_friend.cpp) ---------------------------------------------------------------------
void fox(const Canvas& cv, float x, float base_y, float s, float face, std::uint32_t t) noexcept;  // a friendly orange fox sitting, white chest and tail tip, tail swishing, blinking; ~34 px tall at s = 1
void present(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept;  // a wrapped gift box with ribbon and bow, a little sparkle; ~16 px tall
// Gifts from the owner, i = 0 flower, 1 pebble (smooth, shiny grey-blue), 2 feather, 3 sea shell, 4 pine cone, 5 heart (red, glossy). Centred at (x, y), ~10 px at s = 1.
void gift_icon(const Canvas& cv, float x, float y, float s, int i) noexcept;
// A small wooden wall shelf, its top board centred at (x, y) and `w` wide, with the gifts whose bit is set in `gifts` standing on it in order (bit i = gift_icon i).
void gift_shelf(const Canvas& cv, float x, float y, float w, unsigned gifts) noexcept;
// Treats, i = 0 acorn, 1 hazelnut, 2 berry (a red raspberry). Centred at (x, y), ~10 px at s = 1, rotated by angle.
void treat(const Canvas& cv, float x, float y, float s, int i, float angle) noexcept;
void nightcap(const Canvas& cv, float cx, float brim_y, float width) noexcept;  // a floppy blue sleeping cap with a white brim and a pom-pom drooping to the right
void bow(const Canvas& cv, float x, float y, float s) noexcept;                 // a pink hair bow centred at (x, y), ~12 px wide

}  // namespace screen::props
