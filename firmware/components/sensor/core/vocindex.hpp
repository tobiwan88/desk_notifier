// sensor: VOC index from the BME688 gas resistance. Pure C++ (plus Sensirion's plain-C Gas Index algorithm).
//
// Sensirion's Gas Index algorithm (BSD-3, third_party/sensirion_gas_index) was written for the SGP40/41, whose raw
// signal is proportional to ln(resistance). We feed it a humidity-corrected, scaled ln(gas resistance) of the BME688:
//   AH    = absolute humidity (g/m3) from the SHT40 temperature and humidity
//   ln Rc = ln(R_gas) + k * (AH - 10)                       (k: ln-units per g/m3, fit per device)
//   sraw  = 30000 + S * (ln Rc - ln 50000)                  (S: ticks per ln-unit; centre is absorbed by the learning)
// The output is a relative index (100 = this room's typical air over the last ~day, 1..500), not a concentration.
// Research and the unverified constants: docs/research/iaq-without-bsec.md. k and S are starting values.
#pragma once

#include <cstddef>
#include <cstdint>

#include "sensor/core/airq.hpp"
#include "sensor/third_party/sensirion_gas_index/sensirion_gas_index_algorithm.h"

namespace sensor {

// Absolute humidity in g/m3 from temperature (C) and relative humidity (%), Magnus form.
[[nodiscard]] float absolute_humidity(float temp_c, float rh) noexcept;

struct VocConfig {
    float k_ah = 0.03f;          // ln-units per g/m3 of absolute humidity (open code: 0.03; the fit may differ a lot)
    float ticks_per_ln = 2000.f; // algorithm ticks per ln-unit of resistance (suggested range 1250..3000)
    int version = 1;             // bump when the mapping (k, S, heater profile, cadence) changes: saved state is dropped
};

// What is worth keeping across a reboot: the algorithm's learned mean and standard deviation.
struct VocSaved {
    std::int32_t version = 0;
    std::int64_t unix_s = 0;
    float mean = 0, std = 0;
};

// One line of text for NVS ("v1:<version>:<unix>:<mean bits>:<std bits>:<check>"), and its parser (false if invalid).
[[nodiscard]] std::size_t format_saved(const VocSaved& s, char* out, std::size_t cap) noexcept;
[[nodiscard]] bool parse_saved(const char* text, VocSaved& out) noexcept;

// How a saved state was used: by its age (the thresholds are guesses; no source says how stale a baseline may be).
enum class Restore : std::uint8_t { None, Fresh, Aged, Stale };  // none / <= 10 min / <= 12 h / <= 7 d

[[nodiscard]] constexpr const char* restore_name(Restore r) noexcept {
    switch (r) {
        case Restore::None: return "none";
        case Restore::Fresh: return "fresh";
        case Restore::Aged: return "aged";
        case Restore::Stale: return "stale";
    }
    return "";
}

class VocIndex {
public:
    static constexpr int kMaskSamples = 17;         // ~3 min after boot without output (the filter starts from 0)
    static constexpr int kLearningSamples = 500;    // ~1.5 h: Sensirion's fast start-up phase
    static constexpr int kClockWaitSamples = 28;    // ~5 min: wait this long for the clock when a saved state is offered
    static constexpr int kSaveAfterSamples = 1000;  // ~3 h of continuous operation before a state is worth saving
    static constexpr std::int64_t kSaveEverySec = 3600;
    static constexpr std::int64_t kFreshSec = 600, kAgedSec = 12 * 3600, kStaleSec = 7 * 86400;

    explicit VocIndex(VocConfig cfg = {}) noexcept;

    // Offers a state read from NVS. It is applied on the first update() that knows the time, or dropped when it is
    // too old, from another mapping version, or when the clock does not arrive within kClockWaitSamples.
    void offer(const VocSaved& s) noexcept;

    // One gas sample (every kGasPeriodMs). unix_s = SNTP time, or 0 when unknown.
    // Returns the index 1..500, or 0 while there is none (masked start, blackout, nothing valid yet).
    int update(const Reading& r, std::int64_t unix_s) noexcept;

    int index() const noexcept { return index_; }
    bool learning() const noexcept { return restored_ == Restore::None && samples_ < kLearningSamples; }
    Restore restored() const noexcept { return restored_; }
    int samples() const noexcept { return samples_; }
    float ticks(const Reading& r) const noexcept;  // the algorithm input for a reading (0 when invalid), for logging

    // A state to write to NVS now, or false: not enough uptime, saved less than an hour ago, or nothing moved.
    bool take_save(std::int64_t unix_s, VocSaved& out) noexcept;

private:
    void apply_offer(std::int64_t unix_s) noexcept;

    VocConfig cfg_;
    GasIndexAlgorithmParams p_{};
    int index_ = 0;
    int samples_ = 0;  // samples fed to the algorithm
    int waited_ = 0;   // samples held back while waiting for the clock
    bool has_offer_ = false;
    VocSaved offer_{};
    Restore restored_ = Restore::None;
    VocSaved last_saved_{};
    bool has_last_saved_ = false;
};

}  // namespace sensor
