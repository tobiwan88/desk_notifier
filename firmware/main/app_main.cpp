#include "display/idf/display.hpp"
#include "engine/idf/render_task.hpp"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net/idf/net.hpp"
#include "settings/idf/settings.hpp"
#include "storage/idf/storage.hpp"
#include "web/idf/web.hpp"

static const char* const kTag = "eyes";

// After an OTA update the new image must prove itself, or the bootloader rolls back.
static void selftest(void*) {
    esp_ota_img_states_t st;
    const esp_partition_t* run = esp_ota_get_running_partition();
    if (esp_ota_get_state_partition(run, &st) != ESP_OK || st != ESP_OTA_IMG_PENDING_VERIFY) vTaskDelete(nullptr);
    vTaskDelay(pdMS_TO_TICKS(30000));
    const auto s = engine::stats();
    const bool render_ok = s.fps_x10 >= 50;  // >= 5 fps incl. idle frames
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

extern "C" void app_main(void) {
    settings::init();
    if (!storage::init()) ESP_LOGE(kTag, "storage init failed");
    if (auto r = display::init(); !r) { ESP_LOGE(kTag, "display init failed"); return; }
    if (auto r = engine::start_render_task(); !r) { ESP_LOGE(kTag, "render task failed"); return; }
    // Eyes run even if networking fails.
    if (!net::init()) ESP_LOGE(kTag, "net init failed");
    if (!net::start_mdns()) ESP_LOGE(kTag, "mdns failed");
    if (!web::start()) ESP_LOGE(kTag, "web failed");
    if (!net::start_console()) ESP_LOGE(kTag, "console failed");
    xTaskCreate(selftest, "selftest", 3072, nullptr, 3, nullptr);
    ESP_LOGI(kTag, "up: host=%s token=%s", net::hostname().c_str(), net::token().c_str());
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        const auto s = engine::stats();
        ESP_LOGI(kTag, "fps=%u.%u avg=%luus max=%luus heap=%lu", s.fps_x10 / 10, s.fps_x10 % 10,
                 static_cast<unsigned long>(s.frame_us_avg), static_cast<unsigned long>(s.frame_us_max),
                 static_cast<unsigned long>(esp_get_free_heap_size()));
    }
}
