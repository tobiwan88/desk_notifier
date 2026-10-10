#include "storage/idf/storage.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>

#include "clip/core/clip.hpp"
#include "config.hpp"
#include "engine/core/command.hpp"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace storage {
namespace {

const char* const kTag = "storage";
constexpr const char* kDir = "/fs/clips";
std::array<engine::StaticString<31>, 2> g_playing;
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
std::atomic<bool> g_upload_active{false};

bool rd(void* ctx, std::uint32_t off, std::span<std::byte> out) noexcept {
    const int fd = *static_cast<int*>(ctx);
    return lseek(fd, static_cast<off_t>(off), SEEK_SET) == static_cast<off_t>(off) &&
           read(fd, out.data(), out.size()) == static_cast<ssize_t>(out.size());
}

bool tmp_path(std::string_view name, char* out, std::size_t cap) {
    return std::snprintf(out, cap, "%s/.tmp-%.*s", kDir, static_cast<int>(name.size()), name.data()) < static_cast<int>(cap);
}

std::expected<ClipInfo, engine::Err> info_of(std::string_view name, const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return std::unexpected(engine::Err::NotFound);
    const int fd = open(path, O_RDONLY);
    if (fd < 0) return std::unexpected(engine::Err::Io);
    std::array<std::byte, sizeof(clip::Header)> hb{};
    const bool ok = read(fd, hb.data(), hb.size()) == static_cast<ssize_t>(hb.size());
    std::uint32_t total_ms = 0;
    auto h = clip::parse_header(hb);
    if (ok && h) {
        for (std::uint16_t i = 0; i < h->frame_count; ++i) {
            clip::IndexEntry e{};
            if (lseek(fd, static_cast<off_t>(h->index_offset + i * sizeof e), SEEK_SET) < 0 || read(fd, &e, sizeof e) != sizeof e) break;
            total_ms += e.duration_ms;
        }
    }
    close(fd);
    if (!ok || !h) return std::unexpected(engine::Err::BadClip);
    ClipInfo ci{};
    ci.name.assign(name);
    ci.size = static_cast<std::uint32_t>(st.st_size);
    ci.frames = h->frame_count;
    ci.fps_x10 = total_ms ? static_cast<std::uint16_t>(h->frame_count * 10000ULL / total_ms) : 0;
    return ci;
}

}  // namespace

std::expected<void, engine::Err> init() noexcept {
    esp_vfs_littlefs_conf_t conf = {};
    conf.base_path = "/fs";
    conf.partition_label = "storage";
    conf.format_if_mount_failed = true;
    conf.dont_mount = false;
    if (esp_vfs_littlefs_register(&conf) != ESP_OK) return std::unexpected(engine::Err::Io);
    mkdir(kDir, 0775);
    if (DIR* d = opendir(kDir)) {  // remove stale uploads
        while (dirent* e = readdir(d)) {
            if (std::strncmp(e->d_name, ".tmp-", 5) == 0) {
                char p[320];
                std::snprintf(p, sizeof p, "%s/%s", kDir, e->d_name);
                unlink(p);
            }
        }
        closedir(d);
    }
    ESP_LOGI(kTag, "mounted, free=%lu", static_cast<unsigned long>(free_bytes()));
    return {};
}

bool path_for(std::string_view name, char* out, std::size_t cap) noexcept {
    return engine::clip_name_valid(name) &&
           std::snprintf(out, cap, "%s/%.*s.clip", kDir, static_cast<int>(name.size()), name.data()) < static_cast<int>(cap);
}

std::expected<engine::StaticVec<ClipInfo, 64>, engine::Err> list() noexcept {
    engine::StaticVec<ClipInfo, 64> out;
    DIR* d = opendir(kDir);
    if (!d) return std::unexpected(engine::Err::Io);
    while (dirent* e = readdir(d)) {
        std::string_view n = e->d_name;
        if (n.size() < 6 || n.substr(n.size() - 5) != ".clip" || n[0] == '.') continue;
        const std::string_view base = n.substr(0, n.size() - 5);
        char p[96];
        if (!path_for(base, p, sizeof p)) continue;
        if (auto i = info_of(base, p)) (void)out.push_back(*i);
    }
    closedir(d);
    return out;
}

