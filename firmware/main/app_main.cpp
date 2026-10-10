#include "display/idf/display.hpp"
#include "engine/idf/render_task.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* const kTag = "eyes";

extern "C" void app_main(void) {
    if (auto r = display::init(); !r) { ESP_LOGE(kTag, "display init failed"); return; }
    if (auto r = engine::start_render_task(); !r) { ESP_LOGE(kTag, "render task failed"); return; }
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        const auto s = engine::stats();
        ESP_LOGI(kTag, "fps=%u.%u avg=%luus max=%luus heap=%lu", s.fps_x10 / 10, s.fps_x10 % 10,
                 static_cast<unsigned long>(s.frame_us_avg), static_cast<unsigned long>(s.frame_us_max),
                 static_cast<unsigned long>(esp_get_free_heap_size()));
    }
}
