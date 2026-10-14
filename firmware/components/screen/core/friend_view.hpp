// screen: the squirrel reacting to a visit from the pet page (feed, coffee, party, ...), drawn on the air page.
// Only happy reactions. Pure C++.
#pragma once

#include "screen/core/squirrel.hpp"

namespace screen::friends {

// How the squirrel looks while reacting: face, clothes and the little movements.
struct Pose {
    Mood face;
    Outfit outfit;
    fun::Quirks q;
};
[[nodiscard]] Pose pose(const AirData& d, Mood mood, Outfit outfit, fun::Quirks q) noexcept;
// Behind the squirrel (garland, the plant on the hill).
void back(const Canvas& cv, const AirData& d) noexcept;
// In front of it: what the paws hold, hats, full cheeks, hearts, confetti. (cx, cy, s) as passed to draw_squirrel.
void front(const Canvas& cv, const AirData& d, const Pose& p, float cx, float cy, float s) noexcept;
// The two caption lines at the bottom.
void labels(const Canvas& cv, const AirData& d) noexcept;

}  // namespace screen::friends
