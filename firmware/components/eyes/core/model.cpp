#include "eyes/core/model.hpp"

namespace eyes {
namespace {
constexpr int lerp_i(int a, int b, int t255) { return a + (b - a) * t255 / 255; }
}  // namespace

std::uint32_t EyesModel::rnd(std::uint32_t lo, std::uint32_t hi) noexcept {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return lo + rng_ % (hi - lo + 1);
}

EyesModel::Params EyesModel::target_for(Mood m) const noexcept {
    const MoodShape& s = kMoods[std::to_underlying(m)];
    Params p = {s.lid_top, s.lid_bot, s.tilt, s.pupil, s.scale, s.scale, s.lid_top};
    if (m == Mood::Curious) { p[6] = 60; p[4] = 106; p[5] = 94; }
    if (lid_override_) {
        if (lid_override_->top) { p[0] = *lid_override_->top; p[6] = *lid_override_->top; }
        if (lid_override_->bottom) p[1] = *lid_override_->bottom;
        if (lid_override_->tilt) p[2] = *lid_override_->tilt;
    }
    if (pupil_override_ && m != Mood::Asleep) p[3] = *pupil_override_;
    return p;
}

void EyesModel::start(std::uint32_t seed, const EyeStyle& style) noexcept {
    style_ = style;
    rng_ = seed ? seed : 1;
    state_ = State{};
    to_ = from_ = cur_ = target_for(Mood::Neutral);
    t0_ = 0; dur_ = 0;
    look_from_ = look_to_ = {0, 0};
    next_blink_ = rnd(2500, 6000);
    next_look_ = rnd(1500, 4000);
    started_ = true;
    for (std::size_t i = 0; i < 2; ++i) geom_[i] = layout(style_, state_, static_cast<engine::Eye>(i));
}

void EyesModel::start_blink(std::size_t eye, std::uint32_t now) noexcept { blink_t0_[eye] = now; }

void EyesModel::apply(const engine::EyeCmd& c, std::uint32_t now) noexcept {
    const std::uint32_t tr = c.transition_ms.value_or(250);
    if (c.style) {
        eyes::apply(style_, *c.style);
        if (c.save_style) style_saved_pending_ = true;
    }
    if (c.brightness) state_.brightness = *c.brightness > 100 ? 100 : *c.brightness;
    bool retarget = false;
    if (c.mood) { state_.mood = *c.mood; lid_override_.reset(); retarget = true; }
    if (c.pupil) { pupil_override_ = *c.pupil < 64 ? 64 : *c.pupil; retarget = true; }
    if (c.lids) {
        engine::LidCmd o = lid_override_.value_or(engine::LidCmd{});
        if (c.lids->top) o.top = c.lids->top;
        if (c.lids->bottom) o.bottom = c.lids->bottom;
        if (c.lids->tilt) o.tilt = c.lids->tilt;
        lid_override_ = o;
        retarget = true;
    }
    if (retarget) {
        from_ = cur_;  // retarget from the current interpolated state
        to_ = target_for(state_.mood);
        t0_ = now; dur_ = tr;
    }
    if (c.look_q8) {
        look_from_ = {state_.look_x, state_.look_y};
        auto cl = [](int v) { return v < -256 ? -256 : (v > 256 ? 256 : v); };
        look_to_ = {cl((*c.look_q8)[0]), cl((*c.look_q8)[1])};
        look_t0_ = now; look_dur_ = c.transition_ms.value_or(120);
        if (look_dur_ == 0) look_dur_ = 1;
    }
    if (c.auto_blink) auto_blink_ = *c.auto_blink;
    if (c.auto_look) auto_look_ = *c.auto_look;
    if (c.blink) {
        if (*c.blink != engine::EyeSel::Right) start_blink(0, now);
        if (*c.blink != engine::EyeSel::Left) start_blink(1, now);
    }
}

void EyesModel::tick(std::uint32_t now) noexcept {
    if (!started_) return;
    // mood group
    const std::uint32_t dt = now - t0_;
    const int e = dur_ == 0 || dt >= dur_ ? 255 : gfx::kEaseInOut[dt * 256 / dur_];
    for (std::size_t i = 0; i < cur_.size(); ++i) cur_[i] = lerp_i(from_[i], to_[i], e);
    state_.lid_top = static_cast<std::uint8_t>(cur_[0]);
    state_.lid_bot = static_cast<std::uint8_t>(cur_[1]);
    state_.tilt = static_cast<std::int8_t>(cur_[2]);
    state_.pupil = static_cast<std::uint8_t>(cur_[3]);
    state_.scale_l = static_cast<std::uint8_t>(cur_[4]);
    state_.scale_r = static_cast<std::uint8_t>(cur_[5]);
    state_.lid_top_r = static_cast<std::uint8_t>(cur_[6]);
    // look
    const std::uint32_t ld = now - look_t0_;
    const int le = ld >= look_dur_ ? 255 : gfx::kEaseInOut[ld * 256 / look_dur_];
    state_.look_x = static_cast<std::int16_t>(lerp_i(look_from_[0], look_to_[0], le));
    state_.look_y = static_cast<std::int16_t>(lerp_i(look_from_[1], look_to_[1], le));
    // blink (triangle over 150 ms)
    const bool asleep = state_.mood == Mood::Asleep;
    for (std::size_t i = 0; i < 2; ++i) {
        std::uint16_t v = 0;
        if (blink_t0_[i] >= 0) {
            const std::uint32_t bt = now - static_cast<std::uint32_t>(blink_t0_[i]);
            if (bt >= 150) blink_t0_[i] = -1;
            else v = static_cast<std::uint16_t>(bt < 75 ? bt * 256 / 75 : (150 - bt) * 256 / 75);
        }
        (i == 0 ? state_.blink_l : state_.blink_r) = v;
    }
    // idle behaviour
    if (!asleep) {
        if (auto_blink_ && static_cast<std::int32_t>(now - next_blink_) >= 0) {
            start_blink(0, now); start_blink(1, now);
            next_blink_ = now + rnd(2500, 6000);
        }
        if (auto_look_ && static_cast<std::int32_t>(now - next_look_) >= 0) {
            look_from_ = {state_.look_x, state_.look_y};
            look_to_ = {static_cast<int>(rnd(0, 400)) - 200, static_cast<int>(rnd(0, 300)) - 150};
            look_t0_ = now; look_dur_ = 120;
            next_look_ = now + rnd(1500, 4000);
        }
    }
    for (std::size_t i = 0; i < 2; ++i) geom_[i] = layout(style_, state_, static_cast<engine::Eye>(i));
}

engine::FramePlan EyeView::next(std::uint32_t) noexcept {
    engine::FramePlan p;
    cur_ = m_->geom(eye_);
    p.trim = true;
    p.delay_ms = 33;
    if (!have_prev_) {
        p.full_redraw = true;
        (void)p.dirty.push_back(gfx::Rect{0, 0, gfx::kScreen, gfx::kScreen});
        have_prev_ = true;
    } else {
        const auto d = diff(prev_, cur_);
        for (const auto& r : d) (void)p.dirty.push_back(r);
        if (d.empty()) p.delay_ms = 100;
    }
    prev_ = cur_;
    return p;
}

std::expected<void, engine::Err> EyeView::fill(gfx::Rect r, std::span<gfx::Rgb565be> buf) noexcept {
    if (buf.size() < static_cast<std::size_t>(r.area())) return std::unexpected(engine::Err::Io);
    render(cur_, r, buf);
    return {};
}

}  // namespace eyes
