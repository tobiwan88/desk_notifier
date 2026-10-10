#include "config.hpp"
#include "display/idf/display.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfx/core/gfx.hpp"

static const char* const kTag = "eyes";

extern "C" void app_main(void) {
    if (auto r = display::init(); !r) { ESP_LOGE(kTag, "display init failed"); return; }
    using gfx::literals::operator""_rgb;
    constexpr auto red = "#ff0000"_rgb, blue = "#0000ff"_rgb, green = "#00ff00"_rgb;
    for (bool swap = false;; swap = !swap) {
        (void)display::fill(engine::Eye::Left, swap ? blue : red);
        (void)display::fill(engine::Eye::Right, swap ? red : blue);
        ESP_LOGI(kTag, "left=%s right=%s", swap ? "blue" : "red", swap ? "red" : "blue");
        vTaskDelay(pdMS_TO_TICKS(1500));
        (void)display::fill(engine::Eye::Left, green);
        (void)display::fill(engine::Eye::Right, green);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
