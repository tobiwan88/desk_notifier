#include "settings/idf/settings.hpp"

#include <type_traits>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace settings {
namespace {
const char* const kNs = "eyes";
const char* const kTag = "settings";
struct Handle {
    nvs_handle_t h{};
    bool ok;
    explicit Handle(bool rw) : ok(nvs_open(kNs, rw ? NVS_READWRITE : NVS_READONLY, &h) == ESP_OK) {}
    ~Handle() { if (ok) nvs_close(h); }
};
}  // namespace

void init() noexcept {
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(kTag, "erasing NVS");
        nvs_flash_erase();
        e = nvs_flash_init();
    }
    if (e != ESP_OK) ESP_LOGE(kTag, "nvs init failed: %s", esp_err_to_name(e));
}

std::optional<std::string> get(const char* key) noexcept {
    Handle n(false);
    if (!n.ok) return std::nullopt;
    std::size_t len = 0;
    if (nvs_get_str(n.h, key, nullptr, &len) != ESP_OK || len == 0) return std::nullopt;
    std::string s(len, '\0');
    if (nvs_get_str(n.h, key, s.data(), &len) != ESP_OK) return std::nullopt;
    s.resize(len - 1);
    return s;
}

bool set(const char* key, std::string_view value) noexcept {
    Handle n(true);
    if (!n.ok) return false;
    const std::string v(value);
    return nvs_set_str(n.h, key, v.c_str()) == ESP_OK && nvs_commit(n.h) == ESP_OK;
}

void erase_all() noexcept {
    Handle n(true);
    if (n.ok) { nvs_erase_all(n.h); nvs_commit(n.h); }
}

std::optional<eyes::EyeStyle> load_style() noexcept {
    static_assert(std::is_trivially_copyable_v<eyes::EyeStyle>);
    Handle n(false);
    if (!n.ok) return std::nullopt;
    eyes::EyeStyle s;
    std::size_t len = sizeof s;
    if (nvs_get_blob(n.h, "style", &s, &len) != ESP_OK || len != sizeof s) return std::nullopt;
    return s;
}

bool save_style(const eyes::EyeStyle& s) noexcept {
    Handle n(true);
    return n.ok && nvs_set_blob(n.h, "style", &s, sizeof s) == ESP_OK && nvs_commit(n.h) == ESP_OK;
}

}  // namespace settings
