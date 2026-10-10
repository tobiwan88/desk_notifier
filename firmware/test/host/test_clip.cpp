#include <cstdio>
#include <string>
#include <vector>

#include "clip/core/clip.hpp"
#include "doctest.h"

using namespace clip;

namespace {
std::vector<std::byte> load(const char* name) {
    std::string path = std::string(GOLDEN_DIR) + "/" + name;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    std::vector<std::byte> v;
    if (!f) return v;
    std::byte buf[4096];
    for (std::size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) v.insert(v.end(), buf, buf + n);
    std::fclose(f);
    return v;
}
bool rd(void* ctx, std::uint32_t off, std::span<std::byte> out) noexcept {
    auto& v = *static_cast<std::vector<std::byte>*>(ctx);
    if (off + out.size() > v.size()) return false;
    std::memcpy(out.data(), v.data() + off, out.size());
    return true;
}
std::expected<Header, engine::Err> check(std::vector<std::byte>& v) {
    return validate(Reader{rd, &v}, static_cast<std::uint32_t>(v.size()));
}
}  // namespace

TEST_CASE("golden clips validate") {
    for (const char* n : {"good.clip", "full.clip"}) {
        auto v = load(n);
        CHECK(!v.empty());
        auto h = check(v);
        CHECK(static_cast<bool>(h));
        if (h) CHECK(h->frame_count >= 5);
    }
}

TEST_CASE("corrupted clips are rejected") {
    for (const char* n : {"bad_crc.clip", "truncated.clip", "bad_magic.clip"}) {
        auto v = load(n);
        CHECK(!v.empty());
        auto h = check(v);
        CHECK(!h);
        if (!h) CHECK(h.error() == engine::Err::BadClip);
    }
}

TEST_CASE("rect_ok enforces screen, circle trim and size") {
    RectHdr r{100, 100, 20, 20, 0, 0, 800};
    CHECK(rect_ok(r));
    r.payload_len = 799; CHECK(!rect_ok(r));
    RectHdr corner{0, 0, 10, 10, 0, 0, 200}; CHECK(!rect_ok(corner));
    RectHdr off{230, 100, 20, 20, 0, 0, 800}; CHECK(!rect_ok(off));
    RectHdr enc{100, 100, 20, 20, 3, 0, 800}; CHECK(!rect_ok(enc));
}

TEST_CASE("flipping any single header byte never validates a bad clip as good with changed meaning") {
    auto base = load("good.clip");
    for (std::size_t i = 0; i < 32; ++i) {
        if (i == 16 || i == 17) continue;  // key_color is reserved and unchecked in v1
        auto v = base;
        v[i] = static_cast<std::byte>(~static_cast<unsigned>(v[i]));
        auto h = check(v);
        if (h) CHECK(false);  // header bytes are all covered by checks or crc (crc field) -- must fail
    }
}

TEST_CASE("crc32 check value") {
    const char s[] = "123456789";
    CHECK(crc32(0, std::as_bytes(std::span(s, 9))) == 0xCBF43926u);
}
