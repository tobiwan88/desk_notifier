// screen: the day of the new mascots' owners. Each mascot follows one person's week (Mon-Fri and Sat-Sun differ),
// set by a few times in Kconfig. Like the squirrel's day: the clock decides, never chance. Pure C++.
#pragma once

#include <cstdint>

#include "screen/core/mascot.hpp"

namespace screen {

// Everything any of the new mascots does. Not every mascot does everything.
enum class Act : std::uint8_t {
    Sleep,
    Alarm,         // bunny: the alarm rings, paw on the snooze button
    GetReady,      // bunny: lunchbox, work boots
    Gardening,     // bunny at work / penguin in her garden in the morning
    LunchBreak,    // bunny: sandwich on an upturned bucket
    HomeFromWork,  // bunny: muddy boots by the door
    Gaming,        // bunny: controller and headset on the sofa
    Dinner,
    OutWithFriends,  // bunny: Friday and Saturday night, the burrow is empty
    Breakfast,     // bunny's late weekend breakfast, the cat's breakfast (a treat for the dog)
    Morning,       // stag and penguin: coffee and the newspaper
    BikeRide,      // stag on dry mornings
    Workshop,      // stag when it rains
    Lunch,
    Garden,        // stag in the afternoon
    Backstage,     // stag on show nights: leads the technical crew at the theatre
    Tv,
    WakeUp,        // cat: the dog jumps on the bed
    DogWalk,       // cat
    Errands,       // cat: housework or shopping
    Rest,          // cat: armchair, the dog curled up beside her
    CoffeeCake,
    Supper,
    Cosy,          // cat: reading in the evening, the dog asleep on her feet
    Knitting,      // penguin
    Nap,           // penguin: armchair, glasses pushed up
    Birds,         // penguin feeds the birds in the afternoon
    Church,        // penguin on Sunday morning
    Roast,         // penguin's Sunday roast
};
inline constexpr int kActCount = 29;
[[nodiscard]] const char* act_label(Act a) noexcept;  // "GARDENING", "BACKSTAGE", ...
[[nodiscard]] const char* act_name(Act a) noexcept;   // "gardening", "backstage", ... (previews, tests)

// The owner's times as HHMM (630 = 06:30). Defaults per mascot from default_profile(); Kconfig overrides them.
struct Profile {
    int wake = 630;
    int bed = 2230;
    int work_start = 700, work_end = 1600;  // bunny's work day
    int weekend_late_min = 30;               // how much later Sat, Sun and public holidays start
    unsigned show_days = 0x70;               // stag: theatre nights, bit 0 = Sunday .. bit 6 = Saturday (Thu-Sat)
    int show_start = 1730, show_end = 2300;  // stag
    bool evening_walk = true;                // cat: a second, shorter dog walk at 17:00
};
[[nodiscard]] Profile default_profile(Mascot m) noexcept;

// What the mascot does now. weekday 0 = Sunday; `day_off` = weekend or public holiday; `dry` = no rain or snow
// (the stag cycles only when dry). Without a valid time it sleeps. The squirrel has its own day (activity_for).
[[nodiscard]] Act routine_for(Mascot m, const Profile& p, bool time_valid, int weekday, int hour, int minute,
                              bool day_off, bool dry = true) noexcept;

// Is the mascot out of the house (no window, no indoor climate reactions)?
[[nodiscard]] bool act_outdoors(Act a) noexcept;

}  // namespace screen
