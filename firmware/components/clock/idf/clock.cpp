#include "clock/idf/clock.hpp"
#include <string>

#include <cstdlib>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "sdkconfig.h"
#include "settings/idf/settings.hpp"

namespace clock_sync {

std::string tz() noexcept { return settings::get("tz").value_or(CONFIG_DESK_TZ); }

void set_tz(std::string_view posix_tz) noexcept {
    const std::string v(posix_tz);
    settings::set("tz", v);
    setenv("TZ", v.c_str(), 1);
    tzset();
}

void start() noexcept {
    setenv("TZ", tz().c_str(), 1);
    tzset();
    esp_sntp_config_t c = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    if (esp_netif_sntp_init(&c) != ESP_OK) ESP_LOGE("clock", "sntp init failed");
}

bool valid() noexcept { return std::time(nullptr) > 1'700'000'000; }

bool local(std::tm& out) noexcept {
    if (!valid()) return false;
    const std::time_t t = std::time(nullptr);
    return localtime_r(&t, &out) != nullptr;
}

}  // namespace clock_sync
