#include "ble/idf/ble.hpp"

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "ble/core/provision.hpp"
#include "clock/idf/clock.hpp"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "net/idf/net.hpp"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "screen/idf/screen_task.hpp"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "settings/idf/settings.hpp"

extern "C" void ble_store_config_init(void);  // NimBLE's bond store (NVS), not in a public header

namespace ble {
namespace {

const char* const kTag = "ble";
constexpr std::uint16_t kMtu = 247;
constexpr std::uint16_t kMaxConfigBytes = 512;
constexpr std::uint32_t kRebootDelayMs = 800;
constexpr std::uint32_t kStatusPollMs = 1000;

// 6e2b0001-3b7c-4f1e-9a52-5d0a6c1e7b30 (service), ...0002 status, ...0003 config, ...0004 scan. Bytes are little endian.
#define DESK_UUID(n) BLE_UUID128_INIT(0x30, 0x7b, 0x1e, 0x6c, 0x0a, 0x5d, 0x52, 0x9a, 0x1e, 0x4f, 0x7c, 0x3b, n, 0x00, 0x2b, 0x6e)
const ble_uuid128_t kSvcUuid = DESK_UUID(0x01);
const ble_uuid128_t kStatusUuid = DESK_UUID(0x02);
const ble_uuid128_t kConfigUuid = DESK_UUID(0x03);
const ble_uuid128_t kScanUuid = DESK_UUID(0x04);

std::mutex g_mx;  // g_result, g_scan, g_guard
std::string g_result = "ready";
std::string g_scan = "[]";
PairGuard g_guard;

std::uint16_t g_status_handle = 0, g_scan_handle = 0;
std::atomic<std::uint16_t> g_conn{BLE_HS_CONN_HANDLE_NONE};
std::atomic<bool> g_sub_status{false}, g_sub_scan{false};
std::atomic<bool> g_synced{false}, g_scan_req{false};
std::atomic<std::uint32_t> g_reboot_at{0};
std::uint8_t g_own_addr = 0;
TaskHandle_t g_worker = nullptr;

std::uint32_t now_ms() { return static_cast<std::uint32_t>(esp_timer_get_time() / 1000); }

void set_result(std::string r) {
    const std::lock_guard<std::mutex> lock(g_mx);
    g_result = std::move(r);
}

std::string status_text() {
    static const std::string version = esp_app_get_description()->version;
    const std::string ssid = net::wifi_ssid(), ip = net::ip(), host = net::hostname(), tz = clock_sync::tz();
    std::string result;
    {
        const std::lock_guard<std::mutex> lock(g_mx);
        result = g_result;
    }
    Status s;
    s.version = version;
    s.wifi = net::connected() ? "connected" : net::has_credentials() ? "connecting" : "none";
    s.ssid = ssid;
    s.ip = ip;
    s.hostname = host;
    s.tz = tz;
    s.result = result;
    s.time_valid = clock_sync::valid();
    s.rssi = net::rssi();
    return status_json(s);
}

int append(os_mbuf* om, const std::string& v) {
    return os_mbuf_append(om, v.data(), static_cast<std::uint16_t>(v.size())) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

void notify(std::uint16_t handle, const std::string& v) {
    const std::uint16_t conn = g_conn.load();
    if (conn == BLE_HS_CONN_HANDLE_NONE) return;
    if (os_mbuf* om = ble_hs_mbuf_from_flat(v.data(), static_cast<std::uint16_t>(v.size()))) ble_gatts_notify_custom(conn, handle, om);
}

// A config write. Wi-Fi and timezone apply at once; a hostname needs a restart (the mDNS name is set at boot).
int handle_config(std::string_view json) {
    Config c;
    const ParseError e = parse_config(json, c);
    if (e != ParseError::None) {
        set_result(error_name(e));
        ESP_LOGW(kTag, "config rejected: %s", error_name(e));
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;  // the reason is in the status characteristic
    }
    std::string result = "ok";
    if (c.ssid) {
        if (!net::apply_wifi(*c.ssid, c.pass.value_or(""))) {
            set_result("wifi failed");
            return BLE_ATT_ERR_UNLIKELY;
        }
        ESP_LOGI(kTag, "wifi: joining \"%s\"", c.ssid->c_str());  // never the password
    }
    if (c.tz) {
        clock_sync::set_tz(*c.tz);
        ESP_LOGI(kTag, "timezone set to %s", c.tz->c_str());
    }
    if (c.hostname) {
        settings::set("hostname", *c.hostname);
        result = "ok, restart to use the new hostname";
        ESP_LOGI(kTag, "hostname set to %s (after restart)", c.hostname->c_str());
    }
    if (c.reboot) {
        result = "restarting";
        g_reboot_at = now_ms() + kRebootDelayMs;
    }
    set_result(result);
    return 0;
}

int status_access(std::uint16_t, std::uint16_t, ble_gatt_access_ctxt* ctxt, void*) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_UNLIKELY;
    return append(ctxt->om, status_text());
}

int config_access(std::uint16_t, std::uint16_t, ble_gatt_access_ctxt* ctxt, void*) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;
    const std::uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len > kMaxConfigBytes) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    std::string buf(len, '\0');
    std::uint16_t got = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, buf.data(), len, &got) != 0 || got != len) return BLE_ATT_ERR_UNLIKELY;
    return handle_config(buf);
}

