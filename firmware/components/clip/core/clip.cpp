#include "clip/core/clip.hpp"

namespace clip {

std::expected<Header, engine::Err> parse_header(std::span<const std::byte> b) noexcept {
    if (b.size() < sizeof(Header)) return std::unexpected(engine::Err::BadClip);
    Header h;
    std::memcpy(&h, b.data(), sizeof h);
    if (std::memcmp(h.magic, "EYCL", 4) != 0 || h.version != kVersion || h.header_size != sizeof(Header) ||
        h.width != gfx::kScreen || h.height != gfx::kScreen || h.pixel_format != 1 ||
        !(h.flags & kFlagCircleTrimmed) || h.palette_count != 0 || h.palette_offset != 0 ||
        h.frame_count == 0 || h.frame_count > kMaxFrames)
        return std::unexpected(engine::Err::BadClip);
    return h;
}

std::expected<RectHdr, engine::Err> parse_rect(std::span<const std::byte> b) noexcept {
    if (b.size() < sizeof(RectHdr)) return std::unexpected(engine::Err::BadClip);
    RectHdr r;
    std::memcpy(&r, b.data(), sizeof r);
    if (!rect_ok(r)) return std::unexpected(engine::Err::BadClip);
    return r;
}

bool rect_ok(const RectHdr& r) noexcept {
    if (r.encoding != 0 || r.w == 0 || r.h == 0 || r.x + r.w > gfx::kScreen || r.y + r.h > gfx::kScreen) return false;
    if (r.payload_len != std::uint32_t{r.w} * r.h * 2) return false;
    const gfx::Chord span = gfx::span_of_rows(r.y, r.h);
    return r.x >= span.x0 && r.x + r.w <= span.x1;
}

std::expected<Header, engine::Err> validate(Reader rd, std::uint32_t size) noexcept {
    using engine::Err;
    std::array<std::byte, sizeof(Header)> hb{};
    if (size < sizeof(Header) || !rd.read(rd.ctx, 0, hb)) return std::unexpected(Err::BadClip);
    auto h = parse_header(hb);
    if (!h) return h;
    const std::uint32_t idx_bytes = std::uint32_t{h->frame_count} * sizeof(IndexEntry);
    if (h->index_offset < sizeof(Header) || h->index_offset + idx_bytes != size) return std::unexpected(Err::BadClip);

    // CRC over [32, size)
    std::array<std::byte, 512> chunk{};
    std::uint32_t crc = 0;
    for (std::uint32_t off = sizeof(Header); off < size;) {
        const std::uint32_t n = std::min<std::uint32_t>(chunk.size(), size - off);
        if (!rd.read(rd.ctx, off, std::span(chunk).first(n))) return std::unexpected(Err::Io);
        crc = crc32(crc, std::span<const std::byte>(chunk).first(n));
        off += n;
    }
    if (crc != h->data_crc32) return std::unexpected(Err::BadClip);

    for (std::uint32_t i = 0; i < h->frame_count; ++i) {
        std::array<std::byte, sizeof(IndexEntry)> eb{};
        if (!rd.read(rd.ctx, h->index_offset + i * sizeof(IndexEntry), eb)) return std::unexpected(Err::Io);
        IndexEntry e;
        std::memcpy(&e, eb.data(), sizeof e);
        if (e.offset < sizeof(Header) || e.length < 4 || e.offset > h->index_offset || e.length > h->index_offset - e.offset)
            return std::unexpected(Err::BadClip);
        std::array<std::byte, 4> fb{};
        if (!rd.read(rd.ctx, e.offset, fb)) return std::unexpected(Err::Io);
        std::uint16_t n;
        std::memcpy(&n, fb.data(), 2);
        if (n > kMaxRects) return std::unexpected(Err::BadClip);
        std::uint32_t p = 4;
        for (std::uint16_t r = 0; r < n; ++r) {
            std::array<std::byte, sizeof(RectHdr)> rb{};
            if (p + sizeof(RectHdr) > e.length || !rd.read(rd.ctx, e.offset + p, rb)) return std::unexpected(Err::BadClip);
            auto rh = parse_rect(rb);
            if (!rh) return std::unexpected(rh.error());
            p += sizeof(RectHdr);
            if (rh->payload_len > e.length - p) return std::unexpected(Err::BadClip);
            p += rh->payload_len;
        }
        if (p != e.length) return std::unexpected(Err::BadClip);
    }
    return h;
}

}  // namespace clip
