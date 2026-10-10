// clip: ClipPlayer FrameSource, one per display (plan §3.5). Reads RAW565 rects straight into DMA strips.
#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

#include "clip/core/clip.hpp"
#include "engine/core/command.hpp"

namespace clip {

class ClipPlayer {
public:
    static constexpr std::string_view kName = "clip";
    ClipPlayer() noexcept = default;
    ClipPlayer(const ClipPlayer&) = delete;
    ~ClipPlayer();
    // `slot` (0/1) is only used to register the playing clip with storage.
    void set_slot(int slot) noexcept { slot_ = slot; }
    std::expected<void, engine::Err> start(std::string_view name, bool loop) noexcept;
    engine::FramePlan next(std::uint32_t now) noexcept;
    std::expected<void, engine::Err> fill(gfx::Rect, std::span<gfx::Rgb565be>) noexcept;
    void stop() noexcept;

private:
    struct Rect { gfx::Rect r; std::uint32_t payload; };
    bool read_frame(std::uint32_t idx) noexcept;

    int fd_ = -1, slot_ = 0;
    IndexEntry* index_ = nullptr;
    std::uint16_t frames_ = 0, next_idx_ = 0;
    bool loop_ = false, started_ = false;
    std::uint32_t due_ = 0;
    std::array<Rect, kMaxRects> rects_{};
    std::size_t nrects_ = 0;
};

}  // namespace clip
