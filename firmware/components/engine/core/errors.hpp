// engine: error code -> API name / HTTP status (plan §5.5).
#pragma once

#include <string_view>

#include "engine/core/types.hpp"

namespace engine {

struct ErrInfo { std::string_view code; int http; };

constexpr ErrInfo describe(Err e) noexcept {
    switch (e) {
        case Err::Ok:             return {"OK", 200};
        case Err::Unauthorized:   return {"E_UNAUTHORIZED", 401};
        case Err::BadJson:        return {"E_BAD_JSON", 400};
        case Err::TooLarge:       return {"E_TOO_LARGE", 413};
        case Err::NotFound:       return {"E_NOT_FOUND", 404};
        case Err::BadName:        return {"E_BAD_NAME", 400};
        case Err::BadClip:        return {"E_BAD_CLIP", 422};
        case Err::NoSpace:        return {"E_NO_SPACE", 507};
        case Err::Busy:           return {"E_BUSY", 503};
        case Err::Conflict:       return {"E_CONFLICT", 409};
        case Err::LengthRequired: return {"E_LENGTH_REQUIRED", 411};
        case Err::Io:             return {"E_IO", 500};
        case Err::Ota:            return {"E_OTA", 500};
        case Err::DisplayStall:   return {"E_IO", 500};
    }
    return {"E_IO", 500};
}

// Constant-time comparison for tokens.
constexpr bool ct_equal(std::string_view a, std::string_view b) noexcept {
    unsigned diff = static_cast<unsigned>(a.size() ^ b.size());
    const std::size_t n = a.size() < b.size() ? a.size() : b.size();
    for (std::size_t i = 0; i < n; ++i) diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return diff == 0;
}

// Extracts the token from "Bearer <token>"; empty view if malformed.
constexpr std::string_view bearer_token(std::string_view header) noexcept {
    constexpr std::string_view p = "Bearer ";
    if (header.size() <= p.size() || header.substr(0, p.size()) != p) return {};
    return header.substr(p.size());
}

}  // namespace engine
