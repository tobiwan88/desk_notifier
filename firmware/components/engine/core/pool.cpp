#include "engine/core/pool.hpp"

#include <cmath>
#include <cstdio>

#include "cJSON.h"
#include "engine/core/parse.hpp"
#include "eyes/core/cat.hpp"

namespace engine {
namespace {
// relative frequency of each built-in animation (same as the cat_eyes idle scheduler, x10) and of a clip
constexpr int kAnimWeight[12] = {20, 20, 15, 15, 12, 6, 10, 8, 6, 15, 30, 14};
constexpr int kClipWeight = 20;

std::uint32_t next(std::uint32_t& s) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}
int anim_id(std::string_view n) {
    for (std::size_t i = 0; i < eyes::kAnimNames.size(); ++i)
        if (eyes::kAnimNames[i] == n) return static_cast<int>(i);
    return -1;
}
}  // namespace

bool operator==(const Pool& a, const Pool& b) noexcept {
    if (a.anim_mask != b.anim_mask || a.min_ds != b.min_ds || a.max_ds != b.max_ds || a.clips.size() != b.clips.size()) return false;
    for (std::size_t i = 0; i < a.clips.size(); ++i)
        if (a.clips[i].view() != b.clips[i].view()) return false;
    return true;
}

Pick pick(const Pool& p, std::uint32_t& rng, int last, bool has_special) noexcept {
    int total = 0;
    auto usable = [&](int key, int w) { return key != last && w > 0 ? w : 0; };
    for (int i = 0; i < 12; ++i)
        if ((p.anim_mask >> i) & 1 && (i != 11 || has_special)) total += usable(i, kAnimWeight[i]);
    for (std::size_t c = 0; c < p.clips.size(); ++c) total += usable(64 + static_cast<int>(c), kClipWeight);
    if (total == 0 && last >= 0) return pick(p, rng, -1, has_special);  // only one item enabled: repeat it
    if (total == 0) return {};
    int r = static_cast<int>(next(rng) % static_cast<std::uint32_t>(total));
    for (int i = 0; i < 12; ++i) {
        if (!((p.anim_mask >> i) & 1) || (i == 11 && !has_special)) continue;
        r -= usable(i, kAnimWeight[i]);
        if (r < 0 && usable(i, kAnimWeight[i])) return {Pick::Kind::Anim, i};
    }
    for (std::size_t c = 0; c < p.clips.size(); ++c) {
        const int w = usable(64 + static_cast<int>(c), kClipWeight);
        r -= w;
        if (r < 0 && w) return {Pick::Kind::Clip, static_cast<int>(c)};
    }
    return {};
}

std::expected<PoolPatch, ParseError> parse_pool(std::string_view json) noexcept {
    auto E = [](Err c, std::string_view m) { return std::unexpected(ParseError{c, StaticString<63>(m)}); };
    if (json.size() > kMaxBody) return E(Err::TooLarge, "body over 1 KB");
    cJSON* root = cJSON_ParseWithLength(json.data(), json.size());
    struct Guard { cJSON* r; ~Guard() { cJSON_Delete(r); } } g{root};
    if (!root || !cJSON_IsObject(root)) return E(Err::BadJson, "invalid JSON object");
    PoolPatch out;
    for (const cJSON* c = root->child; c; c = c->next) {
        const std::string_view k = c->string ? c->string : "";
        if (k != "enabled" && k != "interval") return E(Err::BadJson, "unknown field");
    }
    if (const cJSON* en = cJSON_GetObjectItemCaseSensitive(root, "enabled")) {
        if (!cJSON_IsArray(en)) return E(Err::BadJson, "enabled must be a list of names");
        out.has_enabled = true;
        for (const cJSON* it = en->child; it; it = it->next) {
            if (!cJSON_IsString(it)) return E(Err::BadJson, "enabled must be a list of names");
            const std::string_view n = it->valuestring;
            if (const int a = anim_id(n); a >= 0) { out.anim_mask |= static_cast<std::uint16_t>(1u << a); continue; }
            if (!clip_name_valid(n)) return E(Err::BadName, "invalid item name");
            bool dup = false;
            for (const auto& c : out.clips) dup |= (c.view() == n);
            if (!dup && !out.clips.push_back(StaticString<31>(n))) return E(Err::TooLarge, "too many clips in the pool");
        }
    }
    if (const cJSON* iv = cJSON_GetObjectItemCaseSensitive(root, "interval")) {
        if (!cJSON_IsArray(iv) || cJSON_GetArraySize(iv) != 2) return E(Err::BadJson, "interval must be [min_s, max_s]");
        const cJSON* a = cJSON_GetArrayItem(iv, 0);
        const cJSON* b = cJSON_GetArrayItem(iv, 1);
        if (!cJSON_IsNumber(a) || !cJSON_IsNumber(b) || a->valuedouble < 1 || b->valuedouble > 120 || a->valuedouble > b->valuedouble)
            return E(Err::BadJson, "interval needs 1 <= min <= max <= 120 seconds");
        out.has_interval = true;
        out.min_ds = static_cast<std::uint16_t>(std::lround(a->valuedouble * 10));
        out.max_ds = static_cast<std::uint16_t>(std::lround(b->valuedouble * 10));
    }
    if (!out.has_enabled && !out.has_interval) return E(Err::BadJson, "nothing to do");
    return out;
}

void apply(Pool& p, const PoolPatch& d) noexcept {
    if (d.has_enabled) { p.anim_mask = d.anim_mask; p.clips = d.clips; }
    if (d.has_interval) { p.min_ds = d.min_ds; p.max_ds = d.max_ds; }
}

std::string pool_to_json(const Pool& p) {
    std::string s = "{\"enabled\":[";
    bool first = true;
    for (int i = 0; i < 12; ++i) {
        if (!((p.anim_mask >> i) & 1)) continue;
        if (!first) s += ',';
        first = false;
        s += '"'; s.append(eyes::kAnimNames[static_cast<std::size_t>(i)]); s += '"';
    }
    for (const auto& c : p.clips) {
        if (!first) s += ',';
        first = false;
        s += '"'; s.append(c.view()); s += '"';
    }
    char b[48];
    std::snprintf(b, sizeof b, "],\"interval\":[%.1f,%.1f]}", p.min_ds / 10.0, p.max_ds / 10.0);
    return s + b;
}

}  // namespace engine
