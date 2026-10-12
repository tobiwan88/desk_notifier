// screen: the squirrel's day. What the squirrel is doing follows the clock, never chance: the same time on the
// same weekday always gives the same scene. Pure C++.
#pragma once

#include <cstdint>

namespace screen {

enum class Activity : std::uint8_t { Sleep, Coffee, Work, Lunch, CloseLaptop, Read };
inline constexpr int kActivityCount = 6;

// The day plan, times as HHMM (630 = 06:30). Weekends start 90 minutes later and have no work.
struct Schedule {
    int wake = 630;   // coffee from here
    int work = 900;   // laptop open
    int lunch = 1200; // lunch for kLunchMin
    int end = 1800;   // laptop closes, then reading
    int bed = 2300;   // asleep until `wake` (may be after midnight, e.g. 30)
};
inline constexpr int kLunchMin = 60, kCloseMin = 30, kWeekendLateMin = 90, kWeekendCoffeeMin = 120;

// weekday 0 = Sunday. Without a valid time the squirrel sleeps.
[[nodiscard]] Activity activity_for(bool time_valid, int weekday, int hour, int minute, const Schedule& s = {}) noexcept;
[[nodiscard]] const char* activity_label(Activity a) noexcept;  // "COFFEE TIME", "WORKING", ...
[[nodiscard]] const char* activity_name(Activity a) noexcept;   // "coffee", "work", ... (API, previews)

}  // namespace screen
