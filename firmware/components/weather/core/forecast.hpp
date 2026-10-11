// weather: today's forecast from the Open-Meteo API (no key needed). Pure C++: parsing and condition mapping.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace weather {

enum class Sky : std::uint8_t { Clear, PartlyCloudy, Cloudy, Fog, Drizzle, Rain, Snow, Thunder };

struct Forecast {
    float temp_c = 0;        // current outdoor temperature
    bool is_day = true;      // current
    int code = 0;            // today's WMO weather code (daily forecast)
    Sky sky = Sky::Clear;    // from `code`
    float max_c = 0, min_c = 0;
    int precip_pct = -1;     // today's max precipitation probability, -1 = unknown
    float wind_kmh = 0;      // current wind speed at 10 m; 0 if the API did not send it
};

// WMO 4677 weather interpretation code (as used by Open-Meteo) -> picture.
[[nodiscard]] Sky sky_from_wmo(int code) noexcept;
[[nodiscard]] const char* sky_name(Sky s, bool is_day) noexcept;

// Parses the body of
//   /v1/forecast?...&current=temperature_2m,is_day,weather_code,wind_speed_10m
//                   &daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max
// Returns nothing if a required field is missing.
[[nodiscard]] std::optional<Forecast> parse_open_meteo(std::string_view json) noexcept;

}  // namespace weather
