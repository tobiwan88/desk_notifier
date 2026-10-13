// Kconfig -> constexpr config with compile-time validation (plan §3.0).
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

#include "sdkconfig.h"

namespace cfg {

inline constexpr std::array<int, 6> kLcdPins = {
    CONFIG_DESK_LCD_PIN_SCLK, CONFIG_DESK_LCD_PIN_MOSI, CONFIG_DESK_LCD_PIN_DC,   CONFIG_DESK_LCD_PIN_RST,
    CONFIG_DESK_LCD_PIN_CS, CONFIG_DESK_LCD_PIN_BL};  // BL = -1 unless MOSFET mod
inline constexpr bool kHasBacklightPwm = CONFIG_DESK_LCD_PIN_BL >= 0;
inline constexpr int kSpiHz = CONFIG_DESK_LCD_SPI_HZ;
inline constexpr std::size_t kStripLines = CONFIG_DESK_STRIP_LINES;
inline constexpr std::size_t kStripCount = CONFIG_DESK_STRIP_COUNT;

#if CONFIG_IDF_TARGET_ESP32S3
// strapping (0, 3, 45, 46), USB (19, 20), in-package flash/PSRAM (26..37, octal on R8), UART0.
consteval bool reserved(int p) {
    return p == 0 || p == 3 || p == 45 || p == 46 || p == 19 || p == 20 || (p >= 26 && p <= 37) || p == 43 ||
           p == 44;
}
#else
// C61: strapping, UART0, USB, PSRAM CS.
consteval bool reserved(int p) { return p >= 8 && p <= 14; }
#endif
consteval bool all_distinct(const std::array<int, 6>& a) {
    for (std::size_t i = 0; i < a.size(); ++i)
        for (std::size_t j = i + 1; j < a.size(); ++j)
            if (a[i] >= 0 && a[i] == a[j]) return false;
    return true;
}
consteval bool none_reserved(const std::array<int, 6>& a) {
    for (int p : a) if (reserved(p)) return false;
    return true;
}

static_assert(none_reserved(kLcdPins), "LCD pin on strapping/UART/USB/PSRAM pin");
static_assert(all_distinct(kLcdPins), "two LCD signals on the same GPIO");
static_assert(kSpiHz <= 40'000'000, "max 40 MHz on GPIO-matrix pins");
inline constexpr int kI2cSda = CONFIG_DESK_I2C_SDA;
inline constexpr int kI2cScl = CONFIG_DESK_I2C_SCL;
inline constexpr int kBmeAddr = CONFIG_DESK_BME_ADDR;
inline constexpr int kButton = CONFIG_DESK_BTN;
static_assert(!reserved(kI2cSda) && !reserved(kI2cScl) && !reserved(kButton), "sensor/button on reserved pin");
static_assert((240 * kStripLines * 2) % 4 == 0, "DMA length must be a multiple of 4");

}  // namespace cfg
