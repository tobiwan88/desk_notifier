// screen: the new mascots' days, the logic. Deterministic. Pure C++.
#include "screen/core/routine.hpp"

#include <algorithm>

namespace screen {

const char* mascot_name(Mascot m) noexcept {
    switch (m) {
        case Mascot::Squirrel: return "squirrel";
        case Mascot::Bunny: return "bunny";
        case Mascot::Stag: return "stag";
        case Mascot::Cat: return "cat";
        case Mascot::Penguin: return "penguin";
    }
    return "";
}

Coat cat_coat(int day_number) noexcept {
    // a small integer hash of the day, so neighbouring days do not repeat a pattern
    std::uint32_t h = static_cast<std::uint32_t>(day_number) * 0x9E3779B1u;
    h ^= h >> 15; h *= 0x85EBCA77u; h ^= h >> 13;
    switch (h % 7u) {
        case 5: return Coat::Gold;
        case 6: return Coat::Black;
        default: return Coat::Calico;  // five days in seven
    }
}

const char* coat_name(Coat c) noexcept {
    switch (c) {
        case Coat::Calico: return "calico";
        case Coat::Gold: return "gold";
        case Coat::Black: return "black";
    }
    return "";
}

namespace {

int mins(int hhmm) noexcept { return (hhmm / 100) * 60 + hhmm % 100; }

// m in [from, to) on a 24 h circle (to may be past midnight); from and to in minutes
bool within(int m, int from, int to) noexcept {
    from = (from % 1440 + 1440) % 1440;
    to = (to % 1440 + 1440) % 1440;
    return from <= to ? (m >= from && m < to) : (m >= from || m < to);
}

constexpr int H(int hh, int mm = 0) noexcept { return hh * 60 + mm; }

Act bunny(const Profile& p, int wd, int m, bool off) noexcept {
    // Friday and Saturday night out with friends, home at one
    if ((wd == 5 || wd == 6) && m >= H(21)) return Act::OutWithFriends;
    if ((wd == 6 || wd == 0) && m < H(1)) return Act::OutWithFriends;
    const int wake = mins(p.wake) + (off ? p.weekend_late_min : 0), bed = mins(p.bed);
    if (!within(m, wake, bed)) return Act::Sleep;
    if (within(m, H(19), H(19, 30))) return Act::Dinner;
    if (off) return within(m, wake, wake + 60) ? Act::Breakfast : Act::Gaming;
    const int ws = mins(p.work_start), we = mins(p.work_end);
    if (within(m, wake, wake + 30)) return Act::Alarm;
    if (within(m, wake + 30, ws)) return Act::GetReady;
    if (within(m, ws, we)) return within(m, H(12), H(12, 30)) ? Act::LunchBreak : Act::Gardening;
    if (within(m, we, we + 60)) return Act::HomeFromWork;
    return Act::Gaming;
}

Act stag(const Profile& p, int wd, int m, bool off, bool dry) noexcept {
    const bool show = (p.show_days >> (wd % 7)) & 1u;
    const int wake = mins(p.wake) + (off ? p.weekend_late_min : 0);
    const int bed = show ? std::max(mins(p.bed), mins(p.show_end)) : mins(p.bed);
    if (!within(m, wake, bed)) return Act::Sleep;
    if (show && within(m, mins(p.show_start), mins(p.show_end))) return Act::Backstage;
    if (within(m, wake, wake + 90)) return Act::Morning;
    const bool tour = off && !show;  // a longer ride on a free weekend day, lunch an hour later
    const int lunch = tour ? H(13) : H(12);
    if (within(m, lunch, lunch + 60)) return Act::Lunch;
    if (within(m, wake + 90, lunch)) return dry ? Act::BikeRide : Act::Workshop;
    if (tour && within(m, H(15), H(16))) return Act::CoffeeCake;
    if (show) {
        if (within(m, lunch + 60, H(17))) return dry ? Act::Garden : Act::Workshop;
        return Act::Dinner;  // an early dinner before the show
    }
    if (within(m, lunch + 60, H(18))) return dry ? Act::Garden : Act::Workshop;
    if (within(m, H(18), H(20))) return Act::Dinner;
    return Act::Tv;
}

Act cat(const Profile& p, int wd, int m, bool off) noexcept {
    const int wake = mins(p.wake) + (off ? p.weekend_late_min : 0), bed = mins(p.bed);
    if (!within(m, wake, bed)) return Act::Sleep;
    if (within(m, wake, wake + 30)) return Act::WakeUp;
    const int walk = wd == 0 ? 120 : 60;  // a longer walk on Sunday
    if (within(m, wake + 30, wake + 30 + walk)) return Act::DogWalk;
    if (within(m, wake + 30 + walk, wake + 90 + walk)) return Act::Breakfast;
    if (within(m, H(12), H(13))) return Act::Lunch;
    if (within(m, H(13), H(15))) return Act::Rest;
    if (within(m, H(15), H(16))) return Act::CoffeeCake;
    if (p.evening_walk && within(m, H(17), H(17, 30))) return Act::DogWalk;
    if (within(m, H(18), H(19))) return Act::Supper;
    if (within(m, H(19), bed)) return Act::Cosy;
    return Act::Errands;
}

Act penguin(const Profile& p, int wd, int m, bool off) noexcept {
    const int wake = mins(p.wake) + (off ? p.weekend_late_min : 0), bed = mins(p.bed);
    if (!within(m, wake, bed)) return Act::Sleep;
    if (within(m, wake, std::max(wake + 60, H(8)))) return Act::Morning;
    if (wd == 0 && within(m, H(10), H(11))) return Act::Church;
    if (wd == 0 && within(m, H(12), H(13))) return Act::Roast;
    if (within(m, H(8), H(11))) return Act::Gardening;
    if (within(m, H(11), H(12)) || within(m, H(14, 30), H(15))) return Act::Knitting;
    if (within(m, H(12), H(13))) return Act::Lunch;
    if (within(m, H(13), H(14, 30))) return Act::Nap;
    if (within(m, H(15), H(16))) return Act::CoffeeCake;
    if (within(m, H(16), H(18))) return Act::Birds;
    if (within(m, H(18), H(19))) return Act::Supper;
    return Act::Tv;
}

}  // namespace

Profile default_profile(Mascot m) noexcept {
    Profile p;
    switch (m) {
        case Mascot::Bunny: p.wake = 545; p.bed = 2300; p.work_start = 700; p.work_end = 1600; p.weekend_late_min = 285; break;
        case Mascot::Stag: p.wake = 700; p.bed = 2230; p.weekend_late_min = 30; break;
        case Mascot::Cat: p.wake = 630; p.bed = 2200; p.weekend_late_min = 30; break;
        case Mascot::Penguin: p.wake = 630; p.bed = 2130; p.weekend_late_min = 30; break;
        case Mascot::Squirrel: break;
    }
    return p;
}

Act routine_for(Mascot ms, const Profile& p, bool time_valid, int weekday, int hour, int minute, bool day_off,
                bool dry) noexcept {
    if (!time_valid) return Act::Sleep;
    const int m = hour * 60 + minute;
    const int wd = ((weekday % 7) + 7) % 7;
    switch (ms) {
        case Mascot::Bunny: return bunny(p, wd, m, day_off);
        case Mascot::Stag: return stag(p, wd, m, day_off, dry);
        case Mascot::Cat: return cat(p, wd, m, day_off);
        case Mascot::Penguin: return penguin(p, wd, m, day_off);
        case Mascot::Squirrel: break;
    }
    return Act::Sleep;
}

bool act_outdoors(Act a) noexcept {
    switch (a) {
        case Act::Gardening: case Act::LunchBreak: case Act::BikeRide: case Act::Garden: case Act::DogWalk:
        case Act::Birds: case Act::Church: return true;
        default: return false;
    }
}

const char* act_label(Act a) noexcept {
    switch (a) {
        case Act::Sleep: return "GOOD NIGHT";
        case Act::Alarm: return "5 MORE MIN";
        case Act::GetReady: return "OFF TO WORK";
        case Act::Gardening: return "GARDENING";
        case Act::LunchBreak: return "LUNCH BREAK";
        case Act::HomeFromWork: return "HOME!";
        case Act::Gaming: return "GAMING";
        case Act::Dinner: return "DINNER";
        case Act::OutWithFriends: return "OUT TONIGHT";
        case Act::Breakfast: return "BREAKFAST";
        case Act::Morning: return "MORNING";
        case Act::BikeRide: return "BIKE RIDE";
        case Act::Workshop: return "WORKSHOP";
        case Act::Lunch: return "LUNCH";
        case Act::Garden: return "GARDEN";
        case Act::Backstage: return "BACKSTAGE";
        case Act::Tv: return "TV";
        case Act::WakeUp: return "GOOD MORNING";
        case Act::DogWalk: return "DOG WALK";
        case Act::Errands: return "ERRANDS";
        case Act::Rest: return "REST";
        case Act::CoffeeCake: return "COFFEE TIME";
        case Act::Supper: return "SUPPER";
        case Act::Cosy: return "COSY EVENING";
        case Act::Knitting: return "KNITTING";
        case Act::Nap: return "NAP TIME";
        case Act::Birds: return "FEED BIRDS";
        case Act::Church: return "SUNDAY";
        case Act::Roast: return "SUNDAY ROAST";
    }
    return "";
}

const char* act_name(Act a) noexcept {
    switch (a) {
        case Act::Sleep: return "sleep";
        case Act::Alarm: return "alarm";
        case Act::GetReady: return "get_ready";
        case Act::Gardening: return "gardening";
        case Act::LunchBreak: return "lunch_break";
        case Act::HomeFromWork: return "home_from_work";
        case Act::Gaming: return "gaming";
        case Act::Dinner: return "dinner";
        case Act::OutWithFriends: return "out_with_friends";
        case Act::Breakfast: return "breakfast";
        case Act::Morning: return "morning";
        case Act::BikeRide: return "bike_ride";
        case Act::Workshop: return "workshop";
        case Act::Lunch: return "lunch";
        case Act::Garden: return "garden";
        case Act::Backstage: return "backstage";
        case Act::Tv: return "tv";
        case Act::WakeUp: return "wake_up";
        case Act::DogWalk: return "dog_walk";
        case Act::Errands: return "errands";
        case Act::Rest: return "rest";
        case Act::CoffeeCake: return "coffee_cake";
        case Act::Supper: return "supper";
        case Act::Cosy: return "cosy";
        case Act::Knitting: return "knitting";
        case Act::Nap: return "nap";
        case Act::Birds: return "birds";
        case Act::Church: return "church";
        case Act::Roast: return "roast";
    }
    return "";
}

}  // namespace screen
