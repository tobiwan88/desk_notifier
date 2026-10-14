// screen: the friendship, see bond.hpp. Pure C++.
#include "screen/core/bond.hpp"

#include <algorithm>
#include <cstdio>

namespace screen {

namespace {
constexpr const char* kActions[] = {"feed", "coffee", "party", "pet", "play", "water", "goodnight", "gift"};
constexpr int kLevelStart[] = {0, 20, 40, 60, 80};
}  // namespace

const char* action_name(Action a) noexcept { return kActions[static_cast<int>(a) % kActionCount]; }

std::optional<Action> action_from_name(std::string_view s) noexcept {
    for (int i = 0; i < kActionCount; ++i)
        if (s == kActions[i]) return static_cast<Action>(i);
    return std::nullopt;
}

const char* treat_name(int i) noexcept {
    static constexpr const char* k[] = {"acorn", "hazelnut", "berry"};
    return k[std::clamp(i, 0, kTreatCount - 1)];
}

const char* gift_name(int i) noexcept {
    static constexpr const char* k[] = {"flower", "pebble", "feather", "shell", "pinecone", "heart"};
    return k[std::clamp(i, 0, kGiftCount - 1)];
}

const char* level_name(int l) noexcept {
    static constexpr const char* k[] = {"SHY", "FRIENDLY", "FRIEND", "BEST FRIEND", "FAMILY"};
    return k[std::clamp(l, 0, kLevels - 1)];
}

void settle(Bond& b, int day) noexcept {
    if (b.last_day == 0) return;
    const int from = std::max(b.settled, b.last_day + kGraceDays);
    if (day <= from) return;
    const int floor = kLevelStart[std::clamp(b.best, 0, kLevels - 1)];
    b.score = std::max(floor, b.score - (day - from));
    b.settled = day;
}

Reaction interact(Bond& b, int day, Action a, int item) noexcept {
    settle(b, day);
    Reaction r;
    r.action = a;
    r.item = a == Action::Feed ? std::clamp(item, 0, kTreatCount - 1) : a == Action::Gift ? std::clamp(item, 0, kGiftCount - 1) : 0;
    const int before = level(b);
    if (b.today != day) { b.today = day; b.done = 0; b.treats = 0; b.gained = 0; }
    r.first_today = b.done == 0;
    r.welcome_back = b.last_day != 0 && day - b.last_day >= 2;
    if (b.last_day != day) {
        b.streak = b.last_day == day - 1 ? b.streak + 1 : 1;
        b.best_streak = std::max(b.best_streak, b.streak);
    }
    const unsigned bit = 1u << static_cast<unsigned>(a);
    int gain = r.first_today ? kFirstGain : (b.done & bit) ? 0 : kMoreGain;
    if (a == Action::Feed) {
        r.too_full = b.treats >= kFullAfter;
        ++b.treats;
        if (r.too_full) gain = 0;
    }
    if (a == Action::Gift) {
        const unsigned g = 1u << static_cast<unsigned>(r.item);
        r.new_gift = !(b.gifts & g);
        b.gifts |= g;
    }
    gain = std::clamp(gain, 0, kDayCap - b.gained);
    b.gained += gain;
    b.score = std::min(100, b.score + gain);
    b.done |= bit;
    b.last_day = day;
    b.settled = day;
    ++b.total;
    b.best = std::max(b.best, level_of(b.score));
    r.gain = gain;
    r.level = level(b);
    r.level_up = r.level > before;
    return r;
}

std::size_t bond_save(const Bond& b, char* buf, std::size_t n) noexcept {
    const int k = std::snprintf(buf, n, "b1 %d %d %d %d %d %d %d %u %d %d %u %d", b.score, b.best, b.streak, b.best_streak,
                                b.last_day, b.settled, b.today, b.done, b.treats, b.gained, b.gifts, b.total);
    return k > 0 ? static_cast<std::size_t>(k) : 0;
}

Bond bond_load(const char* s) noexcept {
    Bond b, fresh;
    if (!s || std::sscanf(s, "b1 %d %d %d %d %d %d %d %u %d %d %u %d", &b.score, &b.best, &b.streak, &b.best_streak,
                          &b.last_day, &b.settled, &b.today, &b.done, &b.treats, &b.gained, &b.gifts, &b.total) != 12)
        return fresh;
    if (b.score < 0 || b.score > 100 || b.best < 0 || b.best >= kLevels || b.streak < 0 || b.total < 0 ||
        b.done >= (1u << kActionCount) || b.gifts >= (1u << kGiftCount))
        return fresh;
    return b;
}

}  // namespace screen
