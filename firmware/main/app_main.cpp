#include "display/idf/display.hpp"
#include "engine/idf/render_task.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net/idf/net.hpp"
#include "settings/idf/settings.hpp"
#include "storage/idf/storage.hpp"
#include "web/idf/web.hpp"

static const char* const kTag = "eyes";

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
    ESP_LOGI(kTag, "up: host=%s token=%s", net::hostname().c_str(), net::token().c_str());
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        const auto s = engine::stats();
        ESP_LOGI(kTag, "fps=%u.%u avg=%luus max=%luus heap=%lu", s.fps_x10 / 10, s.fps_x10 % 10,
                 static_cast<unsigned long>(s.frame_us_avg), static_cast<unsigned long>(s.frame_us_max),
                 static_cast<unsigned long>(esp_get_free_heap_size()));
    }
}
