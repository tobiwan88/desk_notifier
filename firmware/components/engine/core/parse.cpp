#include "engine/core/parse.hpp"

#include <cmath>
#include <cstring>

#include "cJSON.h"

namespace engine {
namespace {


struct Doc {
    cJSON* root;
    explicit Doc(std::string_view s) : root(cJSON_ParseWithLength(s.data(), s.size())) {}
    ~Doc() { cJSON_Delete(root); }
    Doc(const Doc&) = delete;
    Doc& operator=(const Doc&) = delete;
};

bool known(const cJSON* o, std::initializer_list<std::string_view> keys, std::string_view& bad) {
    for (const cJSON* c = o->child; c; c = c->next) {
        bool ok = false;
        for (auto k : keys) ok |= (c->string && k == c->string);
        if (!ok) { bad = c->string ? c->string : "?"; return false; }
    }
    return true;
}

// ---- typed field readers: return nullopt if absent, error message via `err` ----
struct Ctx {
    std::string_view err;  // non-empty on failure
    void bad(std::string_view m) { if (err.empty()) err = m; }
};

std::optional<bool> get_bool(Ctx& c, const cJSON* o, const char* k) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(o, k);
    if (!v) return std::nullopt;
    if (!cJSON_IsBool(v)) { c.bad("expected boolean"); return std::nullopt; }
    return cJSON_IsTrue(v);
}
std::optional<double> get_num(Ctx& c, const cJSON* o, const char* k, double lo, double hi) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(o, k);
    if (!v) return std::nullopt;
    if (!cJSON_IsNumber(v) || !(v->valuedouble >= lo && v->valuedouble <= hi)) { c.bad("number out of range"); return std::nullopt; }
    return v->valuedouble;
}
std::optional<std::int16_t> get_int(Ctx& c, const cJSON* o, const char* k, int lo, int hi) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(o, k);
    if (!v) return std::nullopt;
    if (!cJSON_IsNumber(v) || v->valuedouble != std::floor(v->valuedouble) || v->valuedouble < lo || v->valuedouble > hi) {
        c.bad("integer out of range"); return std::nullopt;
    }
    return static_cast<std::int16_t>(v->valueint);
}
std::optional<gfx::Rgb565be> get_hex(Ctx& c, const cJSON* o, const char* k) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(o, k);
    if (!v) return std::nullopt;
    const char* s = cJSON_IsString(v) ? v->valuestring : nullptr;
    if (!s || std::strlen(s) != 7 || s[0] != '#') { c.bad("colour must be #rrggbb"); return std::nullopt; }
    unsigned val = 0;
    for (int i = 1; i < 7; ++i) {
        const char ch = s[i];
        int d = (ch >= '0' && ch <= '9') ? ch - '0' : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 : (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 : -1;
        if (d < 0) { c.bad("colour must be #rrggbb"); return std::nullopt; }
        val = val * 16 + static_cast<unsigned>(d);
    }
    return gfx::rgb565_rt(static_cast<std::uint8_t>(val >> 16), static_cast<std::uint8_t>(val >> 8), static_cast<std::uint8_t>(val));
}
const cJSON* get_obj(Ctx& c, const cJSON* o, const char* k) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(o, k);
    if (v && !cJSON_IsObject(v)) { c.bad("expected object"); return nullptr; }
    return v;
}
template <class E, std::size_t N>
std::optional<E> get_enum(Ctx& c, const cJSON* o, const char* k, const std::pair<std::string_view, E> (&tab)[N]) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(o, k);
    if (!v) return std::nullopt;
    if (cJSON_IsString(v))
        for (const auto& [name, val] : tab) if (name == v->valuestring) return val;
    c.bad("unknown enum value");
    return std::nullopt;
}

}  // namespace

