// engine: commands, FramePlan, FrameSource concept (frozen contract, plan §3.2).
#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>

#include "engine/core/static.hpp"
#include "engine/core/types.hpp"
#include "eyes/core/style.hpp"
#include "gfx/core/gfx.hpp"

namespace engine {

struct FramePlan {
    StaticVec<gfx::Rect, 8> dirty;
    bool full_redraw = false;
    bool trim = true;
    std::uint32_t delay_ms = 33;
    bool finished = false;
};

template <class S>
concept FrameSource = requires(S s, std::uint32_t now, gfx::Rect r, std::span<gfx::Rgb565be> buf) {
    { s.next(now) } noexcept -> std::same_as<FramePlan>;
    { s.fill(r, buf) } noexcept -> std::same_as<std::expected<void, Err>>;
    { s.stop() } noexcept;
    { S::kName } -> std::convertible_to<std::string_view>;
};

enum class EyeSel : std::uint8_t { Both, Left, Right };

struct LidCmd { std::optional<std::uint8_t> top, bottom; std::optional<std::int8_t> tilt; };

struct EyeCmd {
    std::optional<eyes::Mood> mood;
    std::optional<std::array<std::int16_t, 2>> look_q8;   // -256..256
    std::optional<EyeSel> blink;                          // Both = blink, Left/Right = wink
    std::optional<bool> auto_blink, auto_look;
    std::optional<std::uint8_t> pupil;                    // 64..255
    std::optional<LidCmd> lids;
    std::optional<eyes::StylePatch> style;
    bool save_style = false;
    std::optional<std::uint8_t> brightness;               // 0..100
    std::optional<std::uint16_t> transition_ms;
    bool interrupt = false;
};

struct PlayCmd {
    std::array<StaticString<31>, kEyes> clip;  // empty = keep eyes on that display
    bool loop = false;
    bool then_eyes = true;
};
struct StopCmd {};
using Command = std::variant<EyeCmd, PlayCmd, StopCmd>;
static_assert(std::is_trivially_copyable_v<Command>);

[[nodiscard]] constexpr bool clip_name_valid(std::string_view n) noexcept {
    if (n.empty() || n.size() > 31) return false;
    for (char c : n)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}

}  // namespace engine
