#include "display/idf/display.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include "config.hpp"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_commands.h"
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

esp_lcd_panel_io_handle_t g_io = nullptr;
esp_lcd_panel_handle_t g_panel = nullptr;
SemaphoreHandle_t g_free = nullptr;  // counts free strip buffers, given from the ISR
std::array<std::uint16_t*, cfg::kStripCount> g_bufs{};
std::size_t g_head = 0;  // next buffer to hand out; returns are FIFO
int g_hz = 0;

extern "C" IRAM_ATTR bool on_color_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* ctx) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(ctx), &woken);
    return woken == pdTRUE;
}

#ifdef CONFIG_DESK_LCD_MIRROR_X
constexpr bool kMirrorX = true;
#else
constexpr bool kMirrorX = false;
#endif
#ifdef CONFIG_DESK_LCD_MIRROR_Y
constexpr bool kMirrorY = true;
#else
constexpr bool kMirrorY = false;
#endif

// GC9A01 needs up to 120 ms after RESX/SWRESET (reset cancel) before it accepts SLPOUT and register writes.
constexpr std::uint32_t kResetCancelMs = 120;

std::expected<void, engine::Err> create_panel(int hz) noexcept {
    esp_lcd_panel_io_spi_config_t io = {};
    io.cs_gpio_num = static_cast<gpio_num_t>(cfg::kLcdPins[4]);
    io.dc_gpio_num = static_cast<gpio_num_t>(cfg::kLcdPins[2]);
    io.spi_mode = 0;
    io.pclk_hz = static_cast<unsigned>(hz);
    io.trans_queue_depth = cfg::kStripCount;
    io.lcd_cmd_bits = 8;
    io.lcd_param_bits = 8;
    io.on_color_trans_done = on_color_done;
    io.user_ctx = g_free;
    if (esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(kHost), &io, &g_io) != ESP_OK)
        return std::unexpected(engine::Err::Io);

    esp_lcd_panel_dev_config_t pc = {};
    pc.reset_gpio_num = static_cast<gpio_num_t>(cfg::kLcdPins[3]);
    pc.rgb_ele_order = CONFIG_DESK_LCD_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB;
    pc.bits_per_pixel = 16;
    if (esp_lcd_new_panel_gc9a01(g_io, &pc, &g_panel) != ESP_OK) return std::unexpected(engine::Err::Io);
    g_hz = hz;
    return {};
}

void destroy_panel() noexcept {
    if (g_panel) esp_lcd_panel_del(g_panel);
    if (g_io) esp_lcd_panel_io_del(g_io);
    g_panel = nullptr;
    g_io = nullptr;
}

std::expected<void, engine::Err> bring_up() noexcept {
    esp_lcd_panel_reset(g_panel);  // RST pulse
    vTaskDelay(pdMS_TO_TICKS(kResetCancelMs));
    if (esp_lcd_panel_io_tx_param(g_io, LCD_CMD_SWRESET, nullptr, 0) != ESP_OK) return std::unexpected(engine::Err::Io);
    vTaskDelay(pdMS_TO_TICKS(kResetCancelMs));
    if (esp_lcd_panel_init(g_panel) != ESP_OK) return std::unexpected(engine::Err::Io);
    esp_lcd_panel_invert_color(g_panel, CONFIG_DESK_LCD_INVERT);
    esp_lcd_panel_mirror(g_panel, kMirrorX, kMirrorY);
    (void)fill(0);  // blank before DISPON
    esp_lcd_panel_disp_on_off(g_panel, true);
    ESP_LOGI(kTag, "panel up, spi=%d Hz", g_hz);
    return {};
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

    if (auto r = create_panel(cfg::kSpiHz); !r) return r;
    return bring_up();
}

std::expected<void, engine::Err> reinit(int hz) noexcept {
    if (auto w = wait_idle(200); !w) return w;
    destroy_panel();
    if (auto r = create_panel(hz > 0 ? hz : g_hz); !r) return r;
    return bring_up();
}

int spi_hz() noexcept { return g_hz; }

void set_backlight(std::uint8_t) noexcept {}

std::expected<StripBuf, engine::Err> acquire(std::uint32_t timeout_ms) noexcept {
    if (xSemaphoreTake(g_free, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) return std::unexpected(engine::Err::DisplayStall);
    const auto i = g_head;
    g_head = (g_head + 1) % cfg::kStripCount;
    return StripBuf{{g_bufs[i], kStripBytes / 2}, static_cast<std::uint8_t>(i)};
}

std::expected<void, engine::Err> push(StripBuf&& b, gfx::Rect r) noexcept {
    const StripBuf moved = b;  // `b` is consumed: caller must not reuse it
    b.px = {};
    if (esp_lcd_panel_draw_bitmap(g_panel, r.x, r.y, r.x + r.w, r.y + r.h, moved.px.data()) != ESP_OK) {
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

std::expected<void, engine::Err> fill(gfx::Rgb565be color) noexcept {
    for (int y = 0; y < gfx::kScreen; y += static_cast<int>(cfg::kStripLines)) {
        auto b = acquire(200);
        if (!b) return std::unexpected(b.error());
        for (auto& px : b->px) px = color;
        if (auto r = push(std::move(*b), gfx::Rect{0, static_cast<std::int16_t>(y), gfx::kScreen, static_cast<std::int16_t>(cfg::kStripLines)}); !r) return r;
    }
    return wait_idle(200);
}

std::expected<void, engine::Err> blit(std::span<const gfx::Rgb565be> frame) noexcept {
    constexpr int kLines = static_cast<int>(cfg::kStripLines);
    if (frame.size() < static_cast<std::size_t>(gfx::kScreen) * gfx::kScreen) return std::unexpected(engine::Err::Io);
    for (int y = 0; y < gfx::kScreen; y += kLines) {
        const int h = std::min(kLines, gfx::kScreen - y);
        auto b = acquire(200);
        if (!b) return std::unexpected(b.error());
        std::memcpy(b->px.data(), frame.data() + static_cast<std::size_t>(y) * gfx::kScreen,
                    static_cast<std::size_t>(h) * gfx::kScreen * 2);
        if (auto r = push(std::move(*b), gfx::Rect{0, static_cast<std::int16_t>(y), gfx::kScreen, static_cast<std::int16_t>(h)}); !r) return r;
    }
    return wait_idle(200);
}

}  // namespace display
