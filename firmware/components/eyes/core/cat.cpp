#include "eyes/core/cat.hpp"

#include "cat_eyes.h"

namespace eyes {

void CatModel::start(std::uint32_t seed, std::uint8_t style, std::uint8_t iris, bool auto_idle) noexcept {
    rng_ = seed | 1u;
    cat_eyes_init(seed);
    cat_eyes_set_swap_bytes(true);  // GC9A01 wants big-endian RGB565
    cat_eyes_set_style(static_cast<cat_style_t>(style < CAT_STYLE_COUNT ? static_cast<int>(style) : 0));
    cat_eyes_set_iris(static_cast<cat_iris_t>(iris < CAT_IRIS_COUNT ? static_cast<int>(iris) : 0));
    cat_eyes_set_auto(auto_idle);
}

void CatModel::apply(const engine::EyeCmd& c, std::uint32_t) noexcept {
    if (c.style && *c.style < CAT_STYLE_COUNT) cat_eyes_set_style(static_cast<cat_style_t>(*c.style));
    if (c.iris && *c.iris < CAT_IRIS_COUNT) cat_eyes_set_iris(static_cast<cat_iris_t>(*c.iris));
    if (c.auto_idle) cat_eyes_set_auto(*c.auto_idle);
    if (c.play) {
        if (*c.play == engine::kPlayBlink) {
            cat_eyes_blink();
        } else if (*c.play == engine::kPlayRandom) {
            rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
            const bool has_special = cat_eyes_get_style() >= CAT_STYLE_ANIME;
            const std::uint32_t n = has_special ? CAT_ANIM_COUNT : CAT_ANIM_COUNT - 1;
            cat_eyes_play(static_cast<cat_anim_t>(rng_ % n));
        } else if (*c.play < CAT_ANIM_COUNT) {
            cat_eyes_play(static_cast<cat_anim_t>(*c.play));
        }
    }
    if (c.look_q8) cat_eyes_look((*c.look_q8)[0] / 256.f, (*c.look_q8)[1] / 256.f, c.hold_ms);
    if (c.save) save_pending_ = true;
}

void CatModel::tick(std::uint32_t now) noexcept { cat_eyes_tick(now); }

CatInfo CatModel::info() noexcept {
    const auto style = cat_eyes_get_style();
    CatInfo i{};
    i.style = static_cast<std::uint8_t>(style);
    i.iris = static_cast<std::uint8_t>(cat_eyes_get_iris());
    i.auto_idle = cat_eyes_get_auto();
    i.anim = cat_eyes_anim_name(cat_eyes_current_anim());
    i.style_name = kStyleNames[i.style < kStyleNames.size() ? i.style : 0];
    i.iris_name = kIrisNames[i.iris < kIrisNames.size() ? i.iris : 0];
    return i;
}

void CatView::invalidate() noexcept { cat_eyes_force_full(static_cast<int>(eye_)); }

engine::FramePlan CatView::next(std::uint32_t) noexcept {
    engine::FramePlan p;
    p.trim = false;  // cat_eyes renders full 240-px rows
    p.delay_ms = 33;
    int y0 = 0, y1 = 0;
    if (cat_eyes_dirty_rows(static_cast<int>(eye_), &y0, &y1)) {
        (void)p.dirty.push_back(gfx::Rect{0, static_cast<std::int16_t>(y0), gfx::kScreen, static_cast<std::int16_t>(y1 - y0)});
        p.full_redraw = (y0 == 0 && y1 == gfx::kScreen);
    } else {
        p.delay_ms = 50;
    }
    return p;
}

std::expected<void, engine::Err> CatView::fill(gfx::Rect r, std::span<gfx::Rgb565be> buf) noexcept {
    if (r.x != 0 || r.w != gfx::kScreen || buf.size() < static_cast<std::size_t>(r.area())) return std::unexpected(engine::Err::Io);
    cat_eyes_render_strip(static_cast<int>(eye_), r.y, r.h, buf.data());
    return {};
}

}  // namespace eyes
