// ble: Bluetooth LE setup service (NimBLE). One GATT service: status (read, notify), config (write JSON) and a Wi-Fi
// scan (write to start, read the list). Everything needs a passkey pairing; the passkey appears on the display.
// Wire format and the browser page: setup/README.md and setup/index.html.
#pragma once

#include <expected>

#include "engine/core/types.hpp"

namespace ble {

[[nodiscard]] std::expected<void, engine::Err> start() noexcept;  // controller, host task, advertising; call after net::init()

}  // namespace ble
