// screen: friendship props for the day page, see props.hpp. Pure C++.
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

// Phase of a repeating cycle, 0..1.
float ph(std::uint32_t t, std::uint32_t period) noexcept {
    return static_cast<float>(t % period) / static_cast<float>(period);
}

// Convex polygon in screen pixels: a darker outline grown by g px from the centroid, then the fill.
void outlined(const Canvas& cv, const Pt* in, int n, std::uint32_t c, std::uint32_t dark, float g) noexcept {
    if (n < 3) return;
    Pt cen{0, 0};
    for (int i = 0; i < n; ++i) {
        cen.x += in[i].x / static_cast<float>(n);
        cen.y += in[i].y / static_cast<float>(n);
    }
    if (g > 0.05f) {
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

// Standing props: design units, u to the right (mirrored when f < 0), v up from the row `by` they sit on.
// 1 unit = 1 px at s = 1.
struct Stand {
    const Canvas& cv;
    float x, by, s, f;
    float X(float u) const noexcept { return x + u * f * s; }
    float Y(float v) const noexcept { return by - v * s; }
    void disc(float u, float v, float r, std::uint32_t c, std::uint32_t dark, float g = 0.8f) const noexcept {
        if (g > 0) draw::circle(cv, X(u), Y(v), (r + g) * s, hex(dark));
        draw::circle(cv, X(u), Y(v), r * s, hex(c));
    }
    void oval(float u, float v, float rx, float ry, float ang, std::uint32_t c, std::uint32_t dark, float g = 0.8f,
              int a = 256) const noexcept {
        if (g > 0) draw::ellipse(cv, X(u), Y(v), (rx + g) * s, (ry + g) * s, hex(dark), ang * f, a);
        draw::ellipse(cv, X(u), Y(v), rx * s, ry * s, hex(c), ang * f, a);
    }
    void poly(const Pt* d, int n, std::uint32_t c, std::uint32_t dark, float g = 0.8f) const noexcept {
        Pt in[16];
        const int m = std::min(n, 16);
        for (int i = 0; i < m; ++i) in[i] = {X(d[i].x), Y(d[i].y)};
        outlined(cv, in, m, c, dark, g * s);
    }
    void glint(float u, float v, float r, int a = 190) const noexcept {
        draw::circle(cv, X(u), Y(v), r * s, hex(0xFFFFFF), a);
    }
};

// Small icons: design units with v down, rotated by `ang` around (x, y); 1 unit = 1 px at s = 1.
struct Icon {
    const Canvas& cv;
    float x, y, s, ang, cos_a, sin_a;
    Icon(const Canvas& canvas, float px, float py, float sc, float a) noexcept
        : cv(canvas), x(px), y(py), s(sc), ang(a), cos_a(std::cos(a)), sin_a(std::sin(a)) {}
    Pt P(float u, float v) const noexcept {
        return {x + (u * cos_a - v * sin_a) * s, y + (u * sin_a + v * cos_a) * s};
    }
    void ring(float u, float v, float r, std::uint32_t dark, float g = 0.6f) const noexcept {
        const Pt p = P(u, v);
        draw::circle(cv, p.x, p.y, (r + g) * s, hex(dark));
    }
    void disc(float u, float v, float r, std::uint32_t c) const noexcept {
        const Pt p = P(u, v);
        draw::circle(cv, p.x, p.y, r * s, hex(c));
    }
    void dot(float u, float v, float r, std::uint32_t c, std::uint32_t dark) const noexcept {
        ring(u, v, r, dark);
        disc(u, v, r, c);
    }
    void ell(float u, float v, float rx, float ry, float a, std::uint32_t c, std::uint32_t dark) const noexcept {
        const Pt p = P(u, v);
        draw::ellipse(cv, p.x, p.y, (rx + 0.6f) * s, (ry + 0.6f) * s, hex(dark), ang + a);
        draw::ellipse(cv, p.x, p.y, rx * s, ry * s, hex(c), ang + a);
    }
    void ell_fill(float u, float v, float rx, float ry, float a, std::uint32_t c) const noexcept {
        const Pt p = P(u, v);
        draw::ellipse(cv, p.x, p.y, rx * s, ry * s, hex(c), ang + a);
    }
    void seg(float u0, float v0, float u1, float v1, float r, std::uint32_t c, std::uint32_t dark) const noexcept {
        const Pt p0 = P(u0, v0), p1 = P(u1, v1);
        draw::capsule(cv, p0.x, p0.y, p1.x, p1.y, (r + 0.6f) * s, hex(dark));
        draw::capsule(cv, p0.x, p0.y, p1.x, p1.y, r * s, hex(c));
    }
    void line(float u0, float v0, float u1, float v1, float r, std::uint32_t c) const noexcept {
        const Pt p0 = P(u0, v0), p1 = P(u1, v1);
        draw::capsule(cv, p0.x, p0.y, p1.x, p1.y, r * s, hex(c));
    }
    void poly(const Pt* d, int n, std::uint32_t c, std::uint32_t dark) const noexcept {
        Pt in[16];
        const int m = std::min(n, 16);
        for (int i = 0; i < m; ++i) in[i] = P(d[i].x, d[i].y);
        outlined(cv, in, m, c, dark, 0.6f * s);
    }
    void poly_fill(const Pt* d, int n, std::uint32_t c) const noexcept {
        Pt in[16];
        const int m = std::min(n, 16);
        for (int i = 0; i < m; ++i) in[i] = P(d[i].x, d[i].y);
        draw::polygon(cv, in, m, hex(c));
    }
    void glint(float u, float v, float r, int a = 190) const noexcept {
        const Pt p = P(u, v);
        draw::circle(cv, p.x, p.y, r * s, hex(0xFFFFFF), a);
    }
};

}  // namespace

// ---- the fox ---------------------------------------------------------------------------------------------------

void fox(const Canvas& cv, float x, float base_y, float s, float face, std::uint32_t t) noexcept {
    const float f = face < 0.0f ? -1.0f : 1.0f;
    const Stand p{cv, x, base_y, s, f};
    constexpr std::uint32_t kOrange = 0xF07A2A, kOrangeDark = 0xB5501E, kHaunch = 0xE5661E;
    constexpr std::uint32_t kCream = 0xFFF4E6, kCreamDark = 0xE2C7A6, kInk = 0x2A1A14, kFoot = 0x4A2A1A;

    // Shadow on the desk.
    draw::ellipse(cv, p.X(0), p.Y(0.4f), 11.0f * s, 1.6f * s, hex(0x3A2416), 0, 70);

    // Tail: a bushy curve from the rump, the tip swishes; the last segment is white.
    {
        const float sw = 0.16f * std::sin(2.0f * kPi * ph(t, 2600));
        const float cs = std::cos(sw), sn = std::sin(sw);
        const float ru = -5.0f, rv = 3.2f;  // rump
        auto turn = [&](float u, float v) noexcept -> Pt {
            const float du = u - ru, dv = v - rv;
            return {ru + du * cs - dv * sn, rv + du * sn + dv * cs};
        };
        const Pt c1 = turn(-12.6f, 0.2f), e = turn(-15.8f, 7.6f);
        Pt q[6];  // the curve from the rump to the tip, screen pixels
        for (int k = 0; k < 6; ++k) {
            const float u = static_cast<float>(k) / 5.0f, a = (1 - u) * (1 - u), b = 2 * (1 - u) * u, c2 = u * u;
            q[k] = {p.X(a * ru + b * c1.x + c2 * e.x), p.Y(a * rv + b * c1.y + c2 * e.y)};
        }
        auto rr = [&](int k) noexcept { return (3.8f - 0.22f * static_cast<float>(k)) * s; };
        // outline first, then the orange part, then the white tip
        for (int k = 0; k < 5; ++k)
            draw::capsule(cv, q[k].x, q[k].y, q[k + 1].x, q[k + 1].y, 0.5f * (rr(k) + rr(k + 1)) + 0.8f * s,
                          hex(kOrangeDark));
        for (int k = 0; k < 6; ++k) draw::circle(cv, q[k].x, q[k].y, rr(k) + 0.8f * s, hex(kOrangeDark));
        for (int k = 0; k < 4; ++k)
            draw::capsule(cv, q[k].x, q[k].y, q[k + 1].x, q[k + 1].y, 0.5f * (rr(k) + rr(k + 1)), hex(kOrange));
        for (int k = 0; k < 5; ++k) draw::circle(cv, q[k].x, q[k].y, rr(k), hex(kOrange));
        draw::capsule(cv, q[4].x, q[4].y, q[5].x, q[5].y, 0.5f * (rr(4) + rr(5)), hex(kCream));
        draw::circle(cv, q[5].x, q[5].y, rr(5), hex(kCream));
    }

    // Feet, haunches, body and the cream chest.
    p.oval(-3.6f, 1.3f, 2.6f, 1.4f, 0, kFoot, 0x2A1408, 0.4f);
    p.oval(3.6f, 1.3f, 2.6f, 1.4f, 0, kFoot, 0x2A1408, 0.4f);
    p.oval(0, 8.6f, 7.4f, 8.6f, 0, kOrange, kOrangeDark, 0.9f);
    p.oval(-4.6f, 4.6f, 3.2f, 3.9f, 0, kHaunch, kOrangeDark, 0.4f);
    p.oval(4.6f, 4.6f, 3.2f, 3.9f, 0, kHaunch, kOrangeDark, 0.4f);
    p.oval(0, 9.2f, 4.3f, 6.2f, 0, kCream, kCreamDark, 0.4f);

    // Ears, pointed, with dark tips. Drawn before the head so the head covers their bases.
    const Pt earL[3] = {{-7.0f, 23.2f}, {-6.6f, 31.0f}, {-1.4f, 25.4f}};
    const Pt earR[3] = {{7.0f, 23.2f}, {6.6f, 31.0f}, {1.4f, 25.4f}};
    const Pt tipL[3] = {{-6.6f, 31.0f}, {-6.76f, 27.88f}, {-4.52f, 28.76f}};
    const Pt tipR[3] = {{6.6f, 31.0f}, {6.76f, 27.88f}, {4.52f, 28.76f}};
    p.poly(earL, 3, kOrange, kOrangeDark);
    p.poly(earR, 3, kOrange, kOrangeDark);
    p.poly(tipL, 3, 0x4A2818, 0x4A2818, 0);
    p.poly(tipR, 3, 0x4A2818, 0x4A2818, 0);

    // Head with cheeks, muzzle, nose and blush.
    p.oval(0, 19.0f, 7.6f, 6.0f, 0, kOrange, kOrangeDark, 0.9f);
    p.oval(-4.3f, 16.4f, 3.0f, 2.3f, 0, kCream, kCreamDark, 0.3f);
    p.oval(4.3f, 16.4f, 3.0f, 2.3f, 0, kCream, kCreamDark, 0.3f);
    p.oval(0, 14.6f, 2.6f, 1.8f, 0, kCream, kCreamDark, 0.3f);
    p.oval(0, 16.4f, 1.3f, 0.95f, 0, kInk, kInk, 0);
    p.oval(-4.9f, 16.6f, 1.3f, 0.8f, 0, 0xFF8FA0, 0xFF8FA0, 0, 110);
    p.oval(4.9f, 16.6f, 1.3f, 0.8f, 0, 0xFF8FA0, 0xFF8FA0, 0, 110);

    // Eyes: they blink for 160 ms about every 3.8 s.
    const std::uint32_t bt = t % 3800;
    const float blink = bt < 160 ? std::sin(kPi * static_cast<float>(bt) / 160.0f) : 0.0f;
    const float open = std::max(0.14f, 1.0f - blink);
    for (const float sx : {-1.0f, 1.0f}) {
        p.oval(sx * 2.9f, 19.4f, 1.15f, 1.45f * open, 0, kInk, kInk, 0);
        if (open > 0.5f) p.glint(sx * 2.9f + 0.4f, 19.4f + 0.5f, 0.42f, 230);
    }
}

// ---- the present -----------------------------------------------------------------------------------------------

void present(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept {
    const Stand p{cv, x, base_y, s, 1.0f};
    constexpr std::uint32_t kBox = 0xB7A4F0, kBoxDark = 0x7E68C8, kLid = 0xCDBFFA;
    constexpr std::uint32_t kRibbon = 0xFFD24A, kRibbonDark = 0xC9941A;

    p.oval(0, 0.2f, 7.8f, 1.2f, 0, 0x3A2416, 0x3A2416, 0, 70);  // shadow

    const Pt body[4] = {{-6.0f, 0.0f}, {6.0f, 0.0f}, {6.0f, 8.0f}, {-6.0f, 8.0f}};
    const Pt lid[4] = {{-6.8f, 8.0f}, {6.8f, 8.0f}, {6.8f, 10.2f}, {-6.8f, 10.2f}};
    p.poly(body, 4, kBox, kBoxDark, 0.6f);
    p.poly(lid, 4, kLid, kBoxDark, 0.6f);

    // Ribbon cross: a band down the middle of the lid and box, and one across the front of the box.
    const Pt rv[4] = {{-1.1f, 0.0f}, {1.1f, 0.0f}, {1.1f, 10.2f}, {-1.1f, 10.2f}};
    const Pt rh[4] = {{-6.0f, 3.2f}, {6.0f, 3.2f}, {6.0f, 4.6f}, {-6.0f, 4.6f}};
    p.poly(rh, 4, kRibbon, kRibbonDark, 0.4f);
    p.poly(rv, 4, kRibbon, kRibbonDark, 0.4f);

    // Bow: two loops and a knot on the lid.
    p.oval(-2.8f, 12.4f, 2.9f, 1.7f, 0.5f, kRibbon, kRibbonDark, 0.6f);
    p.oval(2.8f, 12.4f, 2.9f, 1.7f, -0.5f, kRibbon, kRibbonDark, 0.6f);
    p.disc(0, 11.6f, 1.5f, kRibbon, kRibbonDark, 0.5f);

    // One twinkle, up and to the right of the box, fading in and out every 2 s.
    const float k = std::max(0.0f, std::sin(2.0f * kPi * ph(t, 2000)));
    if (k > 0.02f)
        draw::sparkle(cv, p.X(9.5f), p.Y(14.5f), (1.5f + 2.0f * k) * s, hex(0xFFC233),
                      std::min(256, static_cast<int>(256.0f * k)));
}

// ---- stubs for the remaining props (to be filled in) ---------------------------------------------------------

// ---- gifts and the shelf ----------------------------------------------------------------------------------------

void gift_icon(const Canvas& cv, float x, float y, float s, int i) noexcept {
    const Icon g{cv, x, y, s, 0.0f};
    constexpr std::uint32_t kGreen = 0x58B848, kGreenDark = 0x2F7A2A;
    switch (i) {
        case 0: {  // flower: stem and leaf, five petals, yellow centre
            g.seg(0.2f, 1.6f, 0.4f, 5.6f, 0.6f, kGreen, kGreenDark);
            g.ell(1.9f, 4.0f, 1.8f, 0.9f, -0.5f, 0x7ED957, 0x3F9A2A);
            constexpr float kPetalRing = 2.9f, kPetalR = 2.1f, kCy = -0.6f;
            for (int k = 0; k < 5; ++k) {
                const float a = -kPi / 2 + static_cast<float>(k) * 2 * kPi / 5;
                g.ring(kPetalRing * std::cos(a), kCy + kPetalRing * std::sin(a), kPetalR, 0xC2437A);
            }
            for (int k = 0; k < 5; ++k) {
                const float a = -kPi / 2 + static_cast<float>(k) * 2 * kPi / 5;
                g.disc(kPetalRing * std::cos(a), kCy + kPetalRing * std::sin(a), kPetalR, 0xF78FB3);
            }
            g.dot(0, kCy, 1.9f, 0xFFD23F, 0xC9961A);
            break;
        }
        case 1: {  // pebble: smooth grey-blue oval with a highlight
            g.ell(0, 0.4f, 4.4f, 3.3f, -0.25f, 0x8FA3B8, 0x55667E);
            g.glint(-1.4f, -0.8f, 1.1f, 210);
            g.glint(-0.4f, -2.0f, 0.4f, 170);
            break;
        }
        case 2: {  // feather: two light vanes around a quill
            const Pt vl[5] = {{-3.6f, 4.4f}, {-5.2f, 1.2f}, {-4.2f, -1.6f}, {-1.4f, -3.8f}, {3.6f, -4.6f}};
            const Pt vr[5] = {{-3.6f, 4.4f}, {-1.2f, 3.8f}, {1.8f, 1.4f}, {3.6f, -1.0f}, {3.6f, -4.6f}};
            g.poly(vr, 5, 0xD9EAFB, 0x8FA8C6);
            g.poly(vl, 5, 0xFFFFFF, 0x8FA8C6);
            g.seg(-3.6f, 4.4f, 3.6f, -4.6f, 0.45f, 0x9AA7B8, 0x6E7F96);
            break;
        }
        case 3: {  // sea shell: peach scallop fan with ridges
            constexpr float kHy = 3.6f, kR = 5.4f;
            Pt sh[9];
            sh[0] = {0.0f, kHy};
            for (int k = 0; k < 8; ++k) {
                const float a = -0.88f * kPi + 0.76f * kPi * static_cast<float>(k) / 7.0f;
                sh[k + 1] = {kR * std::cos(a), kHy + kR * std::sin(a)};
            }
            g.poly(sh, 9, 0xFFB48A, 0xC7714A);
            for (const float a : {-0.7f * kPi, -0.5f * kPi, -0.3f * kPi})
                g.line(0, kHy, 4.3f * std::cos(a), kHy + 4.3f * std::sin(a), 0.35f, 0xE0885F);
            break;
        }
        case 4: {  // pine cone: brown cone with rows of scales
            const Pt cone[8] = {{0.0f, -5.4f}, {2.8f, -2.6f}, {3.6f, 0.4f}, {2.6f, 3.4f},
                                {0.0f, 5.0f},  {-2.6f, 3.4f}, {-3.6f, 0.4f}, {-2.8f, -2.6f}};
            g.poly(cone, 8, 0x9A6A3A, 0x5A3A1C);
            for (const float vy : {-1.6f, 0.8f, 3.0f}) {
                const Pt c = g.P(0, vy - 4.5f);
                draw::arc(cv, c.x, c.y, 4.5f * s, 0.3f * kPi, 0.7f * kPi, 0.4f * s, hex(0x5A3A1C), 230, 4);
            }
            break;
        }
        case 5: {  // heart: two glossy lobes and a triangle
            const Pt tri[3] = {{-4.5f, -0.4f}, {4.5f, -0.4f}, {0.0f, 4.8f}};
            const Pt triBig[3] = {{-5.1f, -0.4f}, {5.1f, -0.4f}, {0.0f, 5.4f}};
            g.ring(-2.1f, -1.2f, 2.5f, 0x9E1F2E);
            g.ring(2.1f, -1.2f, 2.5f, 0x9E1F2E);
            g.poly_fill(triBig, 3, 0x9E1F2E);
            g.disc(-2.1f, -1.2f, 2.5f, 0xE23A4A);
            g.disc(2.1f, -1.2f, 2.5f, 0xE23A4A);
            g.poly_fill(tri, 3, 0xE23A4A);
            g.glint(-2.4f, -2.6f, 0.9f, 220);
            g.glint(-1.2f, -3.6f, 0.4f, 170);
            break;
        }
        default:
            break;
    }
}

void gift_shelf(const Canvas& cv, float x, float y, float w, unsigned gifts) noexcept {
    constexpr std::uint32_t kWood = 0xC9975E, kWoodFront = 0x9A6A3C, kWoodDark = 0x6B4424;
    const float h = std::max(3.0f, w * 0.07f);  // board thickness
    const float left = x - w * 0.5f, right = x + w * 0.5f;
    const float top = y - h * 0.5f, bottom = y + h * 0.5f;

    // Brackets under the board.
    const float bw = std::max(2.5f, w * 0.09f), bd = std::max(3.0f, w * 0.1f);
    for (const float bx : {x - w * 0.36f, x + w * 0.36f}) {
        const Pt br[3] = {{bx - bw * 0.5f, bottom - 0.5f}, {bx + bw * 0.5f, bottom - 0.5f}, {bx - bw * 0.5f, bottom + bd}};
        draw::polygon(cv, br, 3, hex(kWoodDark));
    }

    // Board with a darker front edge.
    const Pt board[4] = {{left, top}, {right, top}, {right, bottom}, {left, bottom}};
    outlined(cv, board, 4, kWood, kWoodDark, 0.7f);
    const Pt edge[4] = {{left, bottom - h * 0.4f}, {right, bottom - h * 0.4f}, {right, bottom}, {left, bottom}};
    draw::polygon(cv, edge, 4, hex(kWoodFront));

    // Gifts stand on the board, spread evenly, six fit across.
    int n = 0;
    for (int i = 0; i < 6; ++i) n += static_cast<int>((gifts >> i) & 1u);
    if (n == 0) return;
    const float s = w / 66.0f;
    int k = 0;
    for (int i = 0; i < 6; ++i) {
        if (((gifts >> i) & 1u) == 0) continue;
        const float gx = left + (static_cast<float>(k) + 0.5f) * w / static_cast<float>(n);
        gift_icon(cv, gx, top - 5.4f * s, s, i);
        ++k;
    }
}

// ---- treats -----------------------------------------------------------------------------------------------------

void treat(const Canvas& cv, float x, float y, float s, int i, float angle) noexcept {
    const Icon g{cv, x, y, s, angle};
    switch (i) {
        case 0: {  // acorn: nut with a cap and a stem
            g.seg(0.0f, -3.6f, 0.9f, -5.4f, 0.6f, 0x6A4A2A, 0x2E1A0A);
            g.ell(0, 1.8f, 3.3f, 3.6f, 0, 0xC98A3F, 0x7A4B1A);
            g.ell(0, -1.6f, 3.9f, 2.1f, 0, 0x6E4A2A, 0x3E2610);
            for (const float dx : {-2.0f, 0.0f, 2.0f}) g.disc(dx, -0.3f, 0.35f, 0x2E1A0A);
            g.glint(-1.2f, 1.2f, 0.8f, 170);
            break;
        }
        case 1: {  // hazelnut: round brown nut with a lighter top patch
            g.dot(0, 0.2f, 3.9f, 0xB07438, 0x6B4220);
            g.ell_fill(-0.7f, -1.4f, 2.0f, 1.2f, -0.3f, 0xD9A46A);
            g.glint(-1.6f, -1.8f, 0.5f, 170);
            break;
        }
        case 2: {  // raspberry: cluster of red drupelets with a green leaf on top
            constexpr float kPx[7] = {-1.3f, 1.3f, -2.5f, 0.0f, 2.5f, -1.3f, 1.3f};
            constexpr float kPy[7] = {-1.8f, -1.8f, 0.0f, 0.0f, 0.0f, 1.8f, 1.8f};
            g.ell(1.2f, -4.0f, 2.6f, 1.0f, -0.35f, 0x58B848, 0x2F7A2A);
            g.ell(-1.2f, -4.0f, 2.4f, 0.9f, 0.35f, 0x58B848, 0x2F7A2A);
            g.line(0, -3.2f, 0, -4.4f, 0.35f, 0x2F7A2A);
            for (int k = 0; k < 7; ++k) g.ring(kPx[k], kPy[k], 1.6f, 0x8E1430);
            for (int k = 0; k < 7; ++k) g.disc(kPx[k], kPy[k], 1.6f, 0xD62A4A);
            for (int k = 0; k < 7; ++k) g.glint(kPx[k] - 0.5f, kPy[k] - 0.5f, 0.35f, 170);
            break;
        }
        default:
            break;
    }
}

// ---- the nightcap and the bow ----------------------------------------------------------------------------------

void nightcap(const Canvas& cv, float cx, float brim_y, float w) noexcept {
    constexpr std::uint32_t kBlue = 0x5B7BD5, kBlueDark = 0x34489A, kBlueLight = 0xA6BEF2;
    constexpr std::uint32_t kWhite = 0xFFFFFF, kWhiteDark = 0xC3CCDB;

    // The cone is a tapering stroke along a spine that rises, then flops over to the right.
    const Pt sp[6] = {
        {cx - 0.05f * w, brim_y - 0.04f * w}, {cx - 0.16f * w, brim_y - 0.46f * w},
        {cx - 0.02f * w, brim_y - 0.86f * w}, {cx + 0.26f * w, brim_y - 0.92f * w},
        {cx + 0.44f * w, brim_y - 0.64f * w}, {cx + 0.46f * w, brim_y - 0.42f * w},
    };
    constexpr float kRad[6] = {0.40f, 0.36f, 0.31f, 0.24f, 0.18f, 0.13f};  // times w
    auto r = [&](int k) noexcept { return kRad[k] * w; };
    constexpr float kOut = 1.4f;

    // Outline pass, then the fill.
    for (int k = 0; k < 5; ++k)
        draw::capsule(cv, sp[k].x, sp[k].y, sp[k + 1].x, sp[k + 1].y, 0.5f * (r(k) + r(k + 1)) + kOut,
                      hex(kBlueDark));
    for (int k = 0; k < 6; ++k) draw::circle(cv, sp[k].x, sp[k].y, r(k) + kOut, hex(kBlueDark));
    for (int k = 0; k < 5; ++k)
        draw::capsule(cv, sp[k].x, sp[k].y, sp[k + 1].x, sp[k + 1].y, 0.5f * (r(k) + r(k + 1)), hex(kBlue));
    for (int k = 0; k < 6; ++k) draw::circle(cv, sp[k].x, sp[k].y, r(k), hex(kBlue));

    // Lighter stripes across the cone, perpendicular to the spine.
    for (const int k : {1, 3}) {
        float dx = sp[k + 1].x - sp[k - 1].x, dy = sp[k + 1].y - sp[k - 1].y;
        const float len = std::sqrt(dx * dx + dy * dy) + 1e-3f;
        const float nx = -dy / len * r(k) * 0.95f, ny = dx / len * r(k) * 0.95f;
        draw::capsule(cv, sp[k].x - nx, sp[k].y - ny, sp[k].x + nx, sp[k].y + ny, 1.1f, hex(kBlueLight));
    }

    // Fluffy white brim along brim_y, spanning the width.
    const float br = 0.1f * w;
    const float x0 = cx - 0.5f * w + br, x1 = cx + 0.5f * w - br;
    for (int k = 0; k <= 6; ++k) {
        const float bx = x0 + (x1 - x0) * static_cast<float>(k) / 6.0f;
        draw::circle(cv, bx, brim_y, br + 1.2f, hex(kWhiteDark));
    }
    draw::capsule(cv, x0, brim_y, x1, brim_y, br + 1.2f, hex(kWhiteDark));
    draw::capsule(cv, x0, brim_y, x1, brim_y, br, hex(kWhite));
    for (int k = 0; k <= 6; ++k) {
        const float bx = x0 + (x1 - x0) * static_cast<float>(k) / 6.0f;
        draw::circle(cv, bx, brim_y, br, hex(kWhite));
    }

    // Pom-pom at the drooping tip.
    const Pt tip = sp[5];
    draw::circle(cv, tip.x + 0.02f * w, tip.y + 0.08f * w, 0.14f * w + 1.2f, hex(kWhiteDark));
    draw::circle(cv, tip.x + 0.02f * w, tip.y + 0.08f * w, 0.14f * w, hex(kWhite));
    draw::circle(cv, tip.x - 0.02f * w, tip.y + 0.04f * w, 0.05f * w, hex(0xEEF2F8));
}

void bow(const Canvas& cv, float x, float y, float s) noexcept {
    const Icon g{cv, x, y, s, 0.0f};
    constexpr std::uint32_t kPink = 0xFF8CC0, kPinkDark = 0xB8407C, kKnot = 0xE8509A, kKnotDark = 0xA8306A;

    // Tails hang from the knot, behind the loops.
    const Pt tailL[4] = {{-0.6f, 0.8f}, {-2.6f, 4.6f}, {-1.2f, 5.2f}, {0.2f, 1.6f}};
    const Pt tailR[4] = {{0.6f, 0.8f}, {2.6f, 4.6f}, {1.2f, 5.2f}, {-0.2f, 1.6f}};
    g.poly(tailL, 4, kKnot, kKnotDark);
    g.poly(tailR, 4, kKnot, kKnotDark);

    // Two loops, tilted up and out.
    g.ell(-3.0f, -0.2f, 3.1f, 2.1f, 0.45f, kPink, kPinkDark);
    g.ell(3.0f, -0.2f, 3.1f, 2.1f, -0.45f, kPink, kPinkDark);
    g.glint(-4.4f, -1.0f, 0.6f, 160);
    g.glint(4.4f, -1.0f, 0.6f, 160);

    // Knot on top.
    g.dot(0, 0, 1.6f, kKnot, kKnotDark);
    g.glint(-0.5f, -0.5f, 0.4f, 170);
}

}  // namespace screen::props
