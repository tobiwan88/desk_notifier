// eyes: shared animated model (both eyes in sync) and per-display FrameSource (plan §3.3).
#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

#include "engine/core/command.hpp"
#include "eyes/core/eyes.hpp"

namespace eyes {

class EyesModel {
public:
    void start(std::uint32_t seed, const EyeStyle& style) noexcept;
    void apply(const engine::EyeCmd&, std::uint32_t now) noexcept;
    void tick(std::uint32_t now) noexcept;  // once per frame, before any EyeView::next()
    const EyeGeom& geom(engine::Eye e) const noexcept { return geom_[static_cast<std::size_t>(e)]; }
    const EyeStyle& style() const noexcept { return style_; }
    const State& state() const noexcept { return state_; }
    bool style_dirty() const noexcept { return style_saved_pending_; }
    void clear_style_dirty() noexcept { style_saved_pending_ = false; }

private:
    using Params = std::array<int, 7>;  // lid_top, lid_bot, tilt, pupil, scale_l, scale_r, lid_top_r
    Params target_for(Mood) const noexcept;
    std::uint32_t rnd(std::uint32_t lo, std::uint32_t hi) noexcept;
    void start_blink(std::size_t eye, std::uint32_t now) noexcept;

    EyeStyle style_{};
    State state_{};
    std::array<EyeGeom, 2> geom_{};
    Params from_{}, to_{}, cur_{};
    std::uint32_t t0_ = 0, dur_ = 0;
    std::optional<engine::LidCmd> lid_override_;
    std::optional<std::uint8_t> pupil_override_;
    std::array<int, 2> look_from_{}, look_to_{};
    std::uint32_t look_t0_ = 0, look_dur_ = 1;
    std::array<std::int64_t, 2> blink_t0_{-1, -1};
    bool auto_blink_ = true, auto_look_ = true;
    std::uint32_t next_blink_ = 0, next_look_ = 0, rng_ = 1;
    bool style_saved_pending_ = false;
    bool started_ = false;
};

class EyeView {
public:
    static constexpr std::string_view kName = "eyes";
    EyeView(EyesModel& m, engine::Eye e) noexcept : m_(&m), eye_(e) {}
    engine::FramePlan next(std::uint32_t now) noexcept;
    std::expected<void, engine::Err> fill(gfx::Rect, std::span<gfx::Rgb565be>) noexcept;
    void stop() noexcept {}
    void invalidate() noexcept { have_prev_ = false; }  // next() will request a full redraw

private:
    EyesModel* m_;
    engine::Eye eye_;
    EyeGeom prev_{}, cur_{};
    bool have_prev_ = false;
};

static_assert(engine::FrameSource<EyeView>);

}  // namespace eyes
