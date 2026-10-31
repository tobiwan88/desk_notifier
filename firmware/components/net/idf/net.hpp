// net: Wi-Fi STA, mDNS, UART/USB console (plan §3.7, §5.6).
#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "engine/core/types.hpp"

namespace net {

[[nodiscard]] std::expected<void, engine::Err> init() noexcept;          // NVS creds, else Kconfig; reconnect with backoff
[[nodiscard]] std::expected<void, engine::Err> start_mdns() noexcept;    // hostname eyes-<MAC3> (or NVS), service _eyes._tcp
[[nodiscard]] std::expected<void, engine::Err> start_console() noexcept;
bool apply_wifi(std::string_view ssid, std::string_view pass) noexcept;  // saves and joins that network now
std::string wifi_ssid() noexcept;  // the configured network (NVS, else Kconfig)
bool connected() noexcept;
bool has_credentials() noexcept;  // Wi-Fi configured (NVS or Kconfig)
std::int8_t rssi() noexcept;
std::string ip() noexcept;
std::string hostname() noexcept;
bool open_access() noexcept;   // true: no token required (NVS "open" overrides Kconfig)
std::string token() noexcept;  // API token (created on first boot)

}  // namespace net
