#include <cmath>
#include <cstdint>
#include <cstring>

#include "doctest.h"
#include "sensor/core/vocindex.hpp"

using namespace sensor;

namespace {

Reading reading(float gas_ohm, float temp = 22.0f, float rh = 45.0f, bool valid = true) {
    Reading r{temp, rh, 1013.0f, gas_ohm, valid};
    r.sht40 = true;
    return r;
}

constexpr std::int64_t kT0 = 1'800'000'000;  // a plausible SNTP time

// Runs n samples at constant resistance; returns the last index.
int run(VocIndex& v, float gas, int n, std::int64_t& t) {
    int idx = 0;
    for (int i = 0; i < n; ++i) {
        idx = v.update(reading(gas), t);
        t += 11;
    }
    return idx;
}

}  // namespace

TEST_CASE("absolute humidity matches the Magnus values") {
    CHECK(absolute_humidity(22.0f, 45.0f) == doctest::Approx(8.71f).epsilon(0.01));
    CHECK(absolute_humidity(20.0f, 100.0f) == doctest::Approx(17.3f).epsilon(0.01));
    CHECK(absolute_humidity(25.0f, 0.0f) == 0.0f);
    CHECK(absolute_humidity(25.0f, 150.0f) == absolute_humidity(25.0f, 100.0f));  // clamped
}

TEST_CASE("ticks: invalid samples give 0, resistance halving moves ln-scaled ticks, humidity is compensated") {
    VocIndex v;
    CHECK(v.ticks(reading(0.0f)) == 0.0f);
    CHECK(v.ticks(reading(100000.0f, 22, 45, false)) == 0.0f);
    const float a = v.ticks(reading(100000.0f)), b = v.ticks(reading(50000.0f));
    CHECK(a - b == doctest::Approx(2000.0f * std::log(2.0f)).epsilon(0.002));
    // moister air lowers the resistance by exp(-k dAH); the compensation puts the ticks back
    const float ah1 = absolute_humidity(22, 45), ah2 = absolute_humidity(22, 70);
    const float r2 = 100000.0f * std::exp(-0.03f * (ah2 - ah1));
    CHECK(std::fabs(v.ticks(reading(r2, 22, 70)) - a) <= 1.0f);
    CHECK(v.ticks(reading(1.0f)) >= 1.0f);       // clamped into the library's range
    CHECK(v.ticks(reading(1e12f)) <= 64999.0f);
}

TEST_CASE("the index is masked at the start, settles near 100, and rises when the resistance drops") {
    VocIndex v;
    std::int64_t t = kT0;
    for (int i = 0; i < VocIndex::kMaskSamples; ++i) CHECK(v.update(reading(100000.0f), t) == 0);
    CHECK(v.learning());
    const int settled = run(v, 100000.0f, 1200, t);
    CHECK(settled >= 60);
    CHECK(settled <= 140);
    CHECK(!v.learning());
    const int polluted = run(v, 80000.0f, 5, t);  // -20 %: cooking, a solvent
    CHECK(polluted > settled + 80);
    const int clean = run(v, 130000.0f, 20, t);  // a jump up: fresh air reads as better than typical
    CHECK(clean < 100);
}

TEST_CASE("invalid samples hold the last value and keep the clock running") {
    VocIndex v;
    std::int64_t t = kT0;
    const int good = run(v, 100000.0f, 400, t);
    int idx = 0;
    for (int i = 0; i < 10; ++i) idx = v.update(reading(100000.0f, 22, 45, false), t += 11);
    CHECK(idx > 0);
    CHECK(std::abs(idx - good) < 15);
    CHECK(v.samples() == 410);
}

TEST_CASE("saved state: text round trip and corruption") {
    VocSaved s{1, kT0, 31234.5f, 87.25f};
    char buf[96];
    const std::size_t n = format_saved(s, buf, sizeof buf);
    CHECK(n > 10);
    CHECK(n < sizeof buf);
    VocSaved back;
    CHECK(parse_saved(buf, back));
    CHECK(back.version == 1);
    CHECK(back.unix_s == kT0);
    CHECK(back.mean == s.mean);
    CHECK(back.std == s.std);
    buf[n - 1] = buf[n - 1] == '0' ? '1' : '0';  // flip the check
    CHECK(!parse_saved(buf, back));
    CHECK(!parse_saved("garbage", back));
    CHECK(!parse_saved("", back));
    CHECK(!parse_saved(nullptr, back));
}