std::expected<EyeCmd, ParseError> parse_eye(std::string_view json) noexcept {
    auto E = [](Err c, std::string_view m) { return std::unexpected(ParseError{c, StaticString<63>(m)}); };
    if (json.size() > kMaxBody) return E(Err::TooLarge, "body over 1 KB");
    Doc d(json);
    if (!d.root || !cJSON_IsObject(d.root)) return E(Err::BadJson, "invalid JSON object");
    std::string_view bad;
    if (!known(d.root, {"mood", "look", "blink", "auto_blink", "auto_look", "pupil", "lids", "style", "save_style",
                        "brightness", "transition_ms", "interrupt"}, bad))
        return E(Err::BadJson, "unknown field");
    Ctx c;
    EyeCmd cmd;
    static constexpr std::pair<std::string_view, eyes::Mood> kMoodNames[] = {
        {"neutral", eyes::Mood::Neutral}, {"happy", eyes::Mood::Happy}, {"angry", eyes::Mood::Angry},
        {"sleepy", eyes::Mood::Sleepy}, {"scared", eyes::Mood::Scared}, {"curious", eyes::Mood::Curious},
        {"asleep", eyes::Mood::Asleep}};
    cmd.mood = get_enum(c, d.root, "mood", kMoodNames);

    if (const cJSON* l = cJSON_GetObjectItemCaseSensitive(d.root, "look")) {
        if (!cJSON_IsArray(l) || cJSON_GetArraySize(l) != 2) c.bad("look must have 2 items");
        else {
            std::array<std::int16_t, 2> q{};
            for (int i = 0; i < 2; ++i) {
                const cJSON* n = cJSON_GetArrayItem(l, i);
                if (!cJSON_IsNumber(n) || n->valuedouble < -1 || n->valuedouble > 1) { c.bad("look values must be -1..1"); break; }
                q[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(std::lround(n->valuedouble * 256));
            }
            if (c.err.empty()) cmd.look_q8 = q;
        }
    }
    if (const cJSON* b = cJSON_GetObjectItemCaseSensitive(d.root, "blink")) {
        if (cJSON_IsBool(b)) { if (cJSON_IsTrue(b)) cmd.blink = EyeSel::Both; }
        else {
            static constexpr std::pair<std::string_view, EyeSel> kSel[] = {{"both", EyeSel::Both}, {"left", EyeSel::Left}, {"right", EyeSel::Right}};
            cmd.blink = get_enum(c, d.root, "blink", kSel);
        }
    }
    cmd.auto_blink = get_bool(c, d.root, "auto_blink");
    cmd.auto_look = get_bool(c, d.root, "auto_look");
    if (auto p = get_num(c, d.root, "pupil", 0.25, 1.0)) cmd.pupil = static_cast<std::uint8_t>(std::lround(*p * 255));

    if (const cJSON* l = get_obj(c, d.root, "lids")) {
        if (!known(l, {"top", "bottom", "tilt"}, bad)) c.bad("unknown field in lids");
        LidCmd lc;
        if (auto v = get_num(c, l, "top", 0, 1)) lc.top = static_cast<std::uint8_t>(std::lround(*v * 255));
        if (auto v = get_num(c, l, "bottom", 0, 1)) lc.bottom = static_cast<std::uint8_t>(std::lround(*v * 255));
        if (auto v = get_num(c, l, "tilt", -1, 1)) lc.tilt = static_cast<std::int8_t>(std::lround(*v * 127));
        cmd.lids = lc;
    }
    if (const cJSON* s = get_obj(c, d.root, "style")) {
        if (!known(s, {"preset", "bg", "sclera", "iris", "pupil", "highlight", "lid"}, bad)) c.bad("unknown field in style");
        eyes::StylePatch p;
        static constexpr std::pair<std::string_view, std::uint8_t> kPre[] = {{"realistic", 0}, {"robo", 1}};
        p.preset = get_enum(c, s, "preset", kPre);
        p.bg = get_hex(c, s, "bg");
        if (const cJSON* o = get_obj(c, s, "sclera")) {
            if (!known(o, {"on", "w", "h", "radius", "color"}, bad)) c.bad("unknown field in sclera");
            p.sclera_on = get_bool(c, o, "on"); p.sclera_w = get_int(c, o, "w", 20, 230); p.sclera_h = get_int(c, o, "h", 20, 230);
            p.sclera_radius = get_int(c, o, "radius", 0, 115); p.sclera_color = get_hex(c, o, "color");
        }
        if (const cJSON* o = get_obj(c, s, "iris")) {
            if (!known(o, {"on", "r", "ring_w", "color", "ring"}, bad)) c.bad("unknown field in iris");
            p.iris_on = get_bool(c, o, "on"); p.iris_r = get_int(c, o, "r", 5, 100); p.iris_ring_w = get_int(c, o, "ring_w", 0, 20);
            p.iris_color = get_hex(c, o, "color"); p.iris_ring = get_hex(c, o, "ring");
        }
        if (const cJSON* o = get_obj(c, s, "pupil")) {
            if (!known(o, {"on", "r", "shape", "slit_w", "color"}, bad)) c.bad("unknown field in pupil");
            static constexpr std::pair<std::string_view, eyes::PupilShape> kShape[] = {{"round", eyes::PupilShape::Round}, {"slit", eyes::PupilShape::Slit}};
            p.pupil_on = get_bool(c, o, "on"); p.pupil_r = get_int(c, o, "r", 2, 90); p.pupil_shape = get_enum(c, o, "shape", kShape);
            p.pupil_slit_w = get_int(c, o, "slit_w", 2, 40); p.pupil_color = get_hex(c, o, "color");
        }
        if (const cJSON* o = get_obj(c, s, "highlight")) {
            if (!known(o, {"on", "r", "dx", "dy", "color"}, bad)) c.bad("unknown field in highlight");
            p.hl_on = get_bool(c, o, "on"); p.hl_r = get_int(c, o, "r", 1, 40); p.hl_dx = get_int(c, o, "dx", -120, 120);
            p.hl_dy = get_int(c, o, "dy", -120, 120); p.hl_color = get_hex(c, o, "color");
        }
        if (const cJSON* o = get_obj(c, s, "lid")) {
            if (!known(o, {"color", "curve"}, bad)) c.bad("unknown field in lid");
            p.lid_color = get_hex(c, o, "color"); p.lid_curve = get_int(c, o, "curve", 0, 60);
        }
        cmd.style = p;
    }
    if (auto v = get_bool(c, d.root, "save_style")) cmd.save_style = *v;
    if (auto v = get_int(c, d.root, "brightness", 0, 100)) cmd.brightness = static_cast<std::uint8_t>(*v);
    if (auto v = get_int(c, d.root, "transition_ms", 0, 5000)) cmd.transition_ms = static_cast<std::uint16_t>(*v);
    if (auto v = get_bool(c, d.root, "interrupt")) cmd.interrupt = *v;

    if (!c.err.empty()) return E(Err::BadJson, c.err);
    return cmd;
}

std::expected<PlayCmd, ParseError> parse_play(std::string_view json) noexcept {
    auto E = [](Err c, std::string_view m) { return std::unexpected(ParseError{c, StaticString<63>(m)}); };
    if (json.size() > kMaxBody) return E(Err::TooLarge, "body over 1 KB");
    Doc d(json);
    if (!d.root || !cJSON_IsObject(d.root)) return E(Err::BadJson, "invalid JSON object");
    std::string_view bad;
    if (!known(d.root, {"clip", "left", "right", "loop", "then"}, bad)) return E(Err::BadJson, "unknown field");
    PlayCmd p;
    Ctx c;
    auto name = [&](const char* k, std::initializer_list<std::size_t> targets) -> bool {
        const cJSON* v = cJSON_GetObjectItemCaseSensitive(d.root, k);
        if (!v) return false;
        if (!cJSON_IsString(v)) { c.bad("clip name must be a string"); return false; }
        if (!clip_name_valid(v->valuestring)) { c.bad("invalid clip name"); return false; }
        for (auto t : targets) p.clip[t].assign(v->valuestring);
        return true;
    };
    const bool both = name("clip", {0, 1});
    const bool l = name("left", {0}), r = name("right", {1});
    if (both && (l || r)) c.bad("use clip or left/right, not both");
    if (!both && !l && !r && c.err.empty()) c.bad("clip, left or right required");
    if (auto v = get_bool(c, d.root, "loop")) p.loop = *v;
    if (const cJSON* t = cJSON_GetObjectItemCaseSensitive(d.root, "then")) {
        if (cJSON_IsString(t) && std::string_view(t->valuestring) == "eyes") p.then_eyes = true;
        else if (cJSON_IsString(t) && std::string_view(t->valuestring) == "hold") p.then_eyes = false;
        else c.bad("then must be eyes or hold");
    }
    if (!c.err.empty()) {
        return E(c.err == "invalid clip name" ? Err::BadName : Err::BadJson, c.err);
    }
    return p;
}

}  // namespace engine
