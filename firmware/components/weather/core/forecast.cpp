#include "weather/core/forecast.hpp"

#include <cstdlib>
#include <cstring>

namespace weather {
namespace {

// The `{...}` object that follows "name": (exact key, so "current" does not match "current_units").
std::string_view object(std::string_view json, std::string_view name) noexcept {
    std::size_t at = 0;
    for (;;) {
        at = json.find(name, at);
        if (at == std::string_view::npos) return {};
        const bool quoted = at > 0 && json[at - 1] == '"' && at + name.size() < json.size() && json[at + name.size()] == '"';
        std::size_t p = at + name.size() + 1;
        at += name.size();
        if (!quoted) continue;
        while (p < json.size() && (json[p] == ' ' || json[p] == ':')) ++p;
        if (p >= json.size() || json[p] != '{') continue;
        int depth = 0;
        for (std::size_t e = p; e < json.size(); ++e) {
            if (json[e] == '{') ++depth;
            else if (json[e] == '}' && --depth == 0) return json.substr(p, e - p + 1);
        }
        return {};
    }
}

// Number after "key": (or the first element of "key":[...]). Returns false for missing or null values.
bool number(std::string_view obj, std::string_view key, float& out) noexcept {
    std::size_t at = 0;
    for (;;) {
        at = obj.find(key, at);
        if (at == std::string_view::npos) return false;
        const std::size_t end = at + key.size();
        const bool quoted = at > 0 && obj[at - 1] == '"' && end < obj.size() && obj[end] == '"';
        at = end;
        if (!quoted) continue;
        std::size_t p = end + 1;
        while (p < obj.size() && (obj[p] == ' ' || obj[p] == ':' || obj[p] == '[')) ++p;
        char buf[24]{};
        std::size_t n = 0;
        while (p < obj.size() && n < sizeof buf - 1 && std::strchr("+-.0123456789eE", obj[p])) buf[n++] = obj[p++];
        if (n == 0) return false;
        char* stop = nullptr;
        out = std::strtof(buf, &stop);
        return stop != buf;
    }
}

}  // namespace

Sky sky_from_wmo(int code) noexcept {
    if (code <= 1) return Sky::Clear;
    if (code == 2) return Sky::PartlyCloudy;
    if (code == 3) return Sky::Cloudy;
    if (code == 45 || code == 48) return Sky::Fog;
    if (code >= 51 && code <= 57) return Sky::Drizzle;
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return Sky::Rain;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Sky::Snow;
    if (code >= 95) return Sky::Thunder;
    return Sky::Cloudy;
}

const char* sky_name(Sky s, bool is_day) noexcept {
    switch (s) {
        case Sky::Clear: return is_day ? "SUNNY" : "CLEAR";
        case Sky::PartlyCloudy: return "PARTLY CLOUDY";
        case Sky::Cloudy: return "CLOUDY";
        case Sky::Fog: return "FOGGY";
        case Sky::Drizzle: return "DRIZZLE";
        case Sky::Rain: return "RAIN";
        case Sky::Snow: return "SNOW";
        case Sky::Thunder: return "THUNDER";
    }
    return "";
}

std::optional<Forecast> parse_open_meteo(std::string_view json) noexcept {
    const std::string_view cur = object(json, "current"), day = object(json, "daily");
    if (cur.empty() || day.empty()) return std::nullopt;
    Forecast f;
    float v = 0;
    if (!number(cur, "temperature_2m", f.temp_c)) return std::nullopt;
    if (number(cur, "is_day", v)) f.is_day = v > 0.5f;
    if (!number(day, "weather_code", v)) {
        if (!number(cur, "weather_code", v)) return std::nullopt;
    }
    f.code = static_cast<int>(v);
    f.sky = sky_from_wmo(f.code);
    if (!number(day, "temperature_2m_max", f.max_c)) f.max_c = f.temp_c;
    if (!number(day, "temperature_2m_min", f.min_c)) f.min_c = f.temp_c;
    if (number(day, "precipitation_probability_max", v)) f.precip_pct = static_cast<int>(v + 0.5f);
    return f;
}

}  // namespace weather
