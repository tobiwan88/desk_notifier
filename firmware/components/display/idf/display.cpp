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
SemaphoreHandle_t g_free = nullptr;  // counts free strip buffers, given from the ISR
std::array<std::uint16_t*, cfg::kStripCount> g_bufs{};
std::size_t g_head = 0;  // next buffer to hand out; returns are FIFO (see wait_idle at display switch)

extern "C" IRAM_ATTR bool on_color_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* ctx) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(ctx), &woken);
    return woken == pdTRUE;
}

}  // namespace

std::expected<void, engine::Err> init() noexcept {
    g_free = xSemaphoreCreateCounting(cfg::kStripCount, cfg::kStripCount);
    if (!g_free) return std::unexpected(engine::Err::Io);
    for (auto& b : g_bufs) {
        b = static_cast<std::uint16_t*>(heap_caps_malloc(kStripBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
        if (!b) return std::unexpected(engine::Err::Io);
    }

    spi_bus_config_t bus = {};
    bus.mosi_io_num = cfg::kLcdPins[1];
    bus.miso_io_num = -1;
    bus.sclk_io_num = cfg::kLcdPins[0];
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = static_cast<int>(kStripBytes);
    if (spi_bus_initialize(kHost, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return std::unexpected(engine::Err::Io);

    const std::array<int, engine::kEyes> cs = {cfg::kLcdPins[4], cfg::kLcdPins[5]};
    for (std::size_t i = 0; i < engine::kEyes; ++i) {
        esp_lcd_panel_io_spi_config_t io = {};
        io.cs_gpio_num = static_cast<gpio_num_t>(cs[i]);
        io.dc_gpio_num = static_cast<gpio_num_t>(cfg::kLcdPins[2]);
        io.spi_mode = 0;
        io.pclk_hz = cfg::kSpiHz;
        io.trans_queue_depth = cfg::kStripCount;
        io.lcd_cmd_bits = 8;
        io.lcd_param_bits = 8;
        io.on_color_trans_done = on_color_done;
        io.user_ctx = g_free;
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

std::expected<StripBuf, engine::Err> acquire(std::uint32_t timeout_ms) noexcept {
    if (xSemaphoreTake(g_free, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) return std::unexpected(engine::Err::DisplayStall);
    const auto i = g_head;
    g_head = (g_head + 1) % cfg::kStripCount;
    return StripBuf{{g_bufs[i], kStripBytes / 2}, static_cast<std::uint8_t>(i)};
}

std::expected<void, engine::Err> push(engine::Eye eye, StripBuf&& b, gfx::Rect r) noexcept {
    const StripBuf moved = b;  // `b` is consumed: caller must not reuse it
    b.px = {};
    if (esp_lcd_panel_draw_bitmap(g_panel[static_cast<std::size_t>(eye)], r.x, r.y, r.x + r.w, r.y + r.h,
                                  moved.px.data()) != ESP_OK) {
        xSemaphoreGive(g_free);  // never queued: return the buffer
        return std::unexpected(engine::Err::Io);
    }
    return {};
}

std::expected<void, engine::Err> wait_idle(std::uint32_t timeout_ms) noexcept {
    std::size_t got = 0;
    std::expected<void, engine::Err> res{};
    for (; got < cfg::kStripCount; ++got) {
        if (xSemaphoreTake(g_free, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) { res = std::unexpected(engine::Err::DisplayStall); break; }
    }
    for (std::size_t i = 0; i < got; ++i) xSemaphoreGive(g_free);
    return res;
}

std::expected<void, engine::Err> fill(engine::Eye eye, gfx::Rgb565be color) noexcept {
    for (int y = 0; y < gfx::kScreen; y += static_cast<int>(cfg::kStripLines)) {
        auto b = acquire(200);
        if (!b) return std::unexpected(b.error());
        for (auto& px : b->px) px = color;
        if (auto r = push(eye, std::move(*b), gfx::Rect{0, static_cast<std::int16_t>(y), gfx::kScreen, static_cast<std::int16_t>(cfg::kStripLines)}); !r) return r;
    }
    return wait_idle(200);
}

}  // namespace display
