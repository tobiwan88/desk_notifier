// screen: the squirrel's shop. Coins (screen/core/bond) buy hats, places, home decoration and friends; seasonal
// things only in their season. Six items a day, mixed by the date. The squirrel dresses itself: each morning and
// afternoon it picks from what it owns (the same pick for the same half day), and wears a new thing the day it
// arrives. Pure C++.
#pragma once

#include <cstdint>
#include <string_view>

#include "screen/core/bond.hpp"

namespace screen {

struct Inputs;

enum class Shelf : std::uint8_t { Hat, Face, Body, Place, Decor, Friend };
enum class Season : std::uint8_t { Any, Halloween, Christmas, Easter, Oktoberfest, NewYear, Summer };
enum class Spot : std::uint8_t { None, Window, Wall, Desk };  // where decoration goes in the room

struct ShopItem {
    const char* id;    // "tophat" (API, page)
    const char* name;  // "TOP HAT" (screen)
    Shelf shelf;
    int art;           // which drawing (props::ShopHat, ShopPlace, ... or an index within the shelf)
    int price;
    Season season = Season::Any;
    Spot spot = Spot::None;
};
inline constexpr int kShopItemCount = 31, kOfferSize = 6;
[[nodiscard]] const ShopItem& shop_item(int i) noexcept;
[[nodiscard]] int shop_find(std::string_view id) noexcept;  // -1 if unknown
[[nodiscard]] const char* shelf_name(Shelf s) noexcept;      // "hat", "place", ...
[[nodiscard]] const char* season_name(Season s) noexcept;    // "halloween", ... ("" for Any)
[[nodiscard]] bool in_season(Season s, int y, int m, int d) noexcept;
[[nodiscard]] inline bool shop_open(const Bond& b) noexcept { return level(b) >= kLevels - 1; }

// Today's offer: up to six items not owned yet (seasonal ones only in season, and those first). Returns the count.
int daily_offer(const Bond& b, int day, int y, int m, int d, int out[kOfferSize]) noexcept;

enum class BuyError : std::uint8_t { None, Closed, NotToday, Owned, Coins };
struct BuyResult {
    BuyError error = BuyError::None;
    int item = -1;
    int need = 0;  // coins still missing
};
BuyResult buy(Bond& b, int item, int day, int y, int m, int d) noexcept;

// What the squirrel wears and has around it (shop item indices, -1 = nothing).
struct Look {
    int hat = -1, face = -1, body = -1, place = -1, buddy = -1;
    int window = -1, wall = -1, desk = -1;
};
[[nodiscard]] Look look_for(const Bond& b, int day, int half, int y, int m, int d) noexcept;  // half 0 = morning, 1 = afternoon
[[nodiscard]] Look look_now(const Inputs& in) noexcept;

}  // namespace screen
