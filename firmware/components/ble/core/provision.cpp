#include "ble/core/provision.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace ble {
namespace {

constexpr std::size_t kMaxJson = 512;
constexpr std::size_t kMaxScanJson = 480;

struct Cursor {
    std::string_view s;
    std::size_t i = 0;
    void ws() noexcept { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i; }
    bool eat(char c) noexcept {
        ws();
        if (i < s.size() && s[i] == c) { ++i; return true; }
        return false;
    }
    bool word(std::string_view w) noexcept {
        if (s.substr(i, w.size()) != w) return false;
        i += w.size();
        return true;
    }
};

int hex4(std::string_view s, std::size_t at) noexcept {
    if (at + 4 > s.size()) return -1;
    int v = 0;
    for (std::size_t k = 0; k < 4; ++k) {
        const char c = s[at + k];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return -1;
        v = v * 16 + d;
    }
    return v;
}

void utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// A JSON string with the opening quote already consumed.
bool string_body(Cursor& c, std::string& out) {
    out.clear();
    while (c.i < c.s.size()) {
        const char ch = c.s[c.i++];
        if (ch == '"') return true;
        if (static_cast<unsigned char>(ch) < 0x20) return false;
        if (ch != '\\') { out += ch; continue; }
        if (c.i >= c.s.size()) return false;
        const char e = c.s[c.i++];
        switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                int u = hex4(c.s, c.i);
                if (u < 0) return false;
                c.i += 4;
                std::uint32_t cp = static_cast<std::uint32_t>(u);
                if (cp >= 0xD800 && cp <= 0xDBFF) {  // high surrogate: the low one must follow
                    if (c.s.substr(c.i, 2) != "\\u") return false;
                    const int lo = hex4(c.s, c.i + 2);
                    if (lo < 0xDC00 || lo > 0xDFFF) return false;
                    c.i += 6;
                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (static_cast<std::uint32_t>(lo) - 0xDC00u);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return false;
                }
                if (cp == 0) return false;
                utf8(out, cp);
                break;
            }
            default: return false;
        }
    }
    return false;  // no closing quote
}

bool valid_tz(std::string_view tz) noexcept {
    if (tz.empty() || tz.size() > 63) return false;
    return std::all_of(tz.begin(), tz.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '+' || c == '-' ||
               c == ',' || c == '.' || c == '/' || c == ':' || c == '<' || c == '>' || c == '_' || c == ' ';
    });
}

bool fold_hostname(std::string& h) noexcept {
    if (h.empty() || h.size() > 32) return false;
    for (char& c : h) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    }
    return h.front() != '-' && h.back() != '-';
}

void append_escaped(std::string& out, std::string_view s) {
    for (char ch : s) {
        const auto u = static_cast<unsigned char>(ch);
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            default:
                if (u < 0x20) {
                    char b[8];
                    std::snprintf(b, sizeof b, "\\u%04x", u);
                    out += b;
                } else {
                    out += ch;
                }
        }
    }
}

void add_str(std::string& out, const char* key, std::string_view v, bool first = false) {
    if (!first) out += ',';
    out += '"';
    out += key;
    out += "\":\"";
    append_escaped(out, v);
    out += '"';
}

}  // namespace

const char* error_name(ParseError e) noexcept {
    switch (e) {
        case ParseError::None: return "ok";
        case ParseError::Json: return "bad json";
        case ParseError::Empty: return "nothing to set";
        case ParseError::Ssid: return "bad ssid";
        case ParseError::Pass: return "bad password";
        case ParseError::Tz: return "bad timezone";
        case ParseError::Hostname: return "bad hostname";
    }
    return "";
}

ParseError parse_config(std::string_view json, Config& out) noexcept {
    out = Config{};
    if (json.size() > kMaxJson) return ParseError::Json;
    Cursor c{json};
    if (!c.eat('{')) return ParseError::Json;
    std::string key, val;
    if (!c.eat('}')) {
        for (;;) {
            if (!c.eat('"') || !string_body(c, key) || !c.eat(':')) return ParseError::Json;
            c.ws();
            if (c.eat('"')) {
                if (!string_body(c, val)) return ParseError::Json;
                if (key == "ssid") out.ssid = val;
                else if (key == "pass") out.pass = val;
                else if (key == "tz") out.tz = val;
                else if (key == "hostname") out.hostname = val;
            } else if (c.word("true")) {
                if (key == "reboot") out.reboot = true;
            } else if (c.word("false")) {
                if (key == "reboot") out.reboot = false;
            } else {
                return ParseError::Json;
            }
            if (c.eat(',')) continue;
            if (c.eat('}')) break;
            return ParseError::Json;
        }
    }
    c.ws();
    if (c.i != json.size()) return ParseError::Json;

    if (out.ssid && (out.ssid->empty() || out.ssid->size() > 32)) return ParseError::Ssid;
    if (out.pass && !out.pass->empty() && (out.pass->size() < 8 || out.pass->size() > 63)) return ParseError::Pass;
    if (out.pass && !out.ssid) return ParseError::Ssid;  // a password alone would silently break the saved network
    if (out.tz && !valid_tz(*out.tz)) return ParseError::Tz;
    if (out.hostname && !fold_hostname(*out.hostname)) return ParseError::Hostname;
    if (!out.ssid && !out.tz && !out.hostname && !out.reboot) return ParseError::Empty;
    return ParseError::None;
}

std::string status_json(const Status& s) {
    std::string o = "{";
    add_str(o, "v", s.version, true);
    add_str(o, "wifi", s.wifi);
    add_str(o, "ssid", s.ssid);
    add_str(o, "ip", s.ip);
    add_str(o, "host", s.hostname);
    add_str(o, "tz", s.tz);
    add_str(o, "result", s.result);
    char b[48];
    std::snprintf(b, sizeof b, ",\"time\":%s,\"rssi\":%d}", s.time_valid ? "true" : "false", s.rssi);
    o += b;
    return o;
}

std::string scan_json(std::span<const Network> found) {
    std::vector<const Network*> order;
    for (const auto& n : found) if (!n.ssid.empty()) order.push_back(&n);
    std::stable_sort(order.begin(), order.end(), [](const Network* a, const Network* b) { return a->rssi > b->rssi; });
    std::string o = "[";
    std::vector<std::string_view> seen;
    for (const Network* n : order) {
        if (std::find(seen.begin(), seen.end(), n->ssid) != seen.end()) continue;
        std::string item = "{";
        add_str(item, "ssid", n->ssid, true);
        char b[40];
        std::snprintf(b, sizeof b, ",\"rssi\":%d,\"sec\":%s}", n->rssi, n->secure ? "true" : "false");
        item += b;
        if (o.size() + item.size() + 2 > kMaxScanJson) break;
        if (o.size() > 1) o += ',';
        o += item;
        seen.push_back(n->ssid);
    }
    o += ']';
    return o;
}

void PairGuard::failed(std::uint32_t now_ms) noexcept {
    if (locked_ && static_cast<std::int32_t>(until_ - now_ms) <= 0) { locked_ = false; fails_ = 0; }  // lock ran out
    if (++fails_ >= kMaxFails) {
        locked_ = true;
        until_ = now_ms + kLockMs;
    }
}

bool PairGuard::locked(std::uint32_t now_ms) const noexcept {
    return locked_ && static_cast<std::int32_t>(until_ - now_ms) > 0;
}

}  // namespace ble
