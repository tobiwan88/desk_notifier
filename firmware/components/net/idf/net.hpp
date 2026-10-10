// net: Wi-Fi STA, mDNS, UART/USB console (plan §3.7, §5.6).
#pragma once

#include <cstdint>
#include <expected>
#include <string>

#include "engine/core/types.hpp"

namespace net {

[[nodiscard]] std::expected<void, engine::Err> init() noexcept;          // NVS creds, else Kconfig; reconnect with backoff
[[nodiscard]] std::expected<void, engine::Err> start_mdns() noexcept;    // hostname eyes-<MAC3> (or NVS), service _eyes._tcp
[[nodiscard]] std::expected<void, engine::Err> start_console() noexcept;
bool connected() noexcept;
std::int8_t rssi() noexcept;
std::string ip() noexcept;
std::string hostname() noexcept;
std::string token() noexcept;  // API token (created on first boot)

}  // namespace net
