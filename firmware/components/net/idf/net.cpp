#include "net/idf/net.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "mdns.h"
#include "sdkconfig.h"
#include "settings/idf/settings.hpp"

namespace net {
namespace {

const char* const kTag = "net";
bool g_connected = false;
std::string g_ip;
esp_netif_t* g_netif = nullptr;
TimerHandle_t g_retry = nullptr;
std::uint32_t g_backoff_ms = 1000;

void retry_cb(TimerHandle_t) { esp_wifi_connect(); }

void on_event(void*, esp_event_base_t base, std::int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        g_connected = false;
        g_ip.clear();
        xTimerChangePeriod(g_retry, pdMS_TO_TICKS(g_backoff_ms), 0);
        g_backoff_ms = std::min<std::uint32_t>(g_backoff_ms * 2, 30000);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* e = static_cast<ip_event_got_ip_t*>(data);
        char b[16];
        esp_ip4addr_ntoa(&e->ip_info.ip, b, sizeof b);
        g_ip = b;
        g_connected = true;
        g_backoff_ms = 1000;
        ESP_LOGI(kTag, "connected, ip=%s", b);
    }
}

}  // namespace

std::string hostname() noexcept {
    if (auto h = settings::get("hostname")) return *h;
    std::uint8_t mac[6]{};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char b[20];
    std::snprintf(b, sizeof b, "eyes-%02x%02x%02x", mac[3], mac[4], mac[5]);
    return b;
}

std::string token() noexcept {
    if (auto t = settings::get("token"); t && !t->empty()) return *t;
    char b[33];
    for (int i = 0; i < 4; ++i) std::snprintf(b + i * 8, 9, "%08lx", static_cast<unsigned long>(esp_random()));
    settings::set("token", b);
    ESP_LOGW(kTag, "generated API token: %s", b);
    return b;
}

std::expected<void, engine::Err> init() noexcept {
    if (esp_netif_init() != ESP_OK || esp_event_loop_create_default() != ESP_OK) return std::unexpected(engine::Err::Io);
    g_netif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(g_netif, hostname().c_str());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) return std::unexpected(engine::Err::Io);
    g_retry = xTimerCreate("wifi_retry", pdMS_TO_TICKS(1000), pdFALSE, nullptr, retry_cb);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, nullptr);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);

    std::string ssid = settings::get("ssid").value_or(CONFIG_EYES_WIFI_SSID);
    std::string pass = settings::get("pass").value_or(CONFIG_EYES_WIFI_PASSWORD);
    if (ssid.empty()) {
        ESP_LOGW(kTag, "no Wi-Fi credentials: use the console command `wifi <ssid> <password>`");
        esp_wifi_start();  // started without config: STA_START triggers connect which fails quietly
        return {};
    }
    wifi_config_t wc{};
    std::strncpy(reinterpret_cast<char*>(wc.sta.ssid), ssid.c_str(), sizeof wc.sta.ssid - 1);
    std::strncpy(reinterpret_cast<char*>(wc.sta.password), pass.c_str(), sizeof wc.sta.password - 1);
    wc.sta.threshold.authmode = pass.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    esp_wifi_start();
    return {};
}

std::expected<void, engine::Err> start_mdns() noexcept {
    if (mdns_init() != ESP_OK) return std::unexpected(engine::Err::Io);
    const std::string h = hostname();
    mdns_hostname_set(h.c_str());
    mdns_instance_name_set("eyes");
    mdns_service_add(nullptr, "_eyes", "_tcp", 80, nullptr, 0);
    mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
    ESP_LOGI(kTag, "mDNS: %s.local", h.c_str());
    return {};
}

bool connected() noexcept { return g_connected; }
std::int8_t rssi() noexcept {
    wifi_ap_record_t ap{};
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;
}
std::string ip() noexcept { return g_ip; }

}  // namespace net