std::expected<ClipInfo, engine::Err> info(std::string_view name) noexcept {
    char p[96];
    if (!path_for(name, p, sizeof p)) return std::unexpected(engine::Err::BadName);
    return info_of(name, p);
}

std::expected<void, engine::Err> remove(std::string_view name) noexcept {
    char p[96];
    if (!path_for(name, p, sizeof p)) return std::unexpected(engine::Err::BadName);
    if (is_playing(name)) return std::unexpected(engine::Err::Conflict);
    return unlink(p) == 0 ? std::expected<void, engine::Err>{} : std::unexpected(engine::Err::NotFound);
}

std::uint32_t free_bytes() noexcept {
    std::size_t total = 0, used = 0;
    if (esp_littlefs_info("storage", &total, &used) != ESP_OK) return 0;
    return static_cast<std::uint32_t>(total - used);
}

std::uint32_t max_clip_bytes() noexcept { return CONFIG_EYES_MAX_CLIP_BYTES; }

void mark_playing(int slot, std::string_view name) noexcept {
    taskENTER_CRITICAL(&g_mux);
    g_playing[static_cast<std::size_t>(slot)].assign(name);
    taskEXIT_CRITICAL(&g_mux);
}
void clear_playing(int slot) noexcept { mark_playing(slot, {}); }
bool is_playing(std::string_view name) noexcept {
    taskENTER_CRITICAL(&g_mux);
    bool p = false;
    for (const auto& n : g_playing) p |= (!n.empty() && n.view() == name);
    taskEXIT_CRITICAL(&g_mux);
    return p;
}

// ---- Upload ----
std::expected<Upload, engine::Err> Upload::begin(std::string_view name, std::uint32_t total) noexcept {
    if (!engine::clip_name_valid(name)) return std::unexpected(engine::Err::BadName);
    if (total == 0 || total > max_clip_bytes()) return std::unexpected(engine::Err::TooLarge);
    if (total + 65536 > free_bytes() + (info(name) ? info(name)->size : 0)) return std::unexpected(engine::Err::NoSpace);
    bool expected = false;
    if (!g_upload_active.compare_exchange_strong(expected, true)) return std::unexpected(engine::Err::Busy);
    char p[96];
    tmp_path(name, p, sizeof p);
    Upload u;
    u.fd_ = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (u.fd_ < 0) { g_upload_active = false; return std::unexpected(engine::Err::Io); }
    u.total_ = total;
    u.name_.assign(name);
    u.active_ = true;
    return u;
}

Upload::Upload(Upload&& o) noexcept
    : fd_(o.fd_), total_(o.total_), written_(o.written_), name_(o.name_), active_(o.active_) {
    o.active_ = false;
    o.fd_ = -1;
}

Upload::~Upload() {
    if (!active_) return;
    if (fd_ >= 0) close(fd_);
    char p[96];
    tmp_path(name_.view(), p, sizeof p);
    unlink(p);
    g_upload_active = false;
}

std::expected<void, engine::Err> Upload::write(std::span<const std::byte> data) noexcept {
    if (written_ + data.size() > total_) return std::unexpected(engine::Err::TooLarge);
    if (::write(fd_, data.data(), data.size()) != static_cast<ssize_t>(data.size())) return std::unexpected(engine::Err::NoSpace);
    written_ += static_cast<std::uint32_t>(data.size());
    return {};
}

std::expected<ClipInfo, engine::Err> Upload::commit() noexcept {
    if (written_ != total_) return std::unexpected(engine::Err::BadClip);
    close(fd_);
    fd_ = -1;
    char tp[96], fp[96];
    tmp_path(name_.view(), tp, sizeof tp);
    if (!path_for(name_.view(), fp, sizeof fp)) return std::unexpected(engine::Err::BadName);
    int rfd = open(tp, O_RDONLY);
    if (rfd < 0) return std::unexpected(engine::Err::Io);
    auto v = clip::validate(clip::Reader{rd, &rfd}, total_);
    close(rfd);
    if (!v) return std::unexpected(v.error());
    if (is_playing(name_.view())) return std::unexpected(engine::Err::Conflict);
    if (rename(tp, fp) != 0) return std::unexpected(engine::Err::Io);
    active_ = false;  // renamed: nothing to unlink
    g_upload_active = false;
    return info_of(name_.view(), fp);
}

}  // namespace storage
