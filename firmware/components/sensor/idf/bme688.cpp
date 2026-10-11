#include "sensor/idf/bme688.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <string>

#include "config.hpp"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {
#include "bme68x.h"
}

namespace sensor {
namespace {

const char* const kTag = "bme688";
constexpr std::uint32_t kPeriodMs = 3000;

i2c_master_bus_handle_t g_bus = nullptr;
i2c_master_dev_handle_t g_dev = nullptr;
bme68x_dev g_bme{};
Tracker g_tracker;

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
Snapshot g_snap{};
std::int64_t g_last_ok_us = 0;

BME68X_INTF_RET_TYPE i2c_read(std::uint8_t reg, std::uint8_t* data, std::uint32_t len, void*) {
    return i2c_master_transmit_receive(g_dev, &reg, 1, data, len, 200) == ESP_OK ? BME68X_INTF_RET_SUCCESS : -1;
}

BME68X_INTF_RET_TYPE i2c_write(std::uint8_t reg, const std::uint8_t* data, std::uint32_t len, void*) {
    std::array<std::uint8_t, 32> buf;
    if (len + 1 > buf.size()) return -1;
    buf[0] = reg;
    std::memcpy(buf.data() + 1, data, len);
    return i2c_master_transmit(g_dev, buf.data(), len + 1, 200) == ESP_OK ? BME68X_INTF_RET_SUCCESS : -1;
}

void delay_us(std::uint32_t us, void*) {
    if (us >= 2000) vTaskDelay(pdMS_TO_TICKS((us + 999) / 1000));
    else esp_rom_delay_us(us);
}

bool configure() {
    bme68x_conf conf{};
    conf.os_hum = BME68X_OS_2X;
    conf.os_pres = BME68X_OS_4X;
    conf.os_temp = BME68X_OS_8X;
    conf.filter = BME68X_FILTER_OFF;
    conf.odr = BME68X_ODR_NONE;
    if (bme68x_set_conf(&conf, &g_bme) != BME68X_OK) return false;
    bme68x_heatr_conf heat{};
    heat.enable = BME68X_ENABLE;
    heat.heatr_temp = 320;
    heat.heatr_dur = 150;
    return bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heat, &g_bme) == BME68X_OK;
}

bool sample(Reading& out) {
    if (bme68x_set_op_mode(BME68X_FORCED_MODE, &g_bme) != BME68X_OK) return false;
    bme68x_conf conf{};
    (void)bme68x_get_conf(&conf, &g_bme);
    const std::uint32_t dur_us = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &g_bme) + 150u * 1000u;
    delay_us(dur_us + 5000, nullptr);
    bme68x_data d{};
    std::uint8_t n = 0;
    if (bme68x_get_data(BME68X_FORCED_MODE, &d, &n, &g_bme) != BME68X_OK || n == 0) return false;
    if (!(d.status & BME68X_NEW_DATA_MSK)) return false;
    out.temp_c = d.temperature;
    out.rh = d.humidity;
    out.hpa = d.pressure / 100.0f;
    out.gas_ohm = d.gas_resistance;
    out.gas_valid = (d.status & BME68X_GASM_VALID_MSK) && (d.status & BME68X_HEAT_STAB_MSK);
    return true;
}

void task(void*) {
    int errors = 0;
    for (;;) {
        Reading r;
        const bool ok = sample(r);
        taskENTER_CRITICAL(&g_mux);
        if (ok) {
            g_snap.reading = r;
            g_snap.quality = g_tracker.update(r);
            g_snap.ok = true;
            g_snap.errors = errors = 0;
            g_last_ok_us = esp_timer_get_time();
        } else {
            g_snap.errors = ++errors;
        }
        taskEXIT_CRITICAL(&g_mux);
        if (ok) ESP_LOGI(kTag, "%.2f C  %.1f %%  %.1f hPa  gas %.0f ohm%s  score %d", static_cast<double>(r.temp_c),
                         static_cast<double>(r.rh), static_cast<double>(r.hpa), static_cast<double>(r.gas_ohm),
                         r.gas_valid ? "" : " (unstable)", g_snap.quality.score);
        else ESP_LOGW(kTag, "read failed (%d in a row)", errors);
        vTaskDelay(pdMS_TO_TICKS(kPeriodMs));
    }

}  // namespace

std::string g_scan;  // addresses that ACKed on the bus (diagnostics)

bool attach(int addr) {
    if (g_dev) { i2c_master_bus_rm_device(g_dev); g_dev = nullptr; }
    i2c_device_config_t dc = {};
    dc.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dc.device_address = static_cast<std::uint16_t>(addr);
    dc.scl_speed_hz = 100000;
    if (i2c_master_bus_add_device(g_bus, &dc, &g_dev) != ESP_OK) return false;
    g_bme = {};
    g_bme.intf = BME68X_I2C_INTF;
    g_bme.read = i2c_read;
    g_bme.write = i2c_write;
    g_bme.delay_us = delay_us;
    g_bme.intf_ptr = nullptr;
    g_bme.amb_temp = 22;
    return bme68x_init(&g_bme) == BME68X_OK && configure();
}

void scan() {
    g_scan.clear();
    for (int a = 0x08; a < 0x78; ++a) {
        if (i2c_master_probe(g_bus, static_cast<std::uint16_t>(a), 20) == ESP_OK) {
            char b[8];
            std::snprintf(b, sizeof b, "%s0x%02x", g_scan.empty() ? "" : ",", a);
            g_scan += b;
        }
    }
    ESP_LOGI(kTag, "I2C scan (SDA=%d SCL=%d): [%s]", cfg::kI2cSda, cfg::kI2cScl, g_scan.c_str());
}

void init_task(void*) {  // keeps trying until a BME68x answers, then becomes the sampling task
    for (;;) {
        scan();
        for (int addr : {cfg::kBmeAddr, 0x76, 0x77}) {
            if (attach(addr)) {
                ESP_LOGI(kTag, "BME68x up at 0x%02x (variant %d)", addr, g_bme.variant_id);
                task(nullptr);
            }
        }
        ESP_LOGE(kTag, "no BME68x found: check 3V3, GND, SDA=%d, SCL=%d", cfg::kI2cSda, cfg::kI2cScl);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

}  // namespace

std::expected<void, engine::Err> start() noexcept {
    i2c_master_bus_config_t bc = {};
    bc.i2c_port = I2C_NUM_0;
    bc.sda_io_num = static_cast<gpio_num_t>(cfg::kI2cSda);
    bc.scl_io_num = static_cast<gpio_num_t>(cfg::kI2cScl);
    bc.clk_source = I2C_CLK_SRC_DEFAULT;
    bc.glitch_ignore_cnt = 7;
    bc.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&bc, &g_bus) != ESP_OK) return std::unexpected(engine::Err::Io);
    if (xTaskCreate(init_task, "bme688", 4096, nullptr, 3, nullptr) != pdPASS) return std::unexpected(engine::Err::Io);
    return {};
}

std::string i2c_scan() noexcept {
    return g_scan;  // diagnostics only; a torn read is harmless
}

Snapshot latest() noexcept {
    taskENTER_CRITICAL(&g_mux);
    Snapshot s = g_snap;
    const std::int64_t last = g_last_ok_us;
    taskEXIT_CRITICAL(&g_mux);
    s.age_ms = last ? static_cast<std::uint32_t>((esp_timer_get_time() - last) / 1000) : 0;
    if (s.ok && s.age_ms > 4 * kPeriodMs) s.ok = false;
    return s;
}

}  // namespace sensor
