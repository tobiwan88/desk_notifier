// UART/USB console (plan §5.6).
#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "engine/core/command.hpp"
#include "engine/idf/render_task.hpp"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "net/idf/net.hpp"
#include "settings/idf/settings.hpp"

namespace net {
namespace {

int cmd_wifi(int argc, char** argv) {
    if (argc == 2 && !std::strcmp(argv[1], "show")) {
        std::printf("ssid=%s connected=%d ip=%s rssi=%d\n", settings::get("ssid").value_or("(kconfig)").c_str(),
                    connected(), ip().c_str(), rssi());
        return 0;
    }
    if (argc < 2) { std::printf("usage: wifi <ssid> [password] | wifi show\n"); return 1; }
    settings::set("ssid", argv[1]);
    settings::set("pass", argc > 2 ? argv[2] : "");
    std::printf("saved; rebooting\n");
    esp_restart();
    return 0;
}
int cmd_token(int argc, char** argv) {
    if (argc == 2 && !std::strcmp(argv[1], "show")) { std::printf("%s\n", token().c_str()); return 0; }
    if (argc == 3 && !std::strcmp(argv[1], "set")) { settings::set("token", argv[2]); std::printf("ok\n"); return 0; }
    if (argc == 2 && !std::strcmp(argv[1], "new")) { settings::set("token", ""); std::printf("%s\n", token().c_str()); return 0; }
    std::printf("usage: token show | set <t> | new\n");
    return 1;
}
int cmd_auth(int argc, char** argv) {
    if (argc == 2 && !std::strcmp(argv[1], "open")) { settings::set("open", "1"); std::printf("anonymous access ON (reboot to apply)\n"); return 0; }
    if (argc == 2 && !std::strcmp(argv[1], "token")) { settings::set("open", "0"); std::printf("token required (reboot to apply)\n"); return 0; }
    std::printf("auth: %s | usage: auth open|token\n", open_access() ? "open" : "token");
    return 0;
}
int cmd_hostname(int argc, char** argv) {
    if (argc != 2) { std::printf("%s\n", hostname().c_str()); return 0; }
    settings::set("hostname", argv[1]);
    std::printf("saved; reboot to apply\n");
    return 0;
}
int cmd_status(int, char**) {
    const auto s = engine::stats();
    std::printf("host=%s ip=%s rssi=%d fps=%u.%u frame_avg=%luus src=%.*s/%.*s heap=%lu\n", hostname().c_str(), ip().c_str(),
                rssi(), s.fps_x10 / 10, s.fps_x10 % 10, static_cast<unsigned long>(s.frame_us_avg),
                static_cast<int>(s.source[0].size()), s.source[0].data(), static_cast<int>(s.source[1].size()), s.source[1].data(),
                static_cast<unsigned long>(esp_get_free_heap_size()));
    return 0;
}
int cmd_eye(int argc, char** argv) {  // eye <animation|blink|random>
    if (argc != 2) { std::printf("usage: eye <animation|blink|random>\n"); return 1; }
    engine::EyeCmd c;
    const std::string_view n = argv[1];
    if (n == "blink") c.play = engine::kPlayBlink;
    else if (n == "random") c.play = engine::kPlayRandom;
    else {
        std::size_t i = 0;
        while (i < eyes::kAnimNames.size() && eyes::kAnimNames[i] != n) ++i;
        if (i == eyes::kAnimNames.size()) { std::printf("unknown animation\n"); return 1; }
        c.play = static_cast<std::uint8_t>(i);
    }
    return engine::post(engine::Command{c}, 100) ? 0 : 1;
}
int cmd_play(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: play <clip> [loop]\n"); return 1; }
    engine::PlayCmd p;
    p.clip[0].assign(argv[1]); p.clip[1].assign(argv[1]);
    p.loop = argc > 2;
    return engine::post(engine::Command{p}, 100) ? 0 : 1;
}
int cmd_stop(int, char**) { return engine::post(engine::Command{engine::StopCmd{}}, 100) ? 0 : 1; }
int cmd_fsbench(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: fsbench <clip>\n"); return 1; }
    char path[96];
    std::snprintf(path, sizeof path, "/fs/clips/%s.clip", argv[1]);
    for (std::size_t chunk : {4096u, 16384u, 32768u}) {
        int fd = open(path, O_RDONLY);
        if (fd < 0) { std::printf("cannot open\n"); return 1; }
        auto* buf = static_cast<char*>(heap_caps_malloc(chunk, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
        if (!buf) { close(fd); std::printf("no memory for %u\n", static_cast<unsigned>(chunk)); continue; }
        const int64_t t0 = esp_timer_get_time();
        std::size_t total = 0;
        for (ssize_t n; (n = read(fd, buf, chunk)) > 0;) total += static_cast<std::size_t>(n);
        const int64_t us = esp_timer_get_time() - t0;
        std::printf("chunk=%u: %u bytes in %lld ms = %u KB/s\n", static_cast<unsigned>(chunk), static_cast<unsigned>(total),
                    static_cast<long long>(us / 1000), static_cast<unsigned>(total * 1000000ULL / (us ? us : 1) / 1024));
        heap_caps_free(buf);
        close(fd);
    }
    return 0;
}
int cmd_reboot(int, char**) { esp_restart(); return 0; }
int cmd_factory(int, char**) { settings::erase_all(); std::printf("NVS erased (clips kept); rebooting\n"); esp_restart(); return 0; }

}  // namespace

std::expected<void, engine::Err> start_console() noexcept {
    esp_console_repl_t* repl = nullptr;
    esp_console_repl_config_t rc = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    rc.prompt = "eyes> ";
    rc.task_stack_size = 4096;
    rc.task_priority = 2;
    esp_console_dev_usb_serial_jtag_config_t dc = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    if (esp_console_new_repl_usb_serial_jtag(&dc, &rc, &repl) != ESP_OK) return std::unexpected(engine::Err::Io);
    esp_console_register_help_command();
    const esp_console_cmd_t cmds[] = {
        {.command = "wifi", .help = "wifi <ssid> [password] | wifi show", .hint = nullptr, .func = cmd_wifi, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr},
        {.command = "token", .help = "token show | set <t> | new", .hint = nullptr, .func = cmd_token, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr},
        {.command = "auth", .help = "auth open|token", .hint = nullptr, .func = cmd_auth, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr},
        {.command = "hostname", .help = "hostname [name]", .hint = nullptr, .func = cmd_hostname, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr},
        {.command = "status", .help = "show status", .hint = nullptr, .func = cmd_status, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr},
        {.command = "eye", .help = "eye <animation|blink|random>", .hint = nullptr, .func = cmd_eye, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr},
        {.command = "play", .help = "play <clip> [loop]", .hint = nullptr, .func = cmd_play, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr},
        {.command = "stop", .help = "stop clips", .hint = nullptr, .func = cmd_stop, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr},
        {.command = "fsbench", .help = "fsbench <clip>", .hint = nullptr, .func = cmd_fsbench, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr},
        {.command = "reboot", .help = "reboot", .hint = nullptr, .func = cmd_reboot, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr},
        {.command = "factory-reset", .help = "erase NVS settings", .hint = nullptr, .func = cmd_factory, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr},
    };
    for (const auto& c : cmds) esp_console_cmd_register(&c);
    if (esp_console_start_repl(repl) != ESP_OK) return std::unexpected(engine::Err::Io);
    return {};
}

}  // namespace net
