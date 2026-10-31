// ble: the data side of the BLE setup service. A JSON config in, a JSON status and Wi-Fi list out, the pairing
// passkey and the lock-out after failed pairings. Pure C++ (the NimBLE glue is in idf/).
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ble {

// {"ssid":"..","pass":"..","tz":"CET-1CEST,M3.5.0,M10.5.0/3","hostname":"desk","reboot":true}; every key is optional.
struct Config {
    std::optional<std::string> ssid, pass, tz, hostname;
    bool reboot = false;
};

enum class ParseError : std::uint8_t { None, Json, Empty, Ssid, Pass, Tz, Hostname };
[[nodiscard]] const char* error_name(ParseError e) noexcept;  // "ok", "bad json", "nothing to set", "bad ssid", ...

// Validates as it parses: ssid 1..32 bytes, pass empty (open network) or 8..63, tz a POSIX TZ string up to 63
// characters, hostname a lower-case DNS label up to 32 (upper case is folded). `out` is only meaningful on None.
[[nodiscard]] ParseError parse_config(std::string_view json, Config& out) noexcept;

// What the status characteristic shows.
struct Status {
    std::string_view version, wifi /* none|connecting|connected */, ssid, ip, hostname, tz, result;
    bool time_valid = false;
    int rssi = 0;
};
[[nodiscard]] std::string status_json(const Status& s);

struct Network {
    std::string ssid;
    int rssi = 0;
    bool secure = false;
};
// The strongest networks first, one entry per name, hidden ones skipped, cut so the JSON stays below 480 bytes.
[[nodiscard]] std::string scan_json(std::span<const Network> found);

// Passkey for the display-only pairing: always six digits.
[[nodiscard]] constexpr std::uint32_t passkey_from(std::uint32_t random) noexcept { return 100000u + random % 900000u; }

// Stops advertising for a while after too many failed pairings (the code is 6 digits and a new one is made per try).
class PairGuard {
public:
    static constexpr int kMaxFails = 5;
    static constexpr std::uint32_t kLockMs = 5u * 60u * 1000u;

    void failed(std::uint32_t now_ms) noexcept;
    void succeeded() noexcept { fails_ = 0; locked_ = false; }
    [[nodiscard]] bool locked(std::uint32_t now_ms) const noexcept;

private:
    int fails_ = 0;
    bool locked_ = false;
    std::uint32_t until_ = 0;
};

}  // namespace ble
