#include "doctest.h"
#include "gfx/core/gfx.hpp"

using namespace gfx;
using namespace gfx::literals;

static_assert(trimmed_pixels(20) == 48'880);
static_assert(circle_pixels() > 45'000 && circle_pixels() < 46'000);
static_assert(kChord[0].x1 > kChord[0].x0 && kChord[119].x0 == 0 && kChord[119].x1 == 240);
static_assert(kChord[0].x0 + kChord[0].x1 == kScreen);  // symmetric
static_assert(kEaseInOut[0] == 0 && kEaseInOut[256] == 255 && kEaseInOut[128] >= 127 && kEaseInOut[128] <= 128);
static_assert(kEdgeAlpha[0] == 0 && kEdgeAlpha[15] == 255);
static_assert(rgb565(0xFF8800) == rgb565_rt(0xFF, 0x88, 0x00));
static_assert(rgb565(0xFFFFFF) == 0xFFFF && rgb565(0) == 0);
static_assert("#ff8800"_rgb == rgb565(0xFF8800));
static_assert(blend(rgb565(0), rgb565(0xFFFFFF), 0) == 0 && blend(rgb565(0), rgb565(0xFFFFFF), 255) == 0xFFFF);
static_assert(Fixed<8>::from_int(3).to_int() == 3);
static_assert((Fixed<8>::from_int(3) * Fixed<8>::from_int(2)).to_int() == 6);
static_assert(unite(Rect{0, 0, 10, 10}, Rect{20, 5, 10, 10}) == Rect{0, 0, 30, 15});
static_assert(intersect(Rect{0, 0, 10, 10}, Rect{5, 5, 10, 10}) == Rect{5, 5, 5, 5});
static_assert(intersect(Rect{0, 0, 10, 10}, Rect{10, 0, 5, 5}).empty());

TEST_CASE("kEaseInOut is monotonic") {
    for (int i = 1; i <= 256; ++i) CHECK(kEaseInOut[i] >= kEaseInOut[i - 1]);
}

TEST_CASE("Strips untrimmed covers the rect exactly") {
    std::int32_t n = 0;
    for (Rect s : Strips({10, 5, 100, 47}, 20, false)) n += s.area();
    CHECK(n == 100 * 47);
}

TEST_CASE("Strips trimmed full screen = 48880 px, 12 strips") {
    std::int32_t n = 0; int c = 0;
    for (Rect s : Strips({0, 0, 240, 240}, 20, true)) { n += s.area(); ++c; CHECK(s.h <= 20); }
    CHECK(n == 48'880);
    CHECK(c == 12);
}

TEST_CASE("Strips respects a sub-rect and the circle") {
    for (Rect s : Strips({100, 0, 40, 240}, 20, true)) CHECK(s.x >= 100);
    int c = 0;
    for ([[maybe_unused]] Rect s : Strips({0, 0, 10, 4}, 20, true)) ++c;  // corner is outside the circle
    CHECK(c == 0);
}

TEST_CASE("blend midpoint") {
    CHECK(blend(rgb565(0), rgb565(0xFFFFFF), 128) != 0);
}
