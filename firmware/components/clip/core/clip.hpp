// clip: format structs and validation (plan §3.5, §5.1). Pure C++.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>

#include "engine/core/types.hpp"
#include "gfx/core/gfx.hpp"

namespace clip {

inline constexpr std::uint16_t kVersion = 1;
inline constexpr std::size_t kMaxRects = 16;
inline constexpr std::uint16_t kMaxFrames = 4096;
inline constexpr std::uint8_t kFlagLoop = 1, kFlagCircleTrimmed = 8;

#pragma pack(push, 1)
struct Header { char magic[4]; std::uint16_t version, header_size, width, height;
                std::uint8_t pixel_format, flags; std::uint16_t frame_count, key_color, palette_count;
                std::uint32_t index_offset, palette_offset, data_crc32; };
struct IndexEntry { std::uint32_t offset, length; std::uint16_t duration_ms, flags; };
struct RectHdr { std::uint16_t x, y, w, h; std::uint8_t encoding, reserved; std::uint32_t payload_len; };
#pragma pack(pop)
static_assert(sizeof(Header) == 32 && sizeof(IndexEntry) == 12 && sizeof(RectHdr) == 14);

// Reads `out.size()` bytes at absolute `offset`; false on failure.
struct Reader {
    bool (*read)(void* ctx, std::uint32_t offset, std::span<std::byte> out) noexcept;
    void* ctx;
};

constexpr std::uint32_t crc32(std::uint32_t crc, std::span<const std::byte> data) noexcept {
    crc = ~crc;
    for (const std::byte b : data) {
        crc ^= static_cast<std::uint32_t>(b);
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

[[nodiscard]] std::expected<Header, engine::Err> parse_header(std::span<const std::byte>) noexcept;
[[nodiscard]] std::expected<RectHdr, engine::Err> parse_rect(std::span<const std::byte>) noexcept;
// Rect must be inside the screen and inside the circle-trim span of its rows.
[[nodiscard]] bool rect_ok(const RectHdr&) noexcept;
// Full validation of a stored clip of `size` bytes: header, index, every rect, CRC.
[[nodiscard]] std::expected<Header, engine::Err> validate(Reader, std::uint32_t size) noexcept;

}  // namespace clip
