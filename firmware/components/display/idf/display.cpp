#include "display/idf/display.hpp"

#include <array>

#include "config.hpp"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace display {
namespace {

const char* const kTag = "display";
constexpr spi_host_device_t kHost = SPI2_HOST;
constexpr std::size_t kStripBytes = gfx::kScreen * cfg::kStripLines * 2;

std::array<esp_lcd_panel_io_handle_t, engine::kEyes> g_io{};
std::array<esp_lcd_panel_handle_t, engine::kEyes> g_panel{};
SemaphoreHandle_t g_done = nullptr;
std::uint16_t* g_buf = nullptr;

extern "C" IRAM_ATTR bool on_color_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* ctx) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(ctx), &woken);
    return woken == pdTRUE;
}

}  // namespace

std::expected<void, engine::Err> init() noexcept {
    g_done = xSemaphoreCreateCounting(8, 0);
    g_buf = static_cast<std::uint16_t*>(heap_caps_malloc(kStripBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    if (!g_done || !g_buf) return std::unexpected(engine::Err::Io);

    spi_bus_config_t bus = {};
    bus.mosi_io_num = cfg::kLcdPins[1];
    bus.miso_io_num = -1;
    bus.sclk_io_num = cfg::kLcdPins[0];
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = static_cast<int>(kStripBytes);
    bus.intr_flags = ESP_INTR_FLAG_IRAM;
    if (spi_bus_initialize(kHost, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return std::unexpected(engine::Err::Io);

    const std::array<int, engine::kEyes> cs = {cfg::kLcdPins[4], cfg::kLcdPins[5]};
    for (std::size_t i = 0; i < engine::kEyes; ++i) {
        esp_lcd_panel_io_spi_config_t io = {};
        io.cs_gpio_num = static_cast<gpio_num_t>(cs[i]);
        io.dc_gpio_num = static_cast<gpio_num_t>(cfg::kLcdPins[2]);
        io.spi_mode = 0;
        io.pclk_hz = cfg::kSpiHz;
        io.trans_queue_depth = 4;
        io.lcd_cmd_bits = 8;
        io.lcd_param_bits = 8;
        io.on_color_trans_done = on_color_done;
        io.user_ctx = g_done;
        if (esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(kHost), &io, &g_io[i]) != ESP_OK)
            return std::unexpected(engine::Err::Io);

        esp_lcd_panel_dev_config_t pc = {};
        pc.reset_gpio_num = (i == 0) ? static_cast<gpio_num_t>(cfg::kLcdPins[3]) : GPIO_NUM_NC;   // one reset pulse serves both panels
        pc.rgb_ele_order = CONFIG_EYES_LCD_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB;
        pc.bits_per_pixel = 16;
        if (esp_lcd_new_panel_gc9a01(g_io[i], &pc, &g_panel[i]) != ESP_OK) return std::unexpected(engine::Err::Io);
    }
    for (std::size_t i = 0; i < engine::kEyes; ++i) {
        esp_lcd_panel_reset(g_panel[i]);
        esp_lcd_panel_init(g_panel[i]);
        esp_lcd_panel_invert_color(g_panel[i], CONFIG_EYES_LCD_INVERT);
    }
    // blank before DISPON: the backlight is always on
    for (std::size_t i = 0; i < engine::kEyes; ++i) (void)fill(static_cast<engine::Eye>(i), 0);
    for (std::size_t i = 0; i < engine::kEyes; ++i) esp_lcd_panel_disp_on_off(g_panel[i], true);
    ESP_LOGI(kTag, "2 panels up, spi=%d Hz", cfg::kSpiHz);
    return {};
}

void set_backlight(std::uint8_t) noexcept {}

std::expected<void, engine::Err> fill(engine::Eye eye, gfx::Rgb565be color) noexcept {
    const auto idx = static_cast<std::size_t>(eye);
    for (std::size_t i = 0; i < kStripBytes / 2; ++i) g_buf[i] = color;
    for (int y = 0; y < gfx::kScreen; y += static_cast<int>(cfg::kStripLines)) {
        if (esp_lcd_panel_draw_bitmap(g_panel[idx], 0, y, gfx::kScreen, y + static_cast<int>(cfg::kStripLines), g_buf) != ESP_OK)
            return std::unexpected(engine::Err::Io);
        if (xSemaphoreTake(g_done, pdMS_TO_TICKS(200)) != pdTRUE) return std::unexpected(engine::Err::DisplayStall);
    }
    return {};
}

}  // namespace display
