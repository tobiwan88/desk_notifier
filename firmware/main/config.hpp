// Kconfig -> constexpr config with compile-time validation (plan §3.0).
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

#include "sdkconfig.h"

namespace cfg {

inline constexpr std::array<int, 7> kLcdPins = {
    CONFIG_EYES_LCD_PIN_SCLK, CONFIG_EYES_LCD_PIN_MOSI, CONFIG_EYES_LCD_PIN_DC,   CONFIG_EYES_LCD_PIN_RST,
    CONFIG_EYES_LCD_PIN_CS_L, CONFIG_EYES_LCD_PIN_CS_R, CONFIG_EYES_LCD_PIN_BL};  // BL = -1 unless MOSFET mod
inline constexpr bool kHasBacklightPwm = CONFIG_EYES_LCD_PIN_BL >= 0;
inline constexpr int kSpiHz = CONFIG_EYES_LCD_SPI_HZ;
inline constexpr std::size_t kStripLines = CONFIG_EYES_STRIP_LINES;
inline constexpr std::size_t kStripCount = CONFIG_EYES_STRIP_COUNT;

consteval bool reserved(int p) { return p >= 7 && p <= 14; }  // strapping, UART0, USB, PSRAM CS
consteval bool all_distinct(const std::array<int, 7>& a) {
    for (std::size_t i = 0; i < a.size(); ++i)
        for (std::size_t j = i + 1; j < a.size(); ++j)
            if (a[i] >= 0 && a[i] == a[j]) return false;
    return true;
}
consteval bool none_reserved(const std::array<int, 7>& a) {
    for (int p : a) if (reserved(p)) return false;
    return true;
}

static_assert(none_reserved(kLcdPins), "LCD pin on strapping/UART/USB/PSRAM pin");
static_assert(all_distinct(kLcdPins), "two LCD signals on the same GPIO");
static_assert(kSpiHz <= 40'000'000, "max 40 MHz on GPIO-matrix pins");
static_assert((240 * kStripLines * 2) % 4 == 0, "DMA length must be a multiple of 4");

}  // namespace cfg
