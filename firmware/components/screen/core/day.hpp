// screen: the squirrel's day. What the squirrel is doing follows the clock, the calendar and the measured climate,
// never chance: the same inputs always give the same scene. Pure C++.
#pragma once

#include <cstdint>

namespace screen {

struct Inputs;

// ---- the daily routine -----------------------------------------------------------------------------------------

enum class Activity : std::uint8_t { Sleep, Coffee, Work, Lunch, CloseLaptop, Read };
inline constexpr int kActivityCount = 6;

// The day plan, times as HHMM (630 = 06:30). Days off (weekends, Bavarian public holidays) start 90 minutes later
// and have no work. Personal days as MMDD (202 = 2 February), 0 = none.
struct Schedule {
    int wake = 630;   // coffee from here
    int work = 900;   // laptop open
    int lunch = 1200; // lunch for kLunchMin
    int end = 1800;   // laptop closes, then reading
    int bed = 2300;   // asleep until `wake` (may be after midnight, e.g. 30)
    int birthday = 0;
    char birthday_name[12] = "";  // shown on the birthday, upper case
    int anniversary = 0;
};
inline constexpr int kLunchMin = 60, kCloseMin = 30, kWeekendLateMin = 90, kWeekendCoffeeMin = 120;

// weekday 0 = Sunday. Without a valid time the squirrel sleeps. `day_off`: a public holiday, planned like a weekend.
[[nodiscard]] Activity activity_for(bool time_valid, int weekday, int hour, int minute, const Schedule& s = {},
                                    bool day_off = false) noexcept;
[[nodiscard]] const char* activity_label(Activity a) noexcept;  // "COFFEE TIME", "BUSY", ...
[[nodiscard]] const char* activity_name(Activity a) noexcept;   // "coffee", "work", ... (previews)

// ---- the calendar ----------------------------------------------------------------------------------------------

namespace cal {
[[nodiscard]] int weekday(int y, int m, int d) noexcept;        // 0 = Sunday
[[nodiscard]] int easter_day_of_year(int y) noexcept;           // Easter Sunday, 1 = 1 January
[[nodiscard]] int day_of_year(int y, int m, int d) noexcept;    // 1 = 1 January
[[nodiscard]] int last_sunday(int y, int m) noexcept;           // day of the month
[[nodiscard]] bool public_holiday(int y, int m, int d) noexcept;  // Bavaria (Munich): no work
[[nodiscard]] bool oktoberfest(int y, int m, int d) noexcept;
[[nodiscard]] int advent_candles(int y, int m, int d) noexcept; // 0, or 1..4 from the first Sunday of Advent to 23 Dec
[[nodiscard]] int days_until(int y, int m, int d, int mmdd) noexcept;  // 0 on the day itself, up to 365
[[nodiscard]] float moon_phase(int y, int m, int d, int hour) noexcept;  // 0 new, 0.25 first quarter, 0.5 full, 0.75 last quarter
[[nodiscard]] bool full_moon(int y, int m, int d, int hour) noexcept;    // within about a day of full
}  // namespace cal

// Special days. One at a time, the most personal wins. Advent is extra (see Scene::advent).
enum class Occasion : std::uint8_t {
    None, Birthday, Anniversary, Christmas, NewYearsEve, NewYear, Easter, Nikolaus, StMartin, Halloween,
    ClockChange, Oktoberfest, SquirrelDay, Valentine, AprilFool,
};
inline constexpr int kOccasionCount = 15;
[[nodiscard]] Occasion occasion_for(int y, int m, int d, const Schedule& s = {}) noexcept;
[[nodiscard]] const char* occasion_name(Occasion o) noexcept;  // "birthday", ... (previews, tests)

// ---- the climate and the weather -------------------------------------------------------------------------------

// What the measured air or the weather makes the squirrel do instead of (or on top of) its routine.
enum class Situation : std::uint8_t {
    None,
    Party,     // New Year's Eve: stays up with a sparkler until 00:30
    Smell,     // the air score dropped suddenly (cooking, cleaning products): holds its nose
    Storm,     // thunder in the evening or at night: hides under the blanket
    AirOut,    // stale air and pleasant outside: opens the window
    CoolDown,  // warm room, cooler outside, evening: opens the window
    Yawn,      // stale air during work: yawns over the laptop
    HotRoom,   // 26 °C or more inside: fans itself with a leaf
    ColdRoom,  // below 18 °C inside: blanket and tea
    Humid,     // humidity over 65 %: steamed-up window, a mushroom on the sill
    DryAir,    // humidity under 35 %: drinks water, mists the plant
    Umbrella,  // rain likely on a work morning: an umbrella waits by the desk
    Cocoa,     // rain or snow in the evening: cocoa while reading
    Watering,  // sunny, mild day off: waters the plants
};
inline constexpr int kSituationCount = 14;
inline constexpr float kHotRoomC = 26, kColdRoomC = 18, kHumidPct = 65, kDryPct = 35;
[[nodiscard]] const char* situation_name(Situation s) noexcept;

// Remembers the air score of the last half hour, one sample a minute, to notice a sudden drop.
class AirTrend {
public:
    static constexpr int kSamples = 30;
    static constexpr std::uint32_t kStepMs = 60000;
    static constexpr int kDrop = 15;  // score points below the half-hour best
    void add(std::uint32_t now_ms, int score, bool valid) noexcept;
    [[nodiscard]] bool sudden_drop() const noexcept;

private:
    int score_[kSamples] = {};
    int n_ = 0, head_ = 0, now_ = 0;
    bool valid_ = false, started_ = false;
    std::uint32_t last_ = 0;
};

// ---- everything together ---------------------------------------------------------------------------------------

struct Scene {
    Activity activity = Activity::Sleep;
    Situation situation = Situation::None;
    Occasion occasion = Occasion::None;
    int advent = 0;           // advent wreath candles
    bool day_off = false;     // weekend or public holiday
    bool friday_party = false;  // Friday after work
    bool monday = false;      // Monday morning coffee: the big mug
    int countdown = 0;        // 1..7 days to the next of birthday, anniversary, Christmas (0 = none)
    Occasion countdown_to = Occasion::None;
    bool howl_night = false;  // full moon (or Halloween) at night: the squirrel may howl
};
[[nodiscard]] Scene scene_for(const Inputs& in) noexcept;
// Which gags (bits of fun::Gag) fit the scene right now.
[[nodiscard]] unsigned allowed_gags(const Scene& sc, const Inputs& in) noexcept;
// Is the desk lamp on (reading in the evening)?
[[nodiscard]] bool reading_lamp(const Scene& sc, const Inputs& in) noexcept;

}  // namespace screen
