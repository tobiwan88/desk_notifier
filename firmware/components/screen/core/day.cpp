// screen: the squirrel's day, the logic. Picks the scene from the clock, the calendar, the indoor climate and the
// weather. Deterministic: no randomness anywhere. Pure C++.
#include "screen/core/day.hpp"

#include <algorithm>

#include "screen/core/pages.hpp"

namespace screen {

// ---- the daily routine -----------------------------------------------------------------------------------------

namespace {

int minutes(int hhmm) noexcept { return (hhmm / 100) * 60 + hhmm % 100; }

// m in [from, to) on a 24 h circle (to may be past midnight)
bool within(int m, int from, int to) noexcept {
    from = (from % 1440 + 1440) % 1440;
    to = (to % 1440 + 1440) % 1440;
    return from <= to ? (m >= from && m < to) : (m >= from || m < to);
}

int hhmm_plus(int hhmm, int add_min) noexcept {
    const int m = ((minutes(hhmm) + add_min) % 1440 + 1440) % 1440;
    return (m / 60) * 100 + m % 60;
}

}  // namespace

Activity activity_for(bool time_valid, int weekday, int hour, int minute, const Schedule& s, bool day_off) noexcept {
    if (!time_valid) return Activity::Sleep;
    const int m = hour * 60 + minute;
    const bool free = day_off || weekday == 0 || weekday == 6;
    const int wake = minutes(s.wake) + (free ? kWeekendLateMin : 0), lunch = minutes(s.lunch);
    const int end = minutes(s.end), bed = minutes(s.bed);
    if (!within(m, wake, bed)) return Activity::Sleep;
    if (within(m, lunch, lunch + kLunchMin)) return Activity::Lunch;
    if (free) return within(m, wake, wake + kWeekendCoffeeMin) ? Activity::Coffee : Activity::Read;
    if (within(m, wake, minutes(s.work))) return Activity::Coffee;
    if (within(m, minutes(s.work), end)) return Activity::Work;
    if (within(m, end, end + kCloseMin)) return Activity::CloseLaptop;
    return Activity::Read;
}

const char* activity_label(Activity a) noexcept {
    switch (a) {
        case Activity::Sleep: return "GOOD NIGHT";
        case Activity::Coffee: return "COFFEE TIME";
        case Activity::Work: return "BUSY";
        case Activity::Lunch: return "LUNCH";
        case Activity::CloseLaptop: return "WORK DONE";
        case Activity::Read: return "READING";
    }
    return "";
}

const char* activity_name(Activity a) noexcept {
    switch (a) {
        case Activity::Sleep: return "sleep";
        case Activity::Coffee: return "coffee";
        case Activity::Work: return "work";
        case Activity::Lunch: return "lunch";
        case Activity::CloseLaptop: return "close_laptop";
        case Activity::Read: return "read";
    }
    return "";
}

// ---- the calendar ----------------------------------------------------------------------------------------------

namespace cal {
namespace {

// Days since 1970-01-01 (proleptic Gregorian), after Howard Hinnant's days_from_civil.
int days(int y, int m, int d) noexcept {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = y - era * 400;
    const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

}  // namespace

int weekday(int y, int m, int d) noexcept { return ((days(y, m, d) % 7) + 7 + 4) % 7; }  // 1970-01-01 was a Thursday
int day_of_year(int y, int m, int d) noexcept { return days(y, m, d) - days(y, 1, 1) + 1; }

int easter_day_of_year(int y) noexcept {  // anonymous Gregorian algorithm
    const int a = y % 19, b = y / 100, c = y % 100, d = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3;
    const int h = (19 * a + b - d - g + 15) % 30, i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
    const int mm = (a + 11 * h + 22 * l) / 451;
    const int month = (h + l - 7 * mm + 114) / 31, day = (h + l - 7 * mm + 114) % 31 + 1;
    return day_of_year(y, month, day);
}

int last_sunday(int y, int m) noexcept {
    static constexpr int kLen[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    const int last = kLen[(m + 11) % 12] + (m == 2 && leap);
    return last - weekday(y, m, last);
}

bool public_holiday(int y, int m, int d) noexcept {
    const int md = m * 100 + d;
    for (const int fixed : {101, 106, 501, 815, 1003, 1101, 1225, 1226})
        if (md == fixed) return true;
    const int doy = day_of_year(y, m, d), e = easter_day_of_year(y);
    for (const int off : {-2, 1, 39, 50, 60})  // Good Friday, Easter Monday, Ascension, Whit Monday, Corpus Christi
        if (doy == e + off) return true;
    return false;
}

bool oktoberfest(int y, int m, int d) noexcept {
    // ends on the first Sunday of October, or on 3 October if that is later; starts 15 days before that Sunday
    const int first_sunday = 1 + (7 - weekday(y, 10, 1)) % 7;
    const int start = day_of_year(y, 10, first_sunday) - 15, end = day_of_year(y, 10, std::max(first_sunday, 3));
    const int doy = day_of_year(y, m, d);
    return doy >= start && doy <= end;
}

int advent_candles(int y, int m, int d) noexcept {
    const int fourth = 24 - weekday(y, 12, 24);  // the last Sunday before Christmas (or 24 December itself)
    const int first = day_of_year(y, 12, fourth) - 21, doy = day_of_year(y, m, d);
    if (doy < first || doy > day_of_year(y, 12, 23)) return 0;
    return std::min(4, 1 + (doy - first) / 7);
}

}  // namespace cal

Occasion occasion_for(int y, int m, int d, const Schedule& s) noexcept {
    const int md = m * 100 + d;
    if (s.birthday && md == s.birthday) return Occasion::Birthday;
    if (s.anniversary && md == s.anniversary) return Occasion::Anniversary;
    if (md >= 1224 && md <= 1226) return Occasion::Christmas;
    if (md == 1231) return Occasion::NewYearsEve;
    if (md == 101) return Occasion::NewYear;
    const int doy = cal::day_of_year(y, m, d), e = cal::easter_day_of_year(y);
    if (doy == e || doy == e + 1) return Occasion::Easter;
    if (md == 1206) return Occasion::Nikolaus;
    if (md == 1111) return Occasion::StMartin;
    if (md == 1031) return Occasion::Halloween;
    if ((m == 3 || m == 10) && d == cal::last_sunday(y, m)) return Occasion::ClockChange;
    if (cal::oktoberfest(y, m, d)) return Occasion::Oktoberfest;
    if (md == 121) return Occasion::SquirrelDay;
    if (md == 214) return Occasion::Valentine;
    if (md == 401) return Occasion::AprilFool;
    return Occasion::None;
}

const char* occasion_name(Occasion o) noexcept {
    switch (o) {
        case Occasion::None: return "none";
        case Occasion::Birthday: return "birthday";
        case Occasion::Anniversary: return "anniversary";
        case Occasion::Christmas: return "christmas";
        case Occasion::NewYearsEve: return "new_years_eve";
        case Occasion::NewYear: return "new_year";
        case Occasion::Easter: return "easter";
        case Occasion::Nikolaus: return "nikolaus";
        case Occasion::StMartin: return "st_martin";
        case Occasion::Halloween: return "halloween";
        case Occasion::ClockChange: return "clock_change";
        case Occasion::Oktoberfest: return "oktoberfest";
        case Occasion::SquirrelDay: return "squirrel_day";
        case Occasion::Valentine: return "valentine";
        case Occasion::AprilFool: return "april_fool";
    }
    return "";
}

// ---- the climate and the weather -------------------------------------------------------------------------------

const char* situation_name(Situation s) noexcept {
    switch (s) {
        case Situation::None: return "none";
        case Situation::Party: return "party";
        case Situation::Smell: return "smell";
        case Situation::Storm: return "storm";
        case Situation::AirOut: return "air_out";
        case Situation::CoolDown: return "cool_down";
        case Situation::Yawn: return "yawn";
        case Situation::HotRoom: return "hot_room";
        case Situation::ColdRoom: return "cold_room";
        case Situation::Humid: return "humid";
        case Situation::DryAir: return "dry_air";
        case Situation::Umbrella: return "umbrella";
        case Situation::Cocoa: return "cocoa";
        case Situation::Watering: return "watering";
    }
    return "";
}

void AirTrend::add(std::uint32_t now_ms, int score, bool valid) noexcept {
    if (!valid) { n_ = 0; head_ = 0; valid_ = false; started_ = false; return; }
    now_ = score;
    valid_ = true;
    if (started_ && now_ms - last_ < kStepMs) return;
    started_ = true;
    last_ = now_ms;
    score_[head_] = score;
    head_ = (head_ + 1) % kSamples;
    n_ = std::min(n_ + 1, kSamples);
}

bool AirTrend::sudden_drop() const noexcept {
    if (!valid_ || n_ < 5) return false;  // at least five minutes to compare with
    int best = 0;
    for (int i = 0; i < n_; ++i) best = std::max(best, score_[i]);
    return best - now_ >= kDrop;
}

// ---- everything together ---------------------------------------------------------------------------------------

namespace {

bool wet(weather::Sky s) noexcept {
    return s == weather::Sky::Drizzle || s == weather::Sky::Rain || s == weather::Sky::Thunder || s == weather::Sky::Snow;
}

Situation situation_for(const Inputs& in, const Scene& sc) noexcept {
    const Activity a = sc.activity;
    const int m = in.hour * 60 + in.minute;
    const auto& w = in.weather;
    if (in.time_valid && ((in.month == 12 && in.day == 31 && m >= 22 * 60) || (in.month == 1 && in.day == 1 && m < 30)))
        return Situation::Party;
    if (in.weather_ok && w.sky == weather::Sky::Thunder && (a == Activity::Sleep || a == Activity::Read)) return Situation::Storm;
    if (a == Activity::Sleep) return Situation::None;
    const bool sensor = in.sensor_ok;
    const float inside = in.reading.temp_c, rh = in.reading.rh;
    const bool stale = sensor && (in.quality.level == sensor::Level::Stale || in.quality.level == sensor::Level::Poor);
    const bool nice_out = in.weather_ok && w.temp_c >= 10 && w.temp_c <= 24 && !wet(w.sky) && w.wind_kmh < 30;
    if (sensor && in.air_drop) return Situation::Smell;
    if (stale && nice_out) return Situation::AirOut;
    if (sensor && inside >= kHotRoomC && in.weather_ok && w.temp_c <= inside - 3 && !wet(w.sky) && in.hour >= 18)
        return Situation::CoolDown;
    if (stale && a == Activity::Work) return Situation::Yawn;
    if (sensor && inside >= kHotRoomC) return Situation::HotRoom;
    if (sensor && inside < kColdRoomC) return Situation::ColdRoom;
    if (sensor && rh > kHumidPct) return Situation::Humid;
    if (sensor && rh > 0 && rh < kDryPct) return Situation::DryAir;
    if (a == Activity::Coffee && !sc.day_off && in.weather_ok && w.precip_pct >= 60) return Situation::Umbrella;
    if (a == Activity::Read && in.weather_ok && wet(w.sky) && in.hour >= 17) return Situation::Cocoa;
    if (a == Activity::Read && sc.day_off && in.hour < 18 && in.weather_ok && w.is_day && w.temp_c >= 15 &&
        (w.sky == weather::Sky::Clear || w.sky == weather::Sky::PartlyCloudy))
        return Situation::Watering;
    return Situation::None;
}

}  // namespace

Scene scene_for(const Inputs& in) noexcept {
    Scene sc;
    if (!in.time_valid) return sc;
    sc.occasion = occasion_for(in.year, in.month, in.day, in.schedule);
    sc.advent = cal::advent_candles(in.year, in.month, in.day);
    sc.day_off = in.weekday == 0 || in.weekday == 6 || cal::public_holiday(in.year, in.month, in.day);
    Schedule s = in.schedule;
    if (sc.occasion == Occasion::NewYear) s.wake = hhmm_plus(s.wake, 90);  // sleeps in even longer after the party
    sc.activity = activity_for(true, in.weekday, in.hour, in.minute, s, sc.day_off);
    sc.situation = situation_for(in, sc);
    if (sc.situation == Situation::Party) sc.activity = Activity::Read;  // stays up
    sc.friday_party = in.weekday == 5 && !sc.day_off && (sc.activity == Activity::CloseLaptop || sc.activity == Activity::Read);
    sc.monday = in.weekday == 1 && !sc.day_off && sc.activity == Activity::Coffee;
    return sc;
}

}  // namespace screen
