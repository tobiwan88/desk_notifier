// eyes: cat_eyes (C component) wrapped as a model + per-display FrameSource.
// cat_eyes keeps global state for both eyes: exactly one CatModel may exist, used from the render task only.
#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

#include "engine/core/command.hpp"

namespace eyes {

// Catalogue, in the enum order of cat_eyes.h (checked against the component's name helpers in tests).
inline constexpr std::array<std::string_view, 7> kStyleNames = {"realistic", "flat", "line", "anime", "cute", "halloween", "christmas"};
inline constexpr std::array<std::string_view, 6> kIrisNames = {"amber", "green", "blue", "copper", "violet", "odd"};
inline constexpr std::array<std::string_view, 12> kAnimNames = {"wink", "slow_blink", "roll", "side_eye", "hunt", "startle",
                                                               "nictitate", "sleepy", "cross_eyed", "happy", "glance", "special"};
// special animation name per style ("" = none)
inline constexpr std::array<std::string_view, 7> kSpecialNames = {"", "", "", "sparkle", "heart_eyes", "demon_flare", "twinkle"};

struct CatInfo {
    std::uint8_t style, iris;
    bool auto_idle;
    std::string_view anim;  // "idle" or the running animation's name
    std::string_view style_name, iris_name;
};

class CatModel {
public:
    void start(std::uint32_t seed, std::uint8_t style, std::uint8_t iris, bool auto_idle) noexcept;
    void apply(const engine::EyeCmd&, std::uint32_t now) noexcept;
    void tick(std::uint32_t now) noexcept;  // once per frame, before any CatView::next()
    static CatInfo info() noexcept;
    bool save_pending() const noexcept { return save_pending_; }
    void saved() noexcept { save_pending_ = false; }

private:
    std::uint32_t rng_ = 1;
    bool save_pending_ = false;
};

class CatView {
public:
    static constexpr std::string_view kName = "eyes";
    explicit CatView(engine::Eye e) noexcept : eye_(e) {}
    engine::FramePlan next(std::uint32_t now) noexcept;
    std::expected<void, engine::Err> fill(gfx::Rect, std::span<gfx::Rgb565be>) noexcept;
    void stop() noexcept {}
    void invalidate() noexcept;  // next() reports the whole eye

private:
    engine::Eye eye_;
};

static_assert(engine::FrameSource<CatView>);

}  // namespace eyes
