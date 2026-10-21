// screen: shop items to wear (hats, party glasses, the Christmas jumper, the swim ring), see props.hpp.
// Pure C++, drawn with screen/core/draw.hpp only.
#include "screen/core/props.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include "screen/core/draw.hpp"

namespace screen::props {
namespace {

using draw::hex;
using draw::Pt;

constexpr float kPi = 3.14159265f;

float ph(std::uint32_t t, std::uint32_t period) noexcept {
    return static_cast<float>(t % period) / static_cast<float>(period);
}
float wave(std::uint32_t t, std::uint32_t period) noexcept { return std::sin(2 * kPi * ph(t, period)); }

constexpr std::uint32_t shade(std::uint32_t c) noexcept { return draw::lerp_rgb(c, 0x000000, 80); }

// Convex polygon in screen pixels. With dark != c, a darker outline grown by g px from the centroid goes first.
void convex_fill(const Canvas& cv, Pt* in, int n, std::uint32_t c, std::uint32_t dark, float g) noexcept {
    if (n < 3) return;
    if (dark != c && g > 0) {
        Pt cen{0.0f, 0.0f};
        for (int i = 0; i < n; ++i) {
            cen.x += in[i].x / static_cast<float>(n);
            cen.y += in[i].y / static_cast<float>(n);
        }
        Pt out[16];
        for (int i = 0; i < n; ++i) {
            const float dx = in[i].x - cen.x, dy = in[i].y - cen.y;
            const float len = std::sqrt(dx * dx + dy * dy) + 1e-3f;
            out[i] = {in[i].x + dx / len * g, in[i].y + dy / len * g};
        }
        draw::polygon(cv, out, n, hex(dark));
    }
    draw::polygon(cv, in, n, hex(c));
}

// Five-pointed star as six convex pieces: the centre pentagon and one triangle per point.
void star(const Canvas& cv, float cx, float cy, float R, float ri, float rot, std::uint32_t c, int a) noexcept {
    Pt P[5], Q[5];
    for (int k = 0; k < 5; ++k) {
        const float ta = rot + 2 * kPi * static_cast<float>(k) / 5, tb = ta + kPi / 5;
        P[k] = {cx + R * std::cos(ta), cy + R * std::sin(ta)};
        Q[k] = {cx + ri * std::cos(tb), cy + ri * std::sin(tb)};
    }
    draw::polygon(cv, Q, 5, hex(c), a);
    for (int k = 0; k < 5; ++k) {
        const Pt tri[3] = {Q[(k + 4) % 5], P[k], Q[k]};
        draw::polygon(cv, tri, 3, hex(c), a);
    }
}

// A hat's frame. Design units: 1 unit is the brim width `w`; u to the right, v up from the middle of the brim.
// `tilt` turns the whole hat (radians, positive leans left). Screen pixels for everything drawn.
struct Hat {
    const Canvas& cv;
    float cx, by, w, ang_scr, cs, sn, ol;
    Hat(const Canvas& c, float x, float y, float width, float tilt) noexcept
        : cv(c), cx(x), by(y), w(width), ang_scr(-tilt), cs(std::cos(tilt)), sn(std::sin(tilt)),
          ol(0.6f + 0.02f * width) {}
    Pt P(float u, float v) const noexcept { return {cx + (u * cs - v * sn) * w, by - (u * sn + v * cs) * w}; }