TEST_CASE("a saved state is restored by age, or dropped") {
    const auto restored_with = [](VocSaved s, std::int64_t now) {
        VocIndex v;
        v.offer(s);
        Reading r = reading(100000.0f);
        v.update(r, now);
        return v.restored();
    };
    VocSaved s{1, kT0, 30000.0f, 300.0f};
    CHECK(restored_with(s, kT0 + 60) == Restore::Fresh);
    CHECK(restored_with(s, kT0 + 3600) == Restore::Aged);
    CHECK(restored_with(s, kT0 + 24 * 3600) == Restore::Stale);
    CHECK(restored_with(s, kT0 + 8 * 86400) == Restore::None);  // too old
    CHECK(restored_with(s, kT0 - 5) == Restore::None);          // from the future: clock jumped
    VocSaved other = s;
    other.version = 2;                                          // another mapping
    CHECK(restored_with(other, kT0 + 60) == Restore::None);
    VocSaved bad = s;
    bad.std = 0.0f;
    CHECK(restored_with(bad, kT0 + 60) == Restore::None);
    bad = s;
    bad.mean = std::nanf("");
    CHECK(restored_with(bad, kT0 + 60) == Restore::None);
}

TEST_CASE("without a clock the algorithm waits, then starts from scratch") {
    VocIndex v;
    v.offer({1, kT0, 30000.0f, 300.0f});
    for (int i = 0; i < VocIndex::kClockWaitSamples; ++i) {
        CHECK(v.update(reading(100000.0f), 0) == 0);
        CHECK(v.samples() == 0);  // nothing fed yet: a late clock can still restore
    }
    v.update(reading(100000.0f), 0);
    CHECK(v.samples() == 1);
    CHECK(v.restored() == Restore::None);
    VocIndex w;
    w.offer({1, kT0, 30000.0f, 300.0f});
    w.update(reading(100000.0f), 0);
    w.update(reading(100000.0f), 0);
    w.update(reading(100000.0f), kT0 + 30);  // the clock arrives in time
    CHECK(w.restored() == Restore::Fresh);
    CHECK(!w.learning());
}

TEST_CASE("states are saved after three hours, at most hourly, and only when they moved") {
    VocIndex v;
    VocSaved out;
    std::int64_t t = kT0;
    run(v, 100000.0f, VocIndex::kSaveAfterSamples - 1, t);
    CHECK(!v.take_save(t, out));  // not enough uptime
    run(v, 100000.0f, 1, t);
    CHECK(!v.take_save(0, out));  // no clock, no timestamp
    CHECK(v.take_save(t, out));
    CHECK(out.version == 1);
    CHECK(out.unix_s == t);
    CHECK(out.mean > 0);
    CHECK(out.std > 0);
    CHECK(!v.take_save(t + 60, out));  // just saved
    run(v, 100000.0f, 330, t);         // an hour later, same air: nothing moved
    CHECK(!v.take_save(t, out));
    run(v, 60000.0f, 400, t);          // the room changed for good
    CHECK(v.take_save(t, out));
    char buf[96];
    VocSaved back;
    CHECK(format_saved(out, buf, sizeof buf) > 0);
    CHECK(parse_saved(buf, back));
}

TEST_CASE("a restored state is saveable again soon (it carries the earlier hours)") {
    VocIndex v;
    v.offer({1, kT0, 30000.0f, 300.0f});
    std::int64_t t = kT0 + 4 * 3600;
    run(v, 100000.0f, VocIndex::kMaskSamples + 5, t);
    VocSaved out;
    CHECK(v.restored() == Restore::Aged);
    CHECK(v.take_save(t, out));  // restored, past the mask, over an hour since the stored copy, and the state moved
}
