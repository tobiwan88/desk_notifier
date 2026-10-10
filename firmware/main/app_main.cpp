#include "config.hpp"
#include "esp_log.h"
#include "gfx/core/gfx.hpp"

static const char* const kTag = "eyes";

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "eyes skeleton: spi=%d Hz, trimmed strip px=%d", cfg::kSpiHz,
             static_cast<int>(gfx::trimmed_pixels(cfg::kStripLines)));
}