    void poly(std::initializer_list<Pt> pts, std::uint32_t c, std::uint32_t dark) const noexcept {
        Pt in[16];
        int n = 0;
        for (const Pt q : pts)
            if (n < 16) in[n++] = P(q.x, q.y);
        convex_fill(cv, in, n, c, dark, ol);
    }
    void ell(float u, float v, float rx, float ry, float la, std::uint32_t c, std::uint32_t dark) const noexcept {
        const Pt p = P(u, v);
        const float an = ang_scr + la;
        if (dark != c) draw::ellipse(cv, p.x, p.y, rx * w + ol, ry * w + ol, hex(dark), an);
        draw::ellipse(cv, p.x, p.y, rx * w, ry * w, hex(c), an);
    }
    void circle(float u, float v, float r, std::uint32_t c, std::uint32_t dark, int a = 256) const noexcept {
        const Pt p = P(u, v);
        if (dark != c) draw::circle(cv, p.x, p.y, r * w + ol, hex(dark), a);
        draw::circle(cv, p.x, p.y, r * w, hex(c), a);
    }
    void cap(float u0, float v0, float u1, float v1, float r, std::uint32_t c, std::uint32_t dark,
             int a = 256) const noexcept {
        const Pt p = P(u0, v0), q = P(u1, v1);
        if (dark != c) draw::capsule(cv, p.x, p.y, q.x, q.y, r * w + ol, hex(dark), a);
        draw::capsule(cv, p.x, p.y, q.x, q.y, r * w, hex(c), a);
    }
    void glint(float u, float v, float r, int a) const noexcept {
        const Pt p = P(u, v);
        draw::circle(cv, p.x, p.y, r * w, hex(0xFFFFFF), a);
    }
    void sparkle(float u, float v, float r, int a) const noexcept {
        const Pt p = P(u, v);
        draw::sparkle(cv, p.x, p.y, r * w, hex(0xFFFFFF), a);
    }
};

// ---- the hats ----------------------------------------------------------------------------------------------

void top_hat(const Hat& h) noexcept {
    constexpr std::uint32_t kBlack = 0x2B2B36, kBlackDark = 0x0B0B10, kLid = 0x44444F, kBand = 0xD63A3E;
    auto hw = [](float v) noexcept { return 0.31f - 0.05f * v; };  // the crown narrows towards the top
    h.poly({{-hw(0.0f), 0.0f}, {hw(0.0f), 0.0f}, {hw(0.8f), 0.8f}, {-hw(0.8f), 0.8f}}, kBlack, kBlackDark);
    h.poly({{-hw(0.08f), 0.08f}, {hw(0.08f), 0.08f}, {hw(0.22f), 0.22f}, {-hw(0.22f), 0.22f}}, kBand, kBand);
    h.ell(0.0f, 0.80f, 0.27f, 0.055f, 0.0f, kLid, kLid);
    h.ell(0.0f, 0.0f, 0.50f, 0.07f, 0.0f, kBlack, kBlackDark);
    h.cap(-0.20f, 0.30f, -0.20f, 0.60f, 0.022f, 0xFFFFFF, 0xFFFFFF, 110);
}

void flower_crown(const Hat& h) noexcept {
    constexpr std::uint32_t kStem = 0x5BBF6A, kStemDark = 0x2F7A3E, kLeaf = 0x6CCB72, kLeafDark = 0x2F7A3E;
    constexpr std::uint32_t kPink = 0xFF8FB8, kWhite = 0xFFFFFF, kYellow = 0xFFD23A, kCentre = 0xF0A020;
    constexpr float kCy = -0.44f, kR = 0.6f;  // the ring sits round the top of the head
    auto ux = [](float deg) noexcept { return kR * std::cos(deg * kPi / 180.0f); };
    auto vy = [](float deg) noexcept { return kCy + kR * std::sin(deg * kPi / 180.0f); };
    // garland along the ring
    for (int i = 0; i < 12; ++i) {
        const float d0 = 15.0f + 150.0f * static_cast<float>(i) / 12.0f;
        const float d1 = 15.0f + 150.0f * static_cast<float>(i + 1) / 12.0f;
        h.cap(ux(d0), vy(d0), ux(d1), vy(d1), 0.024f, kStem, kStemDark);
    }
    // leaves between the flowers, lying along the ring
    constexpr float kLeafDeg[4] = {39.0f, 73.0f, 107.0f, 141.0f};
    for (const float d : kLeafDeg) {
        const float rad = kR + 0.02f;
        h.ell(rad * std::cos(d * kPi / 180.0f), kCy + rad * std::sin(d * kPi / 180.0f), 0.085f, 0.034f,
              -(d + 90.0f) * kPi / 180.0f, kLeaf, kLeafDark);
    }
    // flowers: five petals and a centre
    constexpr float kFlowerDeg[5] = {22.0f, 56.0f, 90.0f, 124.0f, 158.0f};
    constexpr std::uint32_t kFlowerC[5] = {kPink, kWhite, kYellow, kPink, kWhite};
    for (int i = 0; i < 5; ++i) {
        const float u = ux(kFlowerDeg[i]), v = vy(kFlowerDeg[i]);
        for (int k = 0; k < 5; ++k) {
            const float a = 2 * kPi * static_cast<float>(k) / 5.0f;
            h.circle(u + 0.058f * std::cos(a), v + 0.058f * std::sin(a), 0.05f, kFlowerC[i],
                     shade(kFlowerC[i]));
        }
        h.circle(u, v, 0.034f, kCentre, shade(kCentre));
    }
}

void beret(const Hat& h) noexcept {
    constexpr std::uint32_t kRed = 0xD63A3E, kRedDark = 0x7E1A22, kRedHi = 0xF27A7E, kStalk = 0x4A1418;
    h.ell(0.0f, 0.10f, 0.42f, 0.16f, 0.0f, kRed, kRedDark);      // the flat disc
    h.ell(0.0f, 0.0f, 0.41f, 0.05f, 0.0f, kRedDark, kRedDark);   // the lower edge where it meets the head
    h.ell(-0.10f, 0.17f, 0.20f, 0.05f, 0.0f, kRedHi, kRedHi);    // a soft light on the top
    h.cap(0.06f, 0.24f, 0.10f, 0.34f, 0.03f, kStalk, kStalk);    // the little stalk
    h.circle(0.10f, 0.35f, 0.03f, kStalk, kStalk);
}

void pirate_hat(const Hat& h) noexcept {
    constexpr std::uint32_t kBlack = 0x2B2B36, kBlackDark = 0x0B0B10, kGold = 0xFFD23A, kGoldDark = 0xB8860B;
    h.ell(0.0f, 0.06f, 0.56f, 0.09f, 0.0f, kBlack, kBlackDark);  // the brim
    h.poly({{-0.58f, 0.10f}, {-0.34f, 0.02f}, {-0.46f, 0.30f}}, kBlack, kBlackDark);  // upturned corners
    h.poly({{0.58f, 0.10f}, {0.34f, 0.02f}, {0.46f, 0.30f}}, kBlack, kBlackDark);
    h.poly({{-0.30f, 0.04f}, {0.30f, 0.04f}, {0.24f, 0.34f}, {0.0f, 0.44f}, {-0.24f, 0.34f}}, kBlack, kBlackDark);
    h.cap(-0.29f, 0.10f, 0.29f, 0.10f, 0.03f, kGold, kGoldDark);  // gold edge along the crown foot
    h.circle(0.0f, 0.24f, 0.075f, kGold, kGoldDark);              // round badge with a white dot
    h.circle(0.0f, 0.24f, 0.035f, 0xFFFFFF, 0xFFFFFF);
}

void cowboy_hat(const Hat& h) noexcept {
    constexpr std::uint32_t kBrown = 0xA86A34, kBrownDark = 0x5A3418, kBrownHi = 0xC78C50, kBand = 0x3E2212;
    constexpr std::uint32_t kGold = 0xFFD23A, kGoldDark = 0xB8860B;
    h.ell(0.0f, 0.02f, 0.58f, 0.12f, 0.0f, kBrown, kBrownDark);     // wide brim
    h.ell(0.0f, 0.07f, 0.48f, 0.04f, 0.0f, kBrownHi, kBrownHi);     // light on the brim
    h.poly({{-0.27f, 0.04f}, {-0.25f, 0.34f}, {-0.12f, 0.45f}, {0.12f, 0.45f}, {0.25f, 0.34f}, {0.27f, 0.04f}},
           kBrown, kBrownDark);
    h.cap(-0.265f, 0.10f, 0.265f, 0.10f, 0.05f, kBand, kBand);      // the band
    h.poly({{-0.045f, 0.06f}, {0.045f, 0.06f}, {0.045f, 0.14f}, {-0.045f, 0.14f}}, kGold, kGoldDark);
}

void chef_hat(const Hat& h) noexcept {
    constexpr std::uint32_t kWhite = 0xFFFFFF, kWhiteDark = 0xC5CCD8, kBand = 0xF2F4F8;
    struct Blob { float u, v, r; };
    constexpr Blob kBlobs[5] = {{-0.22f, 0.40f, 0.20f}, {0.0f, 0.50f, 0.25f}, {0.22f, 0.40f, 0.20f},
                                {-0.12f, 0.28f, 0.17f}, {0.12f, 0.28f, 0.17f}};
    for (const Blob& b : kBlobs) h.circle(b.u, b.v, b.r, kWhite, kWhiteDark);  // the puffy top
    h.poly({{-0.26f, 0.0f}, {0.26f, 0.0f}, {0.26f, 0.20f}, {-0.26f, 0.20f}}, kBand, kWhiteDark);  // the band
    h.cap(-0.26f, 0.09f, 0.26f, 0.09f, 0.012f, kWhiteDark, kWhiteDark);
}

void viking_helmet(const Hat& h) noexcept {
    constexpr std::uint32_t kSteel = 0x9AA3AE, kSteelDark = 0x4E5762, kHorn = 0xFFF3D6, kHornDark = 0xC9B48A;
    constexpr float kHu[4] = {0.30f, 0.41f, 0.50f, 0.47f}, kHv[4] = {0.10f, 0.16f, 0.30f, 0.48f};
    constexpr float kHr[4] = {0.07f, 0.06f, 0.045f, 0.025f};
    for (int side = -1; side <= 1; side += 2) {  // horns first, behind the dome
        const float sd = static_cast<float>(side);
        for (int k = 0; k < 3; ++k)
            h.cap(sd * kHu[k], kHv[k], sd * kHu[k + 1], kHv[k + 1], 0.5f * (kHr[k] + kHr[k + 1]), kHorn, kHornDark);
    }
    Pt dome[9];  // upper half disc
    for (int i = 0; i < 9; ++i) {
        const float a = kPi * static_cast<float>(i) / 8.0f;
        dome[i] = {0.36f * std::cos(a), 0.36f * std::sin(a)};
    }
    Pt in[9];
    for (int i = 0; i < 9; ++i) in[i] = h.P(dome[i].x, dome[i].y);
    convex_fill(h.cv, in, 9, kSteel, kSteelDark, h.ol);
    h.ell(0.0f, 0.0f, 0.40f, 0.065f, 0.0f, kSteelDark, kSteelDark);  // the rim
    h.glint(-0.13f, 0.24f, 0.035f, 170);
}

void acorn_crown(const Hat& h, std::uint32_t t) noexcept {
    constexpr std::uint32_t kGold = 0xFFD23A, kGoldDark = 0xB8860B, kNut = 0xE8B040, kNutDark = 0x9A6A18;
    constexpr std::uint32_t kCap = 0x7A4A20, kCapDark = 0x3E2410;
    constexpr float kU[5] = {-0.34f, -0.17f, 0.0f, 0.17f, 0.34f}, kH[5] = {0.32f, 0.40f, 0.46f, 0.40f, 0.32f};
    for (int i = 0; i < 5; ++i) {  // points, standing on the band
        const float u = kU[i];
        h.poly({{u - 0.09f, 0.08f}, {u + 0.09f, 0.08f}, {u, kH[i]}}, kGold, kGoldDark);
    }
    h.poly({{-0.40f, 0.0f}, {0.40f, 0.0f}, {0.40f, 0.14f}, {-0.40f, 0.14f}}, kGold, kGoldDark);  // the band
    for (int i = 0; i < 5; ++i) {  // an acorn on each point
        const float u = kU[i], v = kH[i];
        h.ell(u, v + 0.04f, 0.065f, 0.075f, 0.0f, kNut, kNutDark);
        h.ell(u, v + 0.11f, 0.07f, 0.035f, 0.0f, kCap, kCapDark);
    }
    const float tw = wave(t, 1800);
    h.sparkle(-0.48f, 0.50f, 0.12f, static_cast<int>(120 + 120 * tw));
    h.sparkle(0.46f, 0.36f, 0.09f, static_cast<int>(120 - 100 * tw));
}

void pumpkin_hat(const Hat& h) noexcept {
    constexpr std::uint32_t kPumpkin = 0xFF8A1F, kPumpkinDark = 0xC85A10, kRib = 0xE0700F, kFace = 0xFFE27A;
    constexpr std::uint32_t kStem = 0x6E9A34, kStemDark = 0x3E5A1C, kLeaf = 0x4AA45E, kLeafDark = 0x25602E;
    h.cap(0.0f, 0.34f, 0.03f, 0.50f, 0.035f, kStem, kStemDark);  // stem and leaf, behind the body
    h.ell(0.12f, 0.46f, 0.08f, 0.035f, -0.5f, kLeaf, kLeafDark);
    struct Lobe { float u, v, rx, ry; };
    constexpr Lobe kLobes[3] = {{-0.12f, 0.18f, 0.18f, 0.19f}, {0.12f, 0.18f, 0.18f, 0.19f}, {0.0f, 0.20f, 0.22f, 0.20f}};
    for (const Lobe& l : kLobes) h.ell(l.u, l.v, l.rx, l.ry, 0.0f, kPumpkin, kPumpkinDark);
    h.cap(-0.14f, 0.34f, -0.14f, 0.06f, 0.012f, kRib, kRib, 160);
    h.cap(0.14f, 0.34f, 0.14f, 0.06f, 0.012f, kRib, kRib, 160);
    h.poly({{-0.14f, 0.26f}, {-0.05f, 0.26f}, {-0.095f, 0.33f}}, kFace, kFace);  // carved eyes
    h.poly({{0.05f, 0.26f}, {0.14f, 0.26f}, {0.095f, 0.33f}}, kFace, kFace);
    h.poly({{-0.14f, 0.13f}, {0.14f, 0.13f}, {0.08f, 0.06f}, {-0.08f, 0.06f}}, kFace, kFace);  // grin
    h.ell(0.0f, 0.0f, 0.38f, 0.05f, 0.0f, kPumpkinDark, kPumpkinDark);  // the rim it sits on
    h.glint(-0.22f, 0.30f, 0.03f, 150);
}

void bunny_ears(const Hat& h, std::uint32_t t) noexcept {
    constexpr std::uint32_t kWhite = 0xFFFFFF, kWhiteDark = 0xC9C2BA, kInner = 0xFFB3C9;
    constexpr std::uint32_t kBand = 0xFF6B9A, kBandDark = 0xB8395F;
    const float wob = 0.07f * wave(t, 1300);
    for (int side = -1; side <= 1; side += 2) {
        const float sd = static_cast<float>(side);
        const float lean = sd * (0.14f + wob);  // screen angle: negative leans left
        const float cu = sd * 0.27f + 0.2f * std::sin(lean);
        h.ell(cu, 0.44f, 0.11f, 0.40f, lean, kWhite, kWhiteDark);
        h.ell(cu, 0.46f, 0.055f, 0.32f, lean, kInner, kInner);
    }
    constexpr int kSeg = 8;  // headband following the top of the head
    for (int i = 0; i < kSeg; ++i) {
        const float a0 = (15.0f + 150.0f * static_cast<float>(i) / kSeg) * kPi / 180.0f;
        const float a1 = (15.0f + 150.0f * static_cast<float>(i + 1) / kSeg) * kPi / 180.0f;
        h.cap(0.6f * std::cos(a0), -0.44f + 0.6f * std::sin(a0), 0.6f * std::cos(a1), -0.44f + 0.6f * std::sin(a1),
              0.035f, kBand, kBandDark);
    }
}

void tracht_hat(const Hat& h) noexcept {
    constexpr std::uint32_t kGreen = 0x3F8F55, kGreenDark = 0x1F5630, kCord = 0x7A4A22, kCordDark = 0x4A2A10;
    constexpr std::uint32_t kTuft = 0x6B4226, kTuftDark = 0x3A2214;
    h.poly({{-0.30f, 0.02f}, {0.30f, 0.02f}, {0.26f, 0.30f}, {0.16f, 0.40f}, {-0.16f, 0.40f}, {-0.26f, 0.30f}},
           kGreen, kGreenDark);
    h.cap(-0.28f, 0.12f, 0.28f, 0.12f, 0.028f, kCord, kCordDark);  // cord band
    h.circle(0.27f, 0.10f, 0.035f, kCord, kCordDark);               // knot
    h.cap(0.20f, 0.14f, 0.30f, 0.46f, 0.035f, kTuft, kTuftDark);   // tuft on the side
    h.cap(0.22f, 0.14f, 0.36f, 0.42f, 0.035f, kTuft, kTuftDark);
    h.cap(0.24f, 0.14f, 0.40f, 0.34f, 0.035f, kTuft, kTuftDark);
    h.ell(0.0f, 0.02f, 0.46f, 0.085f, 0.0f, kGreen, kGreenDark);  // narrow brim
    h.cap(-0.18f, 0.22f, -0.18f, 0.34f, 0.02f, 0xFFFFFF, 0xFFFFFF, 90);
}

void shop_hat_dispatch(const Hat& h, int which, std::uint32_t t) noexcept {
    switch (which) {
        case kHatTop: top_hat(h); break;
        case kHatFlowerCrown: flower_crown(h); break;
        case kHatBeret: beret(h); break;
        case kHatPirate: pirate_hat(h); break;
        case kHatCowboy: cowboy_hat(h); break;
        case kHatChef: chef_hat(h); break;
        case kHatViking: viking_helmet(h); break;
        case kHatAcornCrown: acorn_crown(h, t); break;
        case kHatPumpkin: pumpkin_hat(h); break;
        case kHatBunnyEars: bunny_ears(h, t); break;
        case kHatTracht: tracht_hat(h); break;
        default: break;
    }
}

}  // namespace

void shop_hat(const Canvas& cv, int which, float cx, float brim_y, float width, std::uint32_t t) noexcept {
    // tilt per hat: the top hat and the beret lean to one side
    const float tilt = which == kHatBeret ? -0.2f : which == kHatTop ? 0.1f : which == kHatPirate ? 0.05f : 0.0f;
    const Hat h(cv, cx, brim_y, width, tilt);
    shop_hat_dispatch(h, which, t);
}

void party_glasses(const Canvas& cv, float lx, float ly, float rx, float ry, float r, std::uint32_t t) noexcept {
    constexpr std::uint32_t kGold = 0xFFD23A, kGoldDark = 0xB8860B, kLens = 0xFFE6F4;
    // bridge first, then the star frames, then the pale pink lenses over them
    draw::capsule(cv, lx + r, ly, rx - r, ry, 0.14f * r + 0.8f, hex(kGoldDark));
    draw::capsule(cv, lx + r, ly, rx - r, ry, 0.14f * r, hex(kGold));
    const float R = 1.9f * r, ri = 1.0f * r, rot = -kPi / 2;
    const float eyes[2][2] = {{lx, ly}, {rx, ry}};
    for (const auto& e : eyes) star(cv, e[0], e[1], R + 1.0f, ri + 1.0f, rot, kGoldDark, 256);
    for (const auto& e : eyes) star(cv, e[0], e[1], R, ri, rot, kGold, 256);
    for (const auto& e : eyes) draw::circle(cv, e[0], e[1], r, hex(kLens), 130);
    // a glint sliding across each lens, the right one half a cycle behind
    for (int i = 0; i < 2; ++i) {
        const float gp = ph(t + static_cast<std::uint32_t>(i) * 1200u, 2400);
        const float gx = eyes[i][0] + r * (-0.45f + 0.9f * gp), gy = eyes[i][1] - 0.3f * r;
        draw::sparkle(cv, gx, gy, 0.35f * r, hex(0xFFFFFF), 230);
    }
}

void xmas_sweater(const Canvas& cv, float cx, float cy, float rx, float ry, std::uint32_t t) noexcept {
    (void)t;  // still
    constexpr std::uint32_t kRed = 0xD8393F, kRedDark = 0x7E1A22, kWhite = 0xFFFFFF;
    constexpr std::uint32_t kTree = 0x2E9A5A, kTreeDark = 0x17532F, kStar = 0xFFD23A, kStarDark = 0xB8860B;
    constexpr float k = 0.9f;          // stay inside the body ellipse
    const float top = cy - 0.2f * ry;  // the neckline, a bit above the middle
    // the jumper: the neckline, then the bottom half of the ellipse
    Pt body[12];
    int n = 0;
    body[n++] = {cx + 0.88f * k * rx, top};
    for (int i = 0; i <= 8; ++i) {
        const float a = kPi * static_cast<float>(i) / 8.0f;
        body[n++] = {cx + k * rx * std::cos(a), cy + k * ry * std::sin(a)};
    }
    body[n++] = {cx - 0.88f * k * rx, top};
    convex_fill(cv, body, n, kRed, kRedDark, 0.9f);
    // white zigzag knit band across the chest
    const float zy = cy - 0.06f * ry, zamp = 0.08f * ry, rad = std::max(1.2f, 0.09f * ry);
    for (int i = 0; i < 8; ++i) {
        const float u0 = cx + (-0.8f + 0.2f * static_cast<float>(i)) * rx;
        const float u1 = u0 + 0.2f * rx;
        const bool odd = (i % 2) == 1;
        draw::capsule(cv, u0, zy + (odd ? zamp : -zamp), u1, zy + (odd ? -zamp : zamp), rad, hex(kWhite));
    }
    // a small tree on the front: three tiers and a star
    struct Tier { float apex, base, hw; };
    constexpr Tier kTiers[3] = {{0.16f, 0.36f, 0.17f}, {0.28f, 0.54f, 0.25f}, {0.42f, 0.74f, 0.34f}};
    for (int pass = 0; pass < 2; ++pass) {  // outlines first, then fills
        for (const Tier& tr : kTiers) {
            Pt tri[3] = {{cx, cy + tr.apex * ry}, {cx + tr.hw * rx, cy + tr.base * ry}, {cx - tr.hw * rx, cy + tr.base * ry}};
            if (pass == 0) convex_fill(cv, tri, 3, kTreeDark, kTreeDark, 0.9f);
            else convex_fill(cv, tri, 3, kTree, kTreeDark, 0.0f);
        }
    }
    draw::circle(cv, cx - 0.1f * rx, cy + 0.46f * ry, 0.05f * rx + 0.3f, hex(0xFF5A5A));
    draw::circle(cv, cx + 0.12f * rx, cy + 0.60f * ry, 0.05f * rx + 0.3f, hex(0x5CC8FF));
    star(cv, cx, cy + 0.10f * ry, 0.14f * rx + 0.8f, 0.07f * rx + 0.4f, -kPi / 2, kStarDark, 256);
    star(cv, cx, cy + 0.10f * ry, 0.14f * rx, 0.07f * rx, -kPi / 2, kStar, 256);
}

void swim_ring(const Canvas& cv, float cx, float cy, float rx, std::uint32_t t) noexcept {
    constexpr int kSeg = 16;
    constexpr std::uint32_t kRed = 0xE8384F, kRedBack = 0xB0283A, kWhite = 0xFFFFFF, kWhiteBack = 0xB9C0CC;
    constexpr std::uint32_t kLine = 0x7A1A28;
    const float ax = 0.86f * rx, ay = 0.24f * rx, tube = 0.14f * rx;  // tube centre path and radius
    auto at = [&](float a) noexcept -> Pt { return {cx + ax * std::cos(a), cy + ay * std::sin(a)}; };
    // dark outline of the whole ring first
    for (int i = 0; i < kSeg; ++i) {
        const Pt p = at(2 * kPi * static_cast<float>(i) / kSeg), q = at(2 * kPi * static_cast<float>(i + 1) / kSeg);
        draw::capsule(cv, p.x, p.y, q.x, q.y, tube + 0.9f, hex(kLine));
    }
    // back half first (thinner, darker), then the front half
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < kSeg; ++i) {
            const float mid = 2 * kPi * (static_cast<float>(i) + 0.5f) / kSeg;
            const bool back = std::sin(mid) < 0;
            if ((pass == 0) != back) continue;
            const Pt p = at(2 * kPi * static_cast<float>(i) / kSeg), q = at(2 * kPi * static_cast<float>(i + 1) / kSeg);
            const bool red = (i % 2) == 0;
            const std::uint32_t col = back ? (red ? kRedBack : kWhiteBack) : (red ? kRed : kWhite);
            draw::capsule(cv, p.x, p.y, q.x, q.y, back ? tube * 0.8f : tube, hex(col));
        }
    }
    // a white highlight gliding along the front of the tube
    const float hz = 2.2f + 0.5f * wave(t, 3000);
    const Pt h0 = at(hz), h1 = at(hz + 0.35f);
    draw::capsule(cv, h0.x, h0.y - 0.45f * tube, h1.x, h1.y - 0.45f * tube, 0.22f * tube, hex(kWhite), 170);
}

}  // namespace screen::props
