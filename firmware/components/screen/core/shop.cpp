// screen: the squirrel's shop, see shop.hpp. Pure C++.
#include "screen/core/shop.hpp"

#include <algorithm>

#include "screen/core/day.hpp"
#include "screen/core/fun.hpp"
#include "screen/core/pages.hpp"

namespace screen {
namespace {

constexpr ShopItem kItems[kShopItemCount] = {
    {"tophat", "TOP HAT", Shelf::Hat, 0, 40},
    {"flowercrown", "FLOWER CROWN", Shelf::Hat, 1, 30},
    {"beret", "BERET", Shelf::Hat, 2, 25},
    {"pirate", "PIRATE HAT", Shelf::Hat, 3, 45},
    {"cowboy", "COWBOY HAT", Shelf::Hat, 4, 45},
    {"chef", "CHEF'S HAT", Shelf::Hat, 5, 35},
    {"viking", "VIKING HELMET", Shelf::Hat, 6, 55},
    {"acorncrown", "ACORN CROWN", Shelf::Hat, 7, 80},
    {"pumpkin", "PUMPKIN HAT", Shelf::Hat, 8, 50, Season::Halloween},
    {"bunnyears", "BUNNY EARS", Shelf::Hat, 9, 40, Season::Easter},
    {"tracht", "TRACHT HAT", Shelf::Hat, 10, 50, Season::Oktoberfest},
    {"glasses", "PARTY GLASSES", Shelf::Face, 0, 40, Season::NewYear},
    {"jumper", "XMAS JUMPER", Shelf::Body, 0, 60, Season::Christmas},
    {"swimring", "SWIM RING", Shelf::Body, 1, 45, Season::Summer},
    {"alps", "ALPINE MEADOW", Shelf::Place, 0, 100},
    {"beach", "BEACH", Shelf::Place, 1, 110},
    {"forest", "AUTUMN FOREST", Shelf::Place, 2, 90},
    {"blossom", "CHERRY BLOSSOM", Shelf::Place, 3, 120},
    {"skyline", "CITY LIGHTS", Shelf::Place, 4, 150},
    {"fairylights", "FAIRY LIGHTS", Shelf::Decor, 0, 40, Season::Any, Spot::Window},
    {"curtains", "CURTAINS", Shelf::Decor, 1, 30, Season::Any, Spot::Window},
    {"photo", "PHOTO", Shelf::Decor, 2, 35, Season::Any, Spot::Wall},
    {"poster", "POSTER", Shelf::Decor, 3, 30, Season::Any, Spot::Wall},
    {"aquarium", "AQUARIUM", Shelf::Decor, 4, 70, Season::Any, Spot::Desk},
    {"lavalamp", "LAVA LAMP", Shelf::Decor, 5, 50, Season::Any, Spot::Desk},
    {"record", "RECORD PLAYER", Shelf::Decor, 6, 60, Season::Any, Spot::Desk},
    {"cactus", "CACTUS", Shelf::Decor, 7, 25, Season::Any, Spot::Desk},
    {"hamster", "HAMSTER", Shelf::Friend, 0, 90},
    {"hedgehog", "HEDGEHOG", Shelf::Friend, 1, 85},
    {"nest", "BIRD NEST", Shelf::Friend, 2, 80},
    {"owl", "OWL", Shelf::Friend, 3, 100},
};

std::uint32_t hash(int day, int salt, int i) noexcept {
    return fun::mix(static_cast<std::uint32_t>(day) * 0x9E3779B1u ^ static_cast<std::uint32_t>(salt) * 0x85EBCA77u ^
                    static_cast<std::uint32_t>(i) * 0xC2B2AE3Du);
}

bool wearable_now(const ShopItem& it, int y, int m, int d) noexcept {
    return it.season == Season::Any || in_season(it.season, y, m, d);
}

// Picks one of the owned items that match, or nothing; `none` = how many "nothing" tickets are in the draw, each
// owned item has `tickets` tickets.
int pick(const Bond& b, int day, int half, int salt, int y, int m, int d, bool (*match)(const ShopItem&), int tickets, int none) {
    int pool[kShopItemCount], n = 0;
    for (int i = 0; i < kShopItemCount; ++i)
        if (owns(b, i) && match(kItems[i]) && wearable_now(kItems[i], y, m, d)) pool[n++] = i;
    if (n == 0) return -1;
    const int draw = static_cast<int>(hash(day * 2 + half, salt, 0) % static_cast<std::uint32_t>(n * tickets + none));
    return draw < n * tickets ? pool[draw / tickets] : -1;
}

}  // namespace

const ShopItem& shop_item(int i) noexcept { return kItems[std::clamp(i, 0, kShopItemCount - 1)]; }

int shop_find(std::string_view id) noexcept {
    for (int i = 0; i < kShopItemCount; ++i)
        if (id == kItems[i].id) return i;
    return -1;
}

const char* shelf_name(Shelf s) noexcept {
    switch (s) {
        case Shelf::Hat: return "hat";
        case Shelf::Face: return "face";
        case Shelf::Body: return "body";
        case Shelf::Place: return "place";
        case Shelf::Decor: return "decor";
        case Shelf::Friend: return "friend";
    }
    return "";
}

const char* season_name(Season s) noexcept {
    switch (s) {
        case Season::Any: return "";
        case Season::Halloween: return "halloween";
        case Season::Christmas: return "christmas";
        case Season::Easter: return "easter";
        case Season::Oktoberfest: return "oktoberfest";
        case Season::NewYear: return "newyear";
        case Season::Summer: return "summer";
    }
    return "";
}

bool in_season(Season s, int y, int m, int d) noexcept {
    const int md = m * 100 + d;
    switch (s) {
        case Season::Any: return true;
        case Season::Halloween: return md >= 1015 && md <= 1102;
        case Season::Christmas: return md >= 1201 && md <= 1226;
        case Season::NewYear: return md >= 1227 || md <= 106;
        case Season::Summer: return md >= 621 && md <= 831;
        case Season::Oktoberfest: return cal::oktoberfest(y, m, d);
        case Season::Easter: {
            const int doy = cal::day_of_year(y, m, d), e = cal::easter_day_of_year(y);
            return doy >= e - 14 && doy <= e + 7;
        }
    }
    return false;
}

int daily_offer(const Bond& b, int day, int y, int m, int d, int out[kOfferSize]) noexcept {
    // seasonal things first (at most two), then the rest in the order of the day's shuffle
    int seasonal[kShopItemCount], ns = 0, other[kShopItemCount], no = 0;
    for (int i = 0; i < kShopItemCount; ++i) {
        if (owns(b, i)) continue;
        if (kItems[i].season != Season::Any) { if (in_season(kItems[i].season, y, m, d)) seasonal[ns++] = i; }
        else other[no++] = i;
    }
    auto shuffle = [day](int* a, int n, int salt) {
        std::sort(a, a + n, [day, salt](int p, int q) { return hash(day, salt, p) < hash(day, salt, q); });
    };
    shuffle(seasonal, ns, 1);
    shuffle(other, no, 2);
    int n = 0;
    for (int i = 0; i < ns && n < 2; ++i) out[n++] = seasonal[i];
    for (int i = 0; i < no && n < kOfferSize; ++i) out[n++] = other[i];
    return n;
}

BuyResult buy(Bond& b, int item, int day, int y, int m, int d) noexcept {
    BuyResult r;
    r.item = item;
    if (!shop_open(b)) { r.error = BuyError::Closed; return r; }
    if (item < 0 || item >= kShopItemCount) { r.error = BuyError::NotToday; return r; }
    if (owns(b, item)) { r.error = BuyError::Owned; return r; }
    int offer[kOfferSize];
    const int n = daily_offer(b, day, y, m, d, offer);
    if (std::find(offer, offer + n, item) == offer + n) { r.error = BuyError::NotToday; return r; }
    if (b.coins < kItems[item].price) { r.error = BuyError::Coins; r.need = kItems[item].price - b.coins; return r; }
    b.coins -= kItems[item].price;
    if (item < 32) b.owned_lo |= 1u << item;
    else b.owned_hi |= 1u << (item - 32);
    b.bought = item;
    b.bought_day = day;
    return r;
}

Look look_for(const Bond& b, int day, int half, int y, int m, int d) noexcept {
    Look l;
    // seasonal clothes are worn whenever their season is on
    auto seasonal_hat = [](const ShopItem& it) { return it.shelf == Shelf::Hat && it.season != Season::Any; };
    l.hat = pick(b, day, half, 11, y, m, d, seasonal_hat, 1, 0);
    if (l.hat < 0) l.hat = pick(b, day, half, 12, y, m, d, [](const ShopItem& it) { return it.shelf == Shelf::Hat; }, 2, 1);
    l.face = pick(b, day, half, 13, y, m, d, [](const ShopItem& it) { return it.shelf == Shelf::Face; }, 1, 0);
    l.body = pick(b, day, half, 14, y, m, d, [](const ShopItem& it) { return it.shelf == Shelf::Body; }, 1, 0);
    l.place = pick(b, day, half, 15, y, m, d, [](const ShopItem& it) { return it.shelf == Shelf::Place; }, 2, 1);
    l.buddy = pick(b, day, half, 16, y, m, d, [](const ShopItem& it) { return it.shelf == Shelf::Friend; }, 2, 1);
    l.window = pick(b, day, half, 17, y, m, d, [](const ShopItem& it) { return it.spot == Spot::Window; }, 2, 1);
    l.wall = pick(b, day, half, 18, y, m, d, [](const ShopItem& it) { return it.spot == Spot::Wall; }, 2, 1);
    l.desk = pick(b, day, half, 19, y, m, d, [](const ShopItem& it) { return it.spot == Spot::Desk; }, 2, 1);
    if (b.bought >= 0 && b.bought_day == day && owns(b, b.bought)) {  // something new: it shows it off all day
        const ShopItem& it = kItems[b.bought];
        switch (it.shelf) {
            case Shelf::Hat: l.hat = b.bought; break;
            case Shelf::Face: l.face = b.bought; break;
            case Shelf::Body: l.body = b.bought; break;
            case Shelf::Place: l.place = b.bought; break;
            case Shelf::Friend: l.buddy = b.bought; break;
            case Shelf::Decor:
                if (it.spot == Spot::Window) l.window = b.bought;
                if (it.spot == Spot::Wall) l.wall = b.bought;
                if (it.spot == Spot::Desk) l.desk = b.bought;
                break;
        }
    }
    return l;
}

Look look_now(const Inputs& in) noexcept {
    if (!in.time_valid) return {};
    return look_for(in.bond, cal::day_number(in.year, in.month, in.day), in.hour >= 14 ? 1 : 0, in.year, in.month, in.day);
}

}  // namespace screen
