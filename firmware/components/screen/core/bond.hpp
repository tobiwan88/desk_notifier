// screen: the friendship between the owner and the squirrel. Interactions from the pet web page (feed, coffee,
// party, ...) raise it; days away lower it only gently, and a heart level once reached is never lost, so the
// squirrel is always glad to see you. Pure C++; the render task owns it and saves it to NVS.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace screen {

enum class Action : std::uint8_t { Feed, Coffee, Party, Pet, Play, Water, GoodNight, Gift };
inline constexpr int kActionCount = 8;
[[nodiscard]] const char* action_name(Action a) noexcept;  // "feed", "coffee", ...
[[nodiscard]] std::optional<Action> action_from_name(std::string_view s) noexcept;

// What Feed and Gift can bring.
inline constexpr int kTreatCount = 3;  // acorn, hazelnut, berry
inline constexpr int kGiftCount = 6;   // flower, pebble, feather, shell, pine cone, heart
[[nodiscard]] const char* treat_name(int i) noexcept;
[[nodiscard]] const char* gift_name(int i) noexcept;

struct Bond {
    int score = 0;          // 0..100
    int best = 0;           // the highest level reached, 0..4 (one heart .. five hearts); never goes down
    int streak = 0, best_streak = 0;  // days in a row with a visit
    int last_day = 0;       // day number of the last visit (days since 1970), 0 = never
    int settled = 0;        // day up to which the gentle decay was applied
    int today = 0;          // day the counters below belong to
    unsigned done = 0;      // bits of Action done today
    int treats = 0;         // treats today
    int gained = 0;         // points today
    unsigned gifts = 0;     // bits: gifts on the shelf
    int total = 0;          // visits ever
    // squirrel coins and the shop (screen/core/shop)
    int coins = 0, coins_total = 0;
    std::uint32_t owned_lo = 0, owned_hi = 0;  // bits: shop items owned
    int bought = -1, bought_day = 0;           // the latest purchase: worn for the rest of that day
};
// Coins: a small trickle from the first visit on, the real amounts from five hearts (the shop opens).
inline constexpr int kCoinsTrickle = 1, kCoinsDaily = 3, kCoinsWeek = 10, kCoinsSpecial = 5, kCoinsGolden = 20,
                     kCoinsGoldenEarly = 5, kCoinsLeaf = 2, kCoinsFound = 1;
inline void earn(Bond& b, int coins) noexcept { if (coins > 0) { b.coins += coins; b.coins_total += coins; } }
[[nodiscard]] inline bool owns(const Bond& b, int item) noexcept {
    return item >= 0 && item < 64 && ((item < 32 ? b.owned_lo >> item : b.owned_hi >> (item - 32)) & 1u);
}
inline constexpr int kLevels = 5, kFirstGain = 8, kMoreGain = 3, kDayCap = 12, kFullAfter = 3, kGraceDays = 3;
[[nodiscard]] constexpr int level_of(int score) noexcept { return score >= 80 ? 4 : score >= 60 ? 3 : score >= 40 ? 2 : score >= 20 ? 1 : 0; }
[[nodiscard]] inline int level(const Bond& b) noexcept { return level_of(b.score) > b.best ? level_of(b.score) : b.best; }
[[nodiscard]] const char* level_name(int level) noexcept;  // "SHY", "FRIENDLY", "FRIEND", "BEST FRIEND", "FAMILY"

struct Reaction {
    Action action = Action::Pet;
    int item = 0;               // treat or gift
    int gain = 0;               // points it brought
    int level = 0;              // level after it
    bool first_today = false;
    bool too_full = false;      // a fourth treat today: round belly, no points (still a happy squirrel)
    bool welcome_back = false;  // the first visit after two or more days
    bool level_up = false;
    bool new_gift = false;      // a gift it did not have yet
    int coins = 0;              // coins it earned
    bool unbox = false;         // not a visit: a new shop item being unwrapped (item = shop item)
};

// A reaction playing on screen: the air page shows it for reaction_ms, k = progress 0..1.
struct ReactionNow {
    bool active = false;
    Reaction r{};
    float k = 0;
};
[[nodiscard]] constexpr std::uint32_t reaction_ms(Action a) noexcept { return a == Action::Party ? 20000u : 12000u; }

// A visit on day `day` (days since 1970). Always succeeds; the reaction says how it went.
// `special`: a special day (birthday, holidays): a few coins extra for the first visit, from five hearts.
Reaction interact(Bond& b, int day, Action a, int item = 0, bool special = false) noexcept;
// Applies the gentle decay up to `day`: nothing for kGraceDays after the last visit, then one point a day, never
// below the start of the best level reached.
void settle(Bond& b, int day) noexcept;

std::size_t bond_save(const Bond& b, char* buf, std::size_t n) noexcept;
[[nodiscard]] Bond bond_load(const char* s) noexcept;  // a broken text gives a new friendship

}  // namespace screen
