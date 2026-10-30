#include <array>
#include <cmath>
#include <cstdint>

#include "doctest.h"
#include "sensor/core/sht4x.hpp"

using namespace sensor::sht4x;

static_assert(crc8(std::array<std::uint8_t, 2>{0xBE, 0xEF}.data(), 2) == 0x92);  // Sensirion datasheet example

namespace {
void put(std::uint8_t (&f)[kFrameLen], std::uint16_t t, std::uint16_t h) {
    f[0] = static_cast<std::uint8_t>(t >> 8);
    f[1] = static_cast<std::uint8_t>(t);
    f[2] = crc8(f, 2);
    f[3] = static_cast<std::uint8_t>(h >> 8);
    f[4] = static_cast<std::uint8_t>(h);
    f[5] = crc8(f + 3, 2);
}
}  // namespace

TEST_CASE("sht4x decodes ticks to degC and %RH") {
    std::uint8_t f[kFrameLen];
    float t = 0, h = 0;
    put(f, 0x6666, 0x8000);  // 0x6666 = 26214 -> 25.0 C, 0x8000 -> 56.5 %RH
    CHECK(decode(f, t, h));
    CHECK(std::fabs(t - 25.0f) < 0.01f);
    CHECK(std::fabs(h - 56.5f) < 0.01f);
}

TEST_CASE("sht4x clamps RH and rejects bad CRC") {
    std::uint8_t f[kFrameLen];
    float t = 0, h = 0;
    put(f, 0x6666, 0xFFFF);
    CHECK(decode(f, t, h));
    CHECK(h == 100.0f);
    put(f, 0x6666, 0);
    CHECK(decode(f, t, h));
    CHECK(h == 0.0f);
    put(f, 0x6666, 0x8000);
    f[2] ^= 1;
    CHECK_FALSE(decode(f, t, h));
    put(f, 0x6666, 0x8000);
    f[5] ^= 1;
    CHECK_FALSE(decode(f, t, h));
}