int scan_access(std::uint16_t, std::uint16_t, ble_gatt_access_ctxt* ctxt, void*) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {  // any value starts a scan; the list follows as a notification
        g_scan_req = true;
        if (g_worker) xTaskNotifyGive(g_worker);
        return 0;
    }
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_UNLIKELY;
    std::string v;
    {
        const std::lock_guard<std::mutex> lock(g_mx);
        v = g_scan;
    }
    return append(ctxt->om, v);
}

constexpr ble_gatt_chr_flags kSecureRead = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_AUTHEN;
constexpr ble_gatt_chr_flags kSecureNotify = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_NOTIFY_INDICATE_AUTHEN;
constexpr ble_gatt_chr_flags kSecureWrite = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_AUTHEN;

const ble_gatt_chr_def kChars[] = {
    {.uuid = &kStatusUuid.u, .access_cb = status_access, .arg = nullptr, .descriptors = nullptr,
     .flags = kSecureRead | kSecureNotify, .min_key_size = 0, .val_handle = &g_status_handle, .cpfd = nullptr},
    {.uuid = &kConfigUuid.u, .access_cb = config_access, .arg = nullptr, .descriptors = nullptr,
     .flags = kSecureWrite, .min_key_size = 0, .val_handle = nullptr, .cpfd = nullptr},
    {.uuid = &kScanUuid.u, .access_cb = scan_access, .arg = nullptr, .descriptors = nullptr,
     .flags = kSecureRead | kSecureWrite | kSecureNotify, .min_key_size = 0, .val_handle = &g_scan_handle, .cpfd = nullptr},
    {},
};

const ble_gatt_svc_def kServices[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &kSvcUuid.u, .includes = nullptr, .characteristics = kChars},
    {},
};

int gap_event(ble_gap_event* ev, void*);

void advertise() {
    if (!g_synced) return;
    {
        const std::lock_guard<std::mutex> lock(g_mx);
        if (g_guard.locked(now_ms())) return;  // too many failed pairings: quiet for a while
    }
    if (ble_gap_adv_active()) return;
    ble_hs_adv_fields adv{};
    adv.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv.uuids128 = &kSvcUuid;
    adv.num_uuids128 = 1;
    adv.uuids128_is_complete = 1;
    if (ble_gap_adv_set_fields(&adv) != 0) return;
    const std::string name = net::hostname().substr(0, 29);
    ble_hs_adv_fields rsp{};
    rsp.name = reinterpret_cast<const std::uint8_t*>(name.c_str());
    rsp.name_len = static_cast<std::uint8_t>(name.size());
    rsp.name_is_complete = 1;
    if (ble_gap_adv_rsp_set_fields(&rsp) != 0) return;
    ble_gap_adv_params p{};
    p.conn_mode = BLE_GAP_CONN_MODE_UND;
    p.disc_mode = BLE_GAP_DISC_MODE_GEN;
    p.itvl_min = BLE_GAP_ADV_ITVL_MS(100);
    p.itvl_max = BLE_GAP_ADV_ITVL_MS(150);
    const int rc = ble_gap_adv_start(g_own_addr, nullptr, BLE_HS_FOREVER, &p, gap_event, nullptr);
    if (rc != 0 && rc != BLE_HS_EALREADY) ESP_LOGE(kTag, "advertising failed: %d", rc);
}

void on_pairing_failed(std::uint16_t conn) {
    bool lock = false;
    {
        const std::lock_guard<std::mutex> lock_g(g_mx);
        g_guard.failed(now_ms());
        lock = g_guard.locked(now_ms());
    }
    if (lock) {
        ESP_LOGW(kTag, "too many failed pairings: no advertising for %lu s", static_cast<unsigned long>(PairGuard::kLockMs / 1000));
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    }
}

