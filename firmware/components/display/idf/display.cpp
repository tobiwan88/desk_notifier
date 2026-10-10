#include "display/idf/display.hpp"

#include <algorithm>
#include <array>

#include "config.hpp"
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

std::array<esp_lcd_panel_io_handle_t, engine::kEyes> g_io{};
std::array<esp_lcd_panel_handle_t, engine::kEyes> g_panel{};
SemaphoreHandle_t g_free = nullptr;  // counts free strip buffers, given from the ISR
std::array<std::uint16_t*, cfg::kStripCount> g_bufs{};
std::size_t g_head = 0;  // next buffer to hand out; returns are FIFO (see wait_idle at display switch)
int g_hz = 0;
bool g_swap = false;  // diagnostics: Eye::Left on the CS_R panel

extern "C" IRAM_ATTR bool on_color_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* ctx) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(ctx), &woken);
    return woken == pdTRUE;
}

std::size_t panel_of(engine::Eye e) noexcept { return static_cast<std::size_t>(e) ^ (g_swap ? 1u : 0u); }

// GC9A01 (like ST77xx): after RESX/SWRESET applied in sleep-out mode the controller needs up to 120 ms
// ("reset cancel", OTP reload) before it accepts SLPOUT and register writes. esp_lcd's gc9a01 reset waits
// only 10 ms after the pulse, and only panel 0 owns the shared RST line: on a warm reboot (flash, OTA,
// esp_restart) panel 0 got SLPOUT/MADCTL/COLMOD and the vendor gate/timing table inside that window,
// while panel 1 was initialised ~120 ms later. Both panels now run the same, spec-timed sequence.
constexpr std::uint32_t kResetCancelMs = 120;

std::expected<void, engine::Err> create_panels(int hz) noexcept {
    const std::array<int, engine::kEyes> cs = {cfg::kLcdPins[4], cfg::kLcdPins[5]};
    for (std::size_t i = 0; i < engine::kEyes; ++i) {
        esp_lcd_panel_io_spi_config_t io = {};
        io.cs_gpio_num = static_cast<gpio_num_t>(cs[i]);
        io.dc_gpio_num = static_cast<gpio_num_t>(cfg::kLcdPins[2]);
        io.spi_mode = 0;
        io.pclk_hz = static_cast<unsigned>(hz);
        io.trans_queue_depth = cfg::kStripCount;
        io.lcd_cmd_bits = 8;
        io.lcd_param_bits = 8;
        io.on_color_trans_done = on_color_done;
        io.user_ctx = g_free;
        if (esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(kHost), &io, &g_io[i]) != ESP_OK)
            return std::unexpected(engine::Err::Io);

        esp_lcd_panel_dev_config_t pc = {};
        pc.reset_gpio_num = (i == 0) ? static_cast<gpio_num_t>(cfg::kLcdPins[3]) : GPIO_NUM_NC;   // one RST line serves both panels
        pc.rgb_ele_order = CONFIG_EYES_LCD_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB;
        pc.bits_per_pixel = 16;
        if (esp_lcd_new_panel_gc9a01(g_io[i], &pc, &g_panel[i]) != ESP_OK) return std::unexpected(engine::Err::Io);
    }
    g_hz = hz;
    return {};
}

void destroy_panels() noexcept {
    for (std::size_t i = 0; i < engine::kEyes; ++i) {
        if (g_panel[i]) esp_lcd_panel_del(g_panel[i]);
        if (g_io[i]) esp_lcd_panel_io_del(g_io[i]);
        g_panel[i] = nullptr;
        g_io[i] = nullptr;
    }
}

