#include "sensor/core/sht4x.hpp"

namespace sensor::sht4x {

bool decode(const std::uint8_t (&f)[kFrameLen], float& temp_c, float& rh) noexcept {
    if (crc8(f, 2) != f[2] || crc8(f + 3, 2) != f[5]) return false;
    const float t = static_cast<float>((f[0] << 8) | f[1]);
    const float h = static_cast<float>((f[3] << 8) | f[4]);
    temp_c = -45.0f + 175.0f * t / 65535.0f;
    rh = -6.0f + 125.0f * h / 65535.0f;
    if (rh < 0.0f) rh = 0.0f;
    if (rh > 100.0f) rh = 100.0f;
    return true;
}

}  // namespace sensor::sht4x