int gap_event(ble_gap_event* ev, void*) {
    switch (ev->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (ev->connect.status == 0) {
                g_conn = ev->connect.conn_handle;
                ble_gap_security_initiate(ev->connect.conn_handle);  // the phone then asks for the passkey right away
                ESP_LOGI(kTag, "phone connected");
            } else {
                advertise();
            }
            return 0;
        case BLE_GAP_EVENT_DISCONNECT:
            g_conn = BLE_HS_CONN_HANDLE_NONE;
            g_sub_status = g_sub_scan = false;
            screen::show_pair_code(0);
            ESP_LOGI(kTag, "phone disconnected (reason %d)", ev->disconnect.reason);
            advertise();
            return 0;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            advertise();
            return 0;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (ev->subscribe.attr_handle == g_status_handle) g_sub_status = ev->subscribe.cur_notify;
            if (ev->subscribe.attr_handle == g_scan_handle) g_sub_scan = ev->subscribe.cur_notify;
            return 0;
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(kTag, "mtu %d", ev->mtu.value);
            return 0;
        case BLE_GAP_EVENT_ENC_CHANGE:
            screen::show_pair_code(0);
            if (ev->enc_change.status == 0) {
                const std::lock_guard<std::mutex> lock(g_mx);
                g_guard.succeeded();
                ESP_LOGI(kTag, "link encrypted");
            } else {
                ESP_LOGW(kTag, "pairing failed (status %d)", ev->enc_change.status);
                on_pairing_failed(ev->enc_change.conn_handle);
            }
            return 0;
        case BLE_GAP_EVENT_PASSKEY_ACTION:
            if (ev->passkey.params.action == BLE_SM_IOACT_DISP) {
                ble_sm_io io{};
                io.action = BLE_SM_IOACT_DISP;
                io.passkey = passkey_from(esp_random());
                screen::show_pair_code(io.passkey);  // on the display only, never in the log
                return ble_sm_inject_io(ev->passkey.conn_handle, &io);
            }
            return 0;
        case BLE_GAP_EVENT_REPEAT_PAIRING: {  // the phone forgot its bond: drop ours, the passkey is asked for again
            ble_gap_conn_desc d;
            if (ble_gap_conn_find(ev->repeat_pairing.conn_handle, &d) == 0) ble_store_util_delete_peer(&d.peer_id_addr);
            return BLE_GAP_REPEAT_PAIRING_RETRY;
        }
        default:
            return 0;
    }
}

void on_sync() {
    if (ble_hs_util_ensure_addr(0) != 0 || ble_hs_id_infer_auto(0, &g_own_addr) != 0) {
        ESP_LOGE(kTag, "no Bluetooth address");
        return;
    }
    g_synced = true;
    advertise();
    ESP_LOGI(kTag, "advertising as \"%s\"", net::hostname().c_str());
}

void on_reset(int reason) {
    g_synced = false;
    ESP_LOGW(kTag, "host reset (%d)", reason);
}

void host_task(void*) {
    nimble_port_run();  // returns only when the port is stopped
    nimble_port_freertos_deinit();
}

void run_scan() {
    wifi_scan_config_t sc{};
    sc.show_hidden = false;
    esp_err_t err = esp_wifi_scan_start(&sc, true);
    if (err != ESP_OK && !net::connected()) {  // a connection attempt in progress blocks the scan: stop it, the retry timer resumes
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(300));
        err = esp_wifi_scan_start(&sc, true);
    }
    std::vector<Network> found;
    if (err == ESP_OK) {
        std::uint16_t n = 0;
        esp_wifi_scan_get_ap_num(&n);
        std::vector<wifi_ap_record_t> recs(n > 20 ? 20 : n);
        std::uint16_t got = static_cast<std::uint16_t>(recs.size());
        if (got > 0 && esp_wifi_scan_get_ap_records(&got, recs.data()) == ESP_OK) {
            for (std::uint16_t i = 0; i < got; ++i)
                found.push_back({reinterpret_cast<const char*>(recs[i].ssid), recs[i].rssi, recs[i].authmode != WIFI_AUTH_OPEN});
        }
    } else {
        ESP_LOGW(kTag, "wifi scan failed: %d", err);
    }
    const std::string json = scan_json(found);
    {
        const std::lock_guard<std::mutex> lock(g_mx);
        g_scan = json;
    }
    if (g_sub_scan) notify(g_scan_handle, json);
}

void worker(void*) {
    std::string last;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kStatusPollMs));
        if (g_scan_req.exchange(false)) run_scan();
        const std::uint32_t at = g_reboot_at.load();
        if (at != 0 && static_cast<std::int32_t>(now_ms() - at) >= 0) esp_restart();
        if (g_conn.load() == BLE_HS_CONN_HANDLE_NONE) advertise();  // after a lock-out has run out
        if (g_sub_status) {  // a ping when something changed (the phone then reads the whole status)
            const std::string now = status_text();
            if (now != last) {
                last = now;
                notify(g_status_handle, now);
            }
        } else {
            last.clear();
        }
    }
}

}  // namespace

std::expected<void, engine::Err> start() noexcept {
    if (nimble_port_init() != ESP_OK) return std::unexpected(engine::Err::Io);
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;  // the display shows the passkey, the phone types it
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(net::hostname().substr(0, 29).c_str());
    if (ble_gatts_count_cfg(kServices) != 0 || ble_gatts_add_svcs(kServices) != 0) return std::unexpected(engine::Err::Io);
    ble_att_set_preferred_mtu(kMtu);
    if (xTaskCreate(worker, "ble_work", 4096, nullptr, 3, &g_worker) != pdPASS) return std::unexpected(engine::Err::Io);
    nimble_port_freertos_init(host_task);
    return {};
}

}  // namespace ble
