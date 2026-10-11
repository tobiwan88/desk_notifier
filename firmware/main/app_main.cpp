#include "display/idf/display.hpp"
#include "clock/idf/clock.hpp"
#include "screen/idf/screen_task.hpp"
#include "sensor/idf/bme688.hpp"
#include <string>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net/idf/net.hpp"
#include "settings/idf/settings.hpp"
#include "storage/idf/storage.hpp"
#include "weather/idf/client.hpp"
#include "web/idf/web.hpp"

static const char* const kTag = "desk";

// After an OTA update the new image must prove itself, or the bootloader rolls back.
static void selftest(void*) {
    esp_ota_img_states_t st;
    const esp_partition_t* run = esp_ota_get_running_partition();
    if (esp_ota_get_state_partition(run, &st) != ESP_OK || st != ESP_OTA_IMG_PENDING_VERIFY) vTaskDelete(nullptr);
    vTaskDelay(pdMS_TO_TICKS(30000));
    const bool render_ok = screen::stats().frames >= 5;  // the screen task has drawn at least 5 frames
    const bool net_ok = net::connected() || !net::has_credentials();
    if (render_ok && net_ok) {
        ESP_LOGI(kTag, "self-test passed, marking image valid");
        esp_ota_mark_app_valid_cancel_rollback();
    } else {
        ESP_LOGE(kTag, "self-test failed (render=%d net=%d), rolling back", render_ok, net_ok);
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    vTaskDelete(nullptr);
}

// Keeps the last 8 reset reasons in NVS ("POWERON,SW,BROWNOUT,...", newest last) so a power problem
// (e.g. brown-out on a weak power bank) can be read back later from /api/status or the console.
static void record_reset_reason() {
    const char* name = "OTHER";
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: name = "POWERON"; break;
        case ESP_RST_SW: name = "SW"; break;
        case ESP_RST_PANIC: name = "PANIC"; break;
        case ESP_RST_INT_WDT: name = "INT_WDT"; break;
        case ESP_RST_TASK_WDT: name = "TASK_WDT"; break;
        case ESP_RST_WDT: name = "WDT"; break;
        case ESP_RST_BROWNOUT: name = "BROWNOUT"; break;
        case ESP_RST_DEEPSLEEP: name = "DEEPSLEEP"; break;
        case ESP_RST_USB: name = "USB"; break;
        default: break;
    }
    std::string h = settings::get("rst").value_or("");
    h += h.empty() ? "" : ",";
    h += name;
    int commas = 0;
    for (char c : h) commas += (c == ',');
    while (commas >= 8) { h.erase(0, h.find(',') + 1); --commas; }
    settings::set("rst", h);
}

extern "C" void app_main(void) {
    settings::init();
    record_reset_reason();
#ifdef CONFIG_DESK_TEST_CRASH_ON_BOOT
    vTaskDelay(pdMS_TO_TICKS(1500));
    abort();
#endif
    if (!storage::init()) ESP_LOGE(kTag, "storage init failed");
    if (auto r = display::init(); !r) { ESP_LOGE(kTag, "display init failed"); return; }
    if (auto r = sensor::start(); !r) ESP_LOGE(kTag, "sensor not available, screen shows NO SENSOR");
    if (auto r = screen::start(); !r) { ESP_LOGE(kTag, "screen task failed"); return; }
    // The screen runs even if networking fails.
    if (!net::init()) ESP_LOGE(kTag, "net init failed");
    clock_sync::start();
    if (!weather::start()) ESP_LOGE(kTag, "weather task failed");
    if (!net::start_mdns()) ESP_LOGE(kTag, "mdns failed");
    if (!web::start()) ESP_LOGE(kTag, "web failed");
    if (!net::start_console()) ESP_LOGE(kTag, "console failed");
    xTaskCreate(selftest, "selftest", 3072, nullptr, 3, nullptr);
    ESP_LOGI(kTag, "up: host=%s auth=%s", net::hostname().c_str(), net::open_access() ? "open" : net::token().c_str());
    // app_main returns; the main task is freed (internal RAM is scarce)
}
