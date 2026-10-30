// sensor: Sensirion SHT4x measurement frame decoding (CRC-8 and ticks to degC / %RH). Pure C++.
#pragma once

#include <cstdint>

namespace sensor::sht4x {

inline constexpr std::uint8_t kCmdMeasureHigh = 0xFD;  // high precision, 8.3 ms max
inline constexpr int kMeasureMs = 10;
inline constexpr int kFrameLen = 6;  // T msb, T lsb, CRC, RH msb, RH lsb, CRC

[[nodiscard]] constexpr std::uint8_t crc8(const std::uint8_t* d, int n) noexcept {  // poly 0x31, init 0xFF
    std::uint8_t crc = 0xFF;
    for (int i = 0; i < n; ++i) {
        crc ^= d[i];
        for (int b = 0; b < 8; ++b) crc = static_cast<std::uint8_t>((crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1);
    }
    return crc;
}

// False when a CRC does not match. RH is clamped to 0..100 as the datasheet asks.
[[nodiscard]] bool decode(const std::uint8_t (&frame)[kFrameLen], float& temp_c, float& rh) noexcept;

}  // namespace sensor::sht4x