// Identical sequence for both panels: shared RST pulse, SWRESET each, then init, blank, DISPON.
std::expected<void, engine::Err> bring_up() noexcept {
    esp_lcd_panel_reset(g_panel[0]);  // RST pulse (panel 0 owns the shared line: resets both)
    vTaskDelay(pdMS_TO_TICKS(kResetCancelMs));
    for (std::size_t i = 0; i < engine::kEyes; ++i)
        if (esp_lcd_panel_io_tx_param(g_io[i], LCD_CMD_SWRESET, nullptr, 0) != ESP_OK) return std::unexpected(engine::Err::Io);
    vTaskDelay(pdMS_TO_TICKS(kResetCancelMs));
    for (std::size_t i = 0; i < engine::kEyes; ++i) {
        if (esp_lcd_panel_init(g_panel[i]) != ESP_OK) return std::unexpected(engine::Err::Io);
        esp_lcd_panel_invert_color(g_panel[i], CONFIG_EYES_LCD_INVERT);
    }
    // blank before DISPON: the backlight is always on
    for (std::size_t i = 0; i < engine::kEyes; ++i) (void)fill(static_cast<engine::Eye>(i), 0);
    for (std::size_t i = 0; i < engine::kEyes; ++i) esp_lcd_panel_disp_on_off(g_panel[i], true);
    ESP_LOGI(kTag, "2 panels up, spi=%d Hz", g_hz);
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

    if (auto r = create_panels(cfg::kSpiHz); !r) return r;
    return bring_up();
}

std::expected<void, engine::Err> reinit(int hz) noexcept {
    if (auto w = wait_idle(200); !w) return w;
    destroy_panels();
    if (auto r = create_panels(hz > 0 ? hz : g_hz); !r) return r;
    return bring_up();
}

int spi_hz() noexcept { return g_hz; }
void set_swap(bool on) noexcept { g_swap = on; }
bool swapped() noexcept { return g_swap; }

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
    if (esp_lcd_panel_draw_bitmap(g_panel[panel_of(eye)], r.x, r.y, r.x + r.w, r.y + r.h,
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

// 5x7 glyphs drawn 12x scaled in the middle of the test pattern: L / R identify the logical eye.
constexpr std::uint8_t kGlyphL[7] = {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111};
constexpr std::uint8_t kGlyphR[7] = {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001};
bool glyph_px(const std::uint8_t (&g)[7], int x, int y) {  // x,y in panel coordinates
    constexpr int kScale = 12, kX0 = 90, kY0 = 78;
    const int gx = (x - kX0) / kScale, gy = (y - kY0) / kScale;
    if (x < kX0 || y < kY0 || gx >= 5 || gy >= 7) return false;
    return (g[gy] >> (4 - gx)) & 1;
}

// Left panel: RED bands (row markers brighter), right panel: BLUE bands. Any blue on the left display
// (or red on the right one) means the other panel's data or CS leaked over.
std::expected<void, engine::Err> pattern(engine::Eye eye) noexcept {
    constexpr int kLines = static_cast<int>(cfg::kStripLines);
    const bool left = eye == engine::Eye::Left;
    auto tone = [&](int v) { return left ? gfx::rgb565_rt(static_cast<std::uint8_t>(v), 0, 0) : gfx::rgb565_rt(0, 0, static_cast<std::uint8_t>(v)); };
    for (int y = 0; y < gfx::kScreen; y += kLines) {
        const int h = std::min(kLines, gfx::kScreen - y);
        auto b = acquire(200);
        if (!b) return std::unexpected(b.error());
        for (int r = 0; r < h; ++r) {
            const int row = y + r;
            gfx::Rgb565be c = ((row / 10) & 1) ? tone(120) : tone(40);
            if (row % 40 == 0) c = tone(255);
            if (row == 0 || row == gfx::kScreen - 1) c = gfx::rgb565_rt(255, 255, 255);  // top/bottom edge rows: white
            for (int x = 0; x < gfx::kScreen; ++x) b->px[static_cast<std::size_t>(r * gfx::kScreen + x)] = (x == 120 || glyph_px(left ? kGlyphL : kGlyphR, x, row)) ? gfx::rgb565_rt(255, 255, 255) : c;
        }
        if (auto r = push(eye, std::move(*b), gfx::Rect{0, static_cast<std::int16_t>(y), gfx::kScreen, static_cast<std::int16_t>(h)}); !r) return r;
    }
    return wait_idle(200);
}

}  // namespace display
