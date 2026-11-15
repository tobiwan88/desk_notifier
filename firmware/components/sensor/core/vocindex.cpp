#include "sensor/core/vocindex.hpp"

#include <algorithm>
#include <bit>
#include <climits>
#include <cmath>
#include <cstdio>

namespace sensor {

namespace {

constexpr float kLnCentre = 10.819778f;  // ln(50000): the resistance that maps to the middle of the tick range
constexpr float kTickCentre = 30000.f;
constexpr float kAhRef = 10.f;           // g/m3, only shifts the centre

std::uint32_t check_of(const VocSaved& s) noexcept {
    const auto u = static_cast<std::uint64_t>(s.unix_s);
    return 0x9e3779b9u ^ static_cast<std::uint32_t>(s.version) ^ static_cast<std::uint32_t>(u) ^ static_cast<std::uint32_t>(u >> 32) ^
           std::rotl(std::bit_cast<std::uint32_t>(s.mean), 7) ^ std::rotl(std::bit_cast<std::uint32_t>(s.std), 13);
}

}  // namespace

float absolute_humidity(float temp_c, float rh) noexcept {
    rh = std::clamp(rh, 0.0f, 100.0f);
    return 216.7f * (rh / 100.0f) * 6.112f * std::exp(17.62f * temp_c / (243.12f + temp_c)) / (273.15f + temp_c);
}

std::size_t format_saved(const VocSaved& s, char* out, std::size_t cap) noexcept {
    const int n = std::snprintf(out, cap, "v1:%d:%lld:%08x:%08x:%08x", static_cast<int>(s.version), static_cast<long long>(s.unix_s),
                                static_cast<unsigned>(std::bit_cast<std::uint32_t>(s.mean)), static_cast<unsigned>(std::bit_cast<std::uint32_t>(s.std)),
                                static_cast<unsigned>(check_of(s)));
    return n < 0 ? 0 : static_cast<std::size_t>(n);
}

bool parse_saved(const char* text, VocSaved& out) noexcept {
    if (!text) return false;
    int version = 0;
    long long unix_s = 0;
    unsigned mean_bits = 0, std_bits = 0, chk = 0;
    if (std::sscanf(text, "v1:%d:%lld:%x:%x:%x", &version, &unix_s, &mean_bits, &std_bits, &chk) != 5) return false;
    VocSaved s;
    s.version = version;
    s.unix_s = unix_s;
    s.mean = std::bit_cast<float>(static_cast<std::uint32_t>(mean_bits));
    s.std = std::bit_cast<float>(static_cast<std::uint32_t>(std_bits));
    if (check_of(s) != chk) return false;
    out = s;
    return true;
}

VocIndex::VocIndex(VocConfig cfg) noexcept : cfg_(cfg) {
    GasIndexAlgorithm_init_with_sampling_interval(&p_, GasIndexAlgorithm_ALGORITHM_TYPE_VOC, static_cast<float>(kGasPeriodMs) / 1000.0f);
}

float VocIndex::ticks(const Reading& r) const noexcept {
    if (!r.gas_valid || !(r.gas_ohm > 0)) return 0;
    const float ln_rc = std::log(r.gas_ohm) + cfg_.k_ah * (absolute_humidity(r.temp_c, r.rh) - kAhRef);
    const float t = std::round(kTickCentre + cfg_.ticks_per_ln * (ln_rc - kLnCentre));
    return std::clamp(t, 1.0f, 64999.0f);  // the library clamps further, and treats <= 0 as "no sample"
}

void VocIndex::offer(const VocSaved& s) noexcept {
    offer_ = s;
    has_offer_ = true;
    waited_ = 0;
}

void VocIndex::apply_offer(std::int64_t unix_s) noexcept {
    has_offer_ = false;
    const std::int64_t age = unix_s - offer_.unix_s;
    const bool sane = offer_.version == cfg_.version && age >= 0 && age <= kStaleSec && std::isfinite(offer_.mean) && std::isfinite(offer_.std) &&
                      offer_.mean > 0 && offer_.mean < 40000 && offer_.std > 0 && offer_.std < 10000;
    if (!sane) return;
    GasIndexAlgorithm_set_states(&p_, offer_.mean, offer_.std);
    restored_ = age <= kFreshSec ? Restore::Fresh : age <= kAgedSec ? Restore::Aged : Restore::Stale;
    last_saved_ = offer_;
    has_last_saved_ = true;
}

int VocIndex::update(const Reading& r, std::int64_t unix_s) noexcept {
    if (has_offer_) {
        if (unix_s > 0) {
            apply_offer(unix_s);
        } else if (waited_ < kClockWaitSamples) {  // no clock yet: hold the algorithm back so a state can still be restored
            ++waited_;
            return index_ = 0;
        } else {
            has_offer_ = false;  // never got the time: start learning from scratch
        }
    }
    std::int32_t idx = 0;
    GasIndexAlgorithm_process(&p_, static_cast<std::int32_t>(ticks(r)), &idx);  // 0 = invalid sample: time advances, the last value is held
    if (samples_ < INT_MAX) ++samples_;
    index_ = samples_ <= kMaskSamples ? 0 : static_cast<int>(idx);
    return index_;
}

bool VocIndex::take_save(std::int64_t unix_s, VocSaved& out) noexcept {
    if (unix_s <= 0 || samples_ < kMaskSamples) return false;
    if (restored_ == Restore::None && samples_ < kSaveAfterSamples) return false;  // Sensirion: only after ~3 h in a row
    if (has_last_saved_ && unix_s - last_saved_.unix_s < kSaveEverySec) return false;
    VocSaved s;
    s.version = cfg_.version;
    s.unix_s = unix_s;
    GasIndexAlgorithm_get_states(&p_, &s.mean, &s.std);
    if (!(s.mean > 0) || !(s.std > 0)) return false;
    if (has_last_saved_ && std::fabs(s.mean - last_saved_.mean) < 5.0f && std::fabs(s.std - last_saved_.std) < 2.0f) return false;  // nothing moved: spare the flash
    out = last_saved_ = s;
    has_last_saved_ = true;
    return true;
}

}  // namespace sensor
