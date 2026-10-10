#include "clip/idf/player.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "storage/idf/storage.hpp"

namespace clip {
namespace {
const char* const kTag = "clip";
bool pread_exact(int fd, std::uint32_t off, void* dst, std::size_t n) {
    return lseek(fd, static_cast<off_t>(off), SEEK_SET) == static_cast<off_t>(off) && read(fd, dst, n) == static_cast<ssize_t>(n);
}
}  // namespace

ClipPlayer::~ClipPlayer() { stop(); }

std::expected<void, engine::Err> ClipPlayer::start(std::string_view name, bool loop) noexcept {
    stop();
    char path[96];
    if (!storage::path_for(name, path, sizeof path)) return std::unexpected(engine::Err::BadName);
    fd_ = open(path, O_RDONLY);
    if (fd_ < 0) return std::unexpected(engine::Err::NotFound);
    std::array<std::byte, sizeof(Header)> hb{};
    if (!pread_exact(fd_, 0, hb.data(), hb.size())) { stop(); return std::unexpected(engine::Err::Io); }
    auto h = parse_header(hb);
    if (!h) { stop(); return std::unexpected(h.error()); }
    const std::size_t bytes = std::size_t{h->frame_count} * sizeof(IndexEntry);
    index_ = static_cast<IndexEntry*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM));
    if (!index_) index_ = static_cast<IndexEntry*>(heap_caps_malloc(bytes, MALLOC_CAP_DEFAULT));
    if (!index_ || !pread_exact(fd_, h->index_offset, index_, bytes)) { stop(); return std::unexpected(engine::Err::Io); }
    frames_ = h->frame_count;
    loop_ = loop;
    next_idx_ = 0;
    started_ = false;
    storage::mark_playing(slot_, name);
    return {};
}

void ClipPlayer::stop() noexcept {
    if (fd_ >= 0) { close(fd_); fd_ = -1; }
    if (index_) { heap_caps_free(index_); index_ = nullptr; }
    storage::clear_playing(slot_);
    frames_ = 0;
    nrects_ = 0;
}

bool ClipPlayer::read_frame(std::uint32_t idx) noexcept {
    const IndexEntry& e = index_[idx];
    std::uint16_t n = 0;
    if (!pread_exact(fd_, e.offset, &n, 2) || n > kMaxRects) return false;
    std::uint32_t p = e.offset + 4;
    nrects_ = 0;
    for (std::uint16_t i = 0; i < n; ++i) {
        std::array<std::byte, sizeof(RectHdr)> rb{};
        if (!pread_exact(fd_, p, rb.data(), rb.size())) return false;
        auto r = parse_rect(rb);
        if (!r) return false;
        rects_[nrects_++] = {{static_cast<std::int16_t>(r->x), static_cast<std::int16_t>(r->y),
                              static_cast<std::int16_t>(r->w), static_cast<std::int16_t>(r->h)}, p + sizeof(RectHdr)};
        p += sizeof(RectHdr) + r->payload_len;
    }
    return true;
}

engine::FramePlan ClipPlayer::next(std::uint32_t now) noexcept {
    engine::FramePlan plan;
    plan.trim = false;
    if (fd_ < 0) { plan.finished = true; return plan; }
    if (started_ && static_cast<std::int32_t>(now - due_) < 0) {  // current frame still on screen
        plan.delay_ms = std::max<std::uint32_t>(due_ - now, 1);
        return plan;
    }
    if (next_idx_ >= frames_) {
        if (!loop_) { plan.finished = true; return plan; }
        next_idx_ = 0;
    }
    if (!read_frame(next_idx_)) { ESP_LOGW(kTag, "read error at frame %u", next_idx_); plan.finished = true; return plan; }
    for (std::size_t i = 0; i < nrects_; ++i) (void)plan.dirty.push_back(rects_[i].r);
    plan.full_redraw = false;
    const std::uint32_t dur = std::max<std::uint16_t>(index_[next_idx_].duration_ms, 1);
    due_ = (started_ ? due_ : now) + dur;  // accumulate to avoid drift
    if (static_cast<std::int32_t>(now - due_) > 200) due_ = now + dur;
    started_ = true;
    plan.delay_ms = dur;
    ++next_idx_;
    return plan;
}

std::expected<void, engine::Err> ClipPlayer::fill(gfx::Rect s, std::span<gfx::Rgb565be> buf) noexcept {
    for (std::size_t i = 0; i < nrects_; ++i) {
        const Rect& r = rects_[i];
        if (s.y < r.r.y || s.y + s.h > r.r.y + r.r.h || s.x != r.r.x || s.w != r.r.w) continue;
        const std::uint32_t off = r.payload + static_cast<std::uint32_t>(s.y - r.r.y) * r.r.w * 2;
        const std::size_t n = static_cast<std::size_t>(s.w) * s.h * 2;
        if (buf.size() * 2 < n || !pread_exact(fd_, off, buf.data(), n)) return std::unexpected(engine::Err::Io);
        return {};
    }
    return std::unexpected(engine::Err::Io);
}

}  // namespace clip
