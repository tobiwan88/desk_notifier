// screen: what the desk remembers, see memory.hpp. Pure C++.
#include "screen/core/memory.hpp"

#include <algorithm>
#include <cstdio>

namespace screen {

bool plant_minute(Plant& p, int day, bool valid, sensor::Level level) noexcept {
    bool closed = false;
    if (day != p.day) {
        if (p.day != 0 && p.total >= kPlantMinMinutes) {
            const bool good = p.good * 10 >= p.total * 6, bad = p.bad * 10 >= p.total * 3;
            p.grew = false;
            if (good) {
                p.grew = p.leaves < kPlantMaxLeaves;
                p.leaves = std::min(kPlantMaxLeaves, p.leaves + 1);
                p.droop = std::max(0, p.droop - 1);
                ++p.streak;
            } else if (bad) {
                p.droop = std::min(kPlantMaxDroop, p.droop + 1);
                if (p.droop == kPlantMaxDroop) p.leaves = std::max(0, p.leaves - 1);
                p.streak = 0;
            }
            p.flower = p.streak >= kPlantBloomDays;
        } else if (p.day != 0) {
            p.grew = false;  // a day without enough readings changes nothing
        }
        closed = p.day != 0;
        p.day = day;
        p.good = p.bad = p.total = 0;
    }
    if (valid && level != sensor::Level::WarmingUp) {
        ++p.total;
        p.good += level == sensor::Level::Good || level == sensor::Level::Fair;
        p.bad += level == sensor::Level::Stale || level == sensor::Level::Poor;
    }
    return closed;
}

std::size_t plant_save(const Plant& p, char* buf, std::size_t n) noexcept {
    const int k = std::snprintf(buf, n, "p1 %d %d %d %d %d %d %d %d %d", p.leaves, p.droop, p.streak, p.flower ? 1 : 0,
                                p.grew ? 1 : 0, p.day, p.good, p.bad, p.total);
    return k > 0 ? static_cast<std::size_t>(k) : 0;
}

Plant plant_load(const char* s) noexcept {
    Plant p, fresh;
    int flower = 0, grew = 0;
    if (!s || std::sscanf(s, "p1 %d %d %d %d %d %d %d %d %d", &p.leaves, &p.droop, &p.streak, &flower, &grew, &p.day,
                          &p.good, &p.bad, &p.total) != 9)
        return fresh;
    if (p.leaves < 0 || p.leaves > kPlantMaxLeaves || p.droop < 0 || p.droop > kPlantMaxDroop || p.streak < 0 ||
        p.good < 0 || p.bad < 0 || p.total < 0 || p.good + p.bad > p.total || p.total > 1440)
        return fresh;
    p.flower = flower != 0;
    p.grew = grew != 0;
    return p;
}

void RainWatch::update(std::uint32_t now, bool weather_ok, bool wet_now) noexcept {
    if (!weather_ok) return;
    if (wet_now) { seen_ = true; raining_ = true; last_wet_ = now; return; }
    raining_ = false;
}

int RainWatch::since_min(std::uint32_t now) const noexcept {
    if (!seen_ || raining_) return -1;
    const std::uint32_t m = (now - last_wet_) / 60000u;
    return m < static_cast<std::uint32_t>(kRainMemoryMin) ? static_cast<int>(m) : -1;
}

}  // namespace screen
