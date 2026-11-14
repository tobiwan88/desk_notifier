// screen: which mascot lives on this desk, and the small rules that come with it (the lucky cat's coat of the
// day, the add-on switches). Set once at build time (Kconfig DESK_MASCOT). Pure C++.
#pragma once

#include <cstdint>

namespace screen {

// The squirrel is the original mascot with its own three pages. The others have a home page (clock + air) and a
// day page that follows their owner's routine (screen/core/routine).
enum class Mascot : std::uint8_t { Squirrel, Bunny, Stag, Cat, Penguin };
inline constexpr int kMascotCount = 5;
[[nodiscard]] const char* mascot_name(Mascot m) noexcept;  // "squirrel", "bunny", "stag", "cat", "penguin"

// The lucky cat's coat: calico on most days, now and then gold or black. Picked from the date alone, so it stays
// the same all day and needs no state.
enum class Coat : std::uint8_t { Calico, Gold, Black };
[[nodiscard]] Coat cat_coat(int day_number) noexcept;  // day_number = cal::day_number(y, m, d)
[[nodiscard]] const char* coat_name(Coat c) noexcept;  // "calico", "gold", "black"

// The squirrel's extras, each behind its own switch (Kconfig DESK_ADDON_*). Bits of Inputs::addons.
enum Addon : unsigned {
    kAddonPetPage = 1u << 0,   // the pet web page and its reactions on screen
    kAddonBond = 1u << 1,      // hearts and coins (needs the pet page)
    kAddonShop = 1u << 2,      // the shop: hats, places, decoration, friends (needs the bond)
    kAddonVisitors = 1u << 3,  // birds, butterflies, hedgehogs ... dropping by
    kAddonPlant = 1u << 4,     // the window-sill plant that grows on good-air days
};
inline constexpr unsigned kAllAddons = kAddonPetPage | kAddonBond | kAddonShop | kAddonVisitors | kAddonPlant;

}  // namespace screen
