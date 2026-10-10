// engine: the random pool = which built-in animations and which clips the random mode may pick.
#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "engine/core/command.hpp"
#include "engine/core/parse.hpp"
#include "engine/core/static.hpp"

namespace engine {

inline constexpr std::size_t kMaxPoolClips = 12;
inline constexpr int kAnimSlots = 31;                      // cat_eyes animations (enum order); the last one is "special"
inline constexpr int kSpecialAnim = kAnimSlots - 1;
inline constexpr std::uint32_t kAllAnims = (1u << kAnimSlots) - 1;  // bit i = cat_eyes animation id i

struct Pool {
    std::uint32_t anim_mask = kAllAnims;
    StaticVec<StaticString<31>, kMaxPoolClips> clips;  // enabled clip names
    std::uint16_t min_ds = 25, max_ds = 70;             // pause between random picks, in 0.1 s
    friend bool operator==(const Pool&, const Pool&) noexcept;
};

struct Pick {
    enum class Kind : std::uint8_t { None, Anim, Clip } kind = Kind::None;
    int index = 0;  // animation id or index into Pool::clips
};

// Weighted random choice. `last` = key of the previous pick (never picked twice in a row when there is a choice):
// animation id, or 64 + clip index.
[[nodiscard]] Pick pick(const Pool&, std::uint32_t& rng, int last, bool style_has_special) noexcept;
[[nodiscard]] constexpr int pick_key(const Pick& p) noexcept { return p.kind == Pick::Kind::Clip ? 64 + p.index : p.index; }

// Request body of POST /api/pool: {"enabled":[names...],"interval":[min_s,max_s]}, both optional.
struct PoolPatch {
    bool has_enabled = false, has_interval = false;
    std::uint32_t anim_mask = 0;
    StaticVec<StaticString<31>, kMaxPoolClips> clips;  // names that are not built-in animations (the caller checks they exist)
    std::uint16_t min_ds = 25, max_ds = 70;
};
[[nodiscard]] std::expected<PoolPatch, ParseError> parse_pool(std::string_view json) noexcept;
void apply(Pool&, const PoolPatch&) noexcept;
[[nodiscard]] std::string pool_to_json(const Pool&);  // same shape as the request, used for NVS

}  // namespace engine
