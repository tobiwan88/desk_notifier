// clock: SNTP wall clock with a POSIX timezone.
#pragma once

#include <ctime>
#include <string>
#include <string_view>

namespace clock_sync {

void start() noexcept;                 // call after net::init(); sets TZ and starts SNTP
std::string tz() noexcept;             // the POSIX timezone in use (NVS, else Kconfig)
void set_tz(std::string_view posix_tz) noexcept;  // saves it and applies it at once
bool valid() noexcept;                 // true once the time has been set (after 2024)
bool local(std::tm& out) noexcept;     // local time; false if not yet valid

}  // namespace clock_sync
