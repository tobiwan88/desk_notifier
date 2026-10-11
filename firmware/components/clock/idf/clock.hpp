// clock: SNTP wall clock with a POSIX timezone.
#pragma once

#include <ctime>

namespace clock_sync {

void start() noexcept;                 // call after net::init(); sets TZ and starts SNTP
bool valid() noexcept;                 // true once the time has been set (after 2024)
bool local(std::tm& out) noexcept;     // local time; false if not yet valid

}  // namespace clock_sync
