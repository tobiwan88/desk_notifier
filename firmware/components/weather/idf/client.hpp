// weather: fetches today's forecast from Open-Meteo in the background (every CONFIG_DESK_WEATHER_REFRESH_MIN).
#pragma once

#include <cstdint>
#include <expected>
#include <string>

#include "engine/core/types.hpp"
#include "weather/core/forecast.hpp"

namespace weather {

struct Status {
    bool ok = false;           // a forecast younger than 3 hours is available
    Forecast forecast{};
    std::uint32_t age_s = 0;   // since the last good fetch
    int errors = 0;            // failed fetches in a row
    int http_status = 0;       // of the last attempt
};

[[nodiscard]] std::expected<void, engine::Err> start() noexcept;  // call after net::init()
Status latest() noexcept;
void refresh() noexcept;
// Shows this forecast instead of the real one for `seconds` (0 = back to the real one). For trying the squirrel outfits.
void set_demo(const Forecast& f, std::uint32_t seconds) noexcept;       // fetch again now
std::string place() noexcept;  // the name shown on the page, e.g. "MUNICH"

}  // namespace weather
