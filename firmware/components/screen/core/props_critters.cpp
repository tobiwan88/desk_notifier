// screen: creature props for the day page, see props.hpp. Pure C++.
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

// Phase of a repeating cycle, 0..1, and a sine over it.
float ph(std::uint32_t t, std::uint32_t period) noexcept {
    return static_cast<float>(t % period) / static_cast<float>(period);
}
float sin_ph(std::uint32_t t, std::uint32_t period) noexcept { return std::sin(2 * kPi * ph(t, period)); }

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

// Design units: u to the right (mirrored by f), v up. v = 0 sits on the screen row `by`; 1 unit = 1 px at s = 1.
// Solid shapes are drawn twice: a slightly larger shape in the darker colour, then the fill.
struct Stage {
    const Canvas& cv;
    float x, by, s, f;
    float X(float u) const noexcept { return x + u * f * s; }
    float Y(float v) const noexcept { return by - v * s; }
    void oval(float u, float v, float rx, float ry, std::uint32_t c, std::uint32_t dark, float g = 0.8f,
              int a = 256) const noexcept {
        if (g > 0) draw::ellipse(cv, X(u), Y(v), (rx + g) * s, (ry + g) * s, hex(dark), 0, a);
        draw::ellipse(cv, X(u), Y(v), rx * s, ry * s, hex(c), 0, a);
    }
    void disc(float u, float v, float r, std::uint32_t c, std::uint32_t dark, float g = 0.8f) const noexcept {
        if (g > 0) draw::circle(cv, X(u), Y(v), (r + g) * s, hex(dark));
        draw::circle(cv, X(u), Y(v), r * s, hex(c));
    }
    void stroke(float u0, float v0, float u1, float v1, float r, std::uint32_t c, std::uint32_t dark,
                float g = 0.8f) const noexcept {
        if (g > 0) draw::capsule(cv, X(u0), Y(v0), X(u1), Y(v1), (r + g) * s, hex(dark));
        draw::capsule(cv, X(u0), Y(v0), X(u1), Y(v1), r * s, hex(c));
    }
    void poly(const Pt* d, int n, std::uint32_t c, std::uint32_t dark, float g = 0.8f) const noexcept {
        Pt in[16];
        const int m = std::min(n, 16);
        for (int i = 0; i < m; ++i) in[i] = {X(d[i].x), Y(d[i].y)};
        outlined(cv, in, m, c, dark, g * s);
    }
    void glint(float u, float v, float r, int a = 170) const noexcept {
        draw::circle(cv, X(u), Y(v), r * s, hex(0xFFFFFF), a);
    }
};

}  // namespace

// ---- owl -----------------------------------------------------------------------------------------------------

void owl(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept {
    const Stage p{cv, x, base_y, s, 1.0f};

    // Blink: the eyes shut for 150 ms every 3.6 s.
    const std::uint32_t bt = t % 3600;
    const float blink = bt < 150 ? std::sin(kPi * static_cast<float>(bt) / 150.0f) : 0.0f;
    const float open = 1.0f - 0.9f * blink;
    // Head tilt: a lean of up to 0.12 rad for 1.4 s every 7 s.
    const std::uint32_t lt = t % 7000;
    const float lean = lt < 1400 ? 0.12f * std::sin(kPi * static_cast<float>(lt) / 1400.0f) : 0.0f;
    const float cs = std::cos(lean), sn = std::sin(lean);
    const Pt pivot{p.X(0), p.Y(12)};
    // Head points, turned by the lean around the neck.
    auto H = [&](float u, float v) noexcept -> Pt {
        const float dx = p.X(u) - pivot.x, dy = p.Y(v) - pivot.y;
        return {pivot.x + dx * cs - dy * sn, pivot.y + dx * sn + dy * cs};
    };

    // Short branch stub, then the feet gripping it.
    p.stroke(-9.0f, 2.6f, 9.0f, 0.9f, 1.3f, 0x8B5E34, 0x4E3218);
    p.oval(-2.6f, 2.9f, 1.8f, 1.1f, 0xF2A93B, 0xB8741C, 0.5f);
    p.oval(2.6f, 2.9f, 1.8f, 1.1f, 0xF2A93B, 0xB8741C, 0.5f);

    // Body, belly, wings folded at the sides.
    p.oval(0, 10.5f, 8.2f, 8.8f, 0x9C6B3E, 0x5E3D1E);
    p.oval(0, 6.6f, 5.0f, 5.2f, 0xE8C896, 0xE8C896, 0);
    p.stroke(-2.0f, 5.9f, -1.3f, 4.6f, 0.3f, 0xB58450, 0xB58450, 0);
    p.stroke(2.0f, 5.9f, 1.3f, 4.6f, 0.3f, 0xB58450, 0xB58450, 0);
    p.oval(-5.8f, 9.0f, 2.2f, 5.0f, 0x7A4D28, 0x4A2C14, 0.5f);
    p.oval(5.8f, 9.0f, 2.2f, 5.0f, 0x7A4D28, 0x4A2C14, 0.5f);

    // Head group: face disc, ear tufts, beak and eyes, all turned by the lean.
    {
        const Pt q = H(0, 14.6f);
        draw::ellipse(cv, q.x, q.y, 7.8f * s, 5.8f * s, hex(0x8A5A2A), lean);
        draw::ellipse(cv, q.x, q.y, 7.0f * s, 5.4f * s, hex(0xE9C58E), lean);
    }
    for (const float sx : {-1.0f, 1.0f}) {
        const Pt tuft[3] = {H(sx * 7.0f, 15.6f), H(sx * 3.8f, 17.6f), H(sx * 6.0f, 21.6f)};
        outlined(cv, tuft, 3, 0x9C6B3E, 0x5E3D1E, 0.8f * s);
    }
    {
        const Pt beak[3] = {H(-1.2f, 14.4f), H(1.2f, 14.4f), H(0, 11.6f)};
        outlined(cv, beak, 3, 0xF2A93B, 0xB8741C, 0.5f * s);
    }
    for (const float u : {-3.9f, 3.9f}) {
        const Pt q = H(u, 16.0f);
        draw::ellipse(cv, q.x, q.y, 3.9f * s, 3.9f * s * open, hex(0x5E3D1E), lean);
        draw::ellipse(cv, q.x, q.y, 3.3f * s, 3.3f * s * open, hex(0xFFC93C), lean);
        draw::ellipse(cv, q.x, q.y, 1.9f * s, 1.9f * s * open, hex(0x1A120A), lean);
        if (open > 0.5f) draw::circle(cv, q.x + 0.8f * s, q.y - 0.8f * s, 0.6f * s, hex(0xFFFFFF), 230);
    }
}

// ---- hedgehog ------------------------------------------------------------------------------------------------

void hedgehog(const Canvas& cv, float x, float base_y, float s, float face, std::uint32_t t) noexcept {
    const Stage p{cv, x, base_y, s, face < 0 ? -1.0f : 1.0f};
    const float step = sin_ph(t, 520);  // gait, -1..1: the legs swing in pairs
    const float bob = 0.35f * std::fabs(step);

    // Far legs first (darker), then the near legs in front of the belly.
    p.stroke(-6.5f, 3.0f, -6.5f + step * 1.4f, 0.9f, 0.9f, 0x4E3018, 0x2E1C0E, 0.5f);
    p.stroke(2.5f, 3.0f, 2.5f - step * 1.4f, 0.9f, 0.9f, 0x4E3018, 0x2E1C0E, 0.5f);

    const float cy = 7.0f + bob;
    // Spines: a row of small triangles along the back, pointing back and out.
    for (int i = 0; i < 7; ++i) {
        const float a = kPi * (0.16f + 0.68f * static_cast<float>(i) / 6.0f);
        const float bx = -2.6f + 8.4f * std::cos(a), bz = cy + 4.9f * std::sin(a);
        float nx = std::cos(a) / 8.4f, nz = std::sin(a) / 4.9f;
        const float nl = std::sqrt(nx * nx + nz * nz);
        nx /= nl;
        nz /= nl;
        const float tx = -nz, tz = nx;
        const Pt tri[3] = {{bx - tx * 1.5f, bz - tz * 1.5f},
                           {bx + nx * 3.0f - 0.8f, bz + nz * 3.0f},
                           {bx + tx * 1.5f, bz + tz * 1.5f}};
        p.poly(tri, 3, (i % 2) != 0 ? 0x9A6A3C : 0x6E4826, 0x2E1A0C, 0.6f);
    }
    p.oval(-2.6f, cy, 8.4f, 4.9f, 0x6E4826, 0x3A2412);
    p.stroke(-9.0f, 3.0f, -9.0f - step * 1.4f, 0.9f, 0.9f, 0x7A4E2A, 0x3E2410, 0.5f);
    p.stroke(1.0f, 3.0f, 1.0f + step * 1.4f, 0.9f, 0.9f, 0x7A4E2A, 0x3E2410, 0.5f);

    // Cream face: round head, pointed snout, black nose dot, one eye, a small ear.
    p.oval(6.4f, cy - 1.2f, 4.0f, 3.8f, 0xF6E7C8, 0x8A6A44, 0.5f);
    const Pt snout[3] = {{9.0f, cy + 0.8f}, {14.4f, cy - 2.2f}, {9.2f, cy - 3.8f}};
    p.poly(snout, 3, 0xF6E7C8, 0x8A6A44, 0.5f);
    p.disc(14.0f, cy - 2.0f, 0.95f, 0x1E1410, 0x1E1410, 0);
    p.disc(8.0f, cy + 0.2f, 0.95f, 0x1E1410, 0x1E1410, 0);
    p.glint(8.3f, cy + 0.5f, 0.35f, 200);
    p.oval(4.6f, cy + 2.4f, 1.5f, 1.5f, 0x6E4826, 0x3A2412);
}

// ---- snail ---------------------------------------------------------------------------------------------------

void snail(const Canvas& cv, float x, float base_y, float s, float face, std::uint32_t t) noexcept {
    const Stage p{cv, x, base_y, s, face < 0 ? -1.0f : 1.0f};
    const float st = sin_ph(t, 1800);  // crawl: +1 long and thin, -1 short and fat
    const float rx = 8.6f + 1.4f * st, ry = 2.9f - 0.45f * st;

    p.oval(-7.0f, 0.6f, 6.5f, 0.6f, 0xD9E6C6, 0xD9E6C6, 0, 150);  // slime trail
    p.oval(3.0f, ry, rx, ry, 0xA3BE9B, 0x5F7D5A, 0.7f);           // soft body

    // Two eye stalks, the tips stretching with the body.
    const float tip = 10.2f + 0.7f * st;
    p.stroke(7.4f, ry + 0.5f, 7.9f, tip, 0.55f, 0xA3BE9B, 0x5F7D5A, 0.5f);
    p.stroke(9.6f, ry + 0.3f, 10.8f, tip - 0.6f, 0.55f, 0xA3BE9B, 0x5F7D5A, 0.5f);
    p.disc(7.9f, tip, 1.25f, 0xFFFFFF, 0x5F7D5A, 0.5f);
    p.disc(10.8f, tip - 0.6f, 1.25f, 0xFFFFFF, 0x5F7D5A, 0.5f);
    p.disc(8.1f, tip + 0.1f, 0.5f, 0x2A2A2A, 0x2A2A2A, 0);
    p.disc(11.0f, tip - 0.7f, 0.5f, 0x2A2A2A, 0x2A2A2A, 0);

    // Spiral shell on the back.
    const float sx = -2.5f, sy = 7.2f;
    p.disc(sx, sy, 4.6f, 0xC8782E, 0x6E3A14, 0.9f);
    p.disc(sx + 0.6f, sy + 0.4f, 2.9f, 0xE9A35A, 0xE9A35A, 0);
    draw::arc(cv, p.X(sx), p.Y(sy), 2.9f * s, 0.4f, 5.3f, 0.45f * s, hex(0x7A3E14), 255, 10);
    draw::arc(cv, p.X(sx), p.Y(sy), 1.3f * s, 5.3f, 7.9f, 0.4f * s, hex(0x7A3E14), 255, 6);
    p.glint(sx - 2.3f, sy + 2.4f, 0.9f, 170);
}

// ---- bee -----------------------------------------------------------------------------------------------------

void bee(const Canvas& cv, float x, float y, float s, std::uint32_t t) noexcept {
    const float hover = 0.6f * sin_ph(t, 900);
    const Stage p{cv, x, y - hover * s, s, 1.0f};

    // Wings flicker fast: their height pulses, the two wings out of step.
    const float k1 = 0.3f + 0.7f * std::fabs(sin_ph(t, 70));
    const float k2 = 0.3f + 0.7f * std::fabs(sin_ph(t + 35, 70));
    p.oval(1.4f, 4.0f, 2.3f, 2.0f * k2, 0xDDEFFF, 0x9CC8E6, 0.4f, 150);
    p.oval(-0.8f, 4.2f, 3.0f, 2.2f * k1, 0xE6F4FF, 0x9CC8E6, 0.4f, 170);

    p.stroke(-3.6f, 0.0f, -5.6f, 0.0f, 0.9f, 0x3A2A08, 0x3A2A08, 0);  // stinger
    p.oval(0, 0, 4.0f, 3.2f, 0xFFD23A, 0x4A3A10, 0.7f);
    p.oval(-1.4f, 0, 0.85f, 2.7f, 0x2A2010, 0x2A2010, 0);  // black stripes
    p.oval(1.2f, 0, 0.85f, 2.7f, 0x2A2010, 0x2A2010, 0);
    p.disc(3.4f, 0.2f, 2.0f, 0xFFD23A, 0x4A3A10, 0.7f);
    p.disc(4.2f, 0.6f, 0.6f, 0x1E1410, 0x1E1410, 0);
    p.glint(4.0f, 0.9f, 0.25f, 220);
    p.stroke(3.0f, 2.0f, 4.2f, 4.4f, 0.25f, 0x3A2A08, 0x3A2A08, 0);  // antennae
    p.stroke(3.6f, 2.0f, 5.4f, 3.9f, 0.25f, 0x3A2A08, 0x3A2A08, 0);
}

// ---- paper plane ---------------------------------------------------------------------------------------------

void paper_plane(const Canvas& cv, float x, float y, float s, float angle) noexcept {
    const float c = std::cos(angle), sn = std::sin(angle);
    // Design units along the flight: nose at +9, wings out to v = +-6. Result in screen pixels.
    auto P = [&](float u, float v, float ox, float oy) noexcept -> Pt {
        return {x + (u * c - v * sn) * s + ox, y + (u * sn + v * c) * s + oy};
    };
    const Pt lit[3] = {P(9, 0, 0, 0), P(-8, 6.0f, 0, 0), P(-3.4f, 0, 0, 0)};
    const Pt shade[3] = {P(9, 0, 0, 0), P(-3.4f, 0, 0, 0), P(-8, -6.0f, 0, 0)};

    // Soft shadow on the desk, offset down and to the right.
    const float ox = 1.1f * s, oy = 1.6f * s;
    const Pt sl[3] = {P(9, 0, ox, oy), P(-8, 6.0f, ox, oy), P(-3.4f, 0, ox, oy)};
    const Pt ss[3] = {P(9, 0, ox, oy), P(-3.4f, 0, ox, oy), P(-8, -6.0f, ox, oy)};
    draw::polygon(cv, sl, 3, hex(0x1E2A3A), 60);
    draw::polygon(cv, ss, 3, hex(0x1E2A3A), 60);

    outlined(cv, shade, 3, 0xD3DBE6, 0x6E7A8C, 0.7f * s);  // shadow side
    outlined(cv, lit, 3, 0xFFFFFF, 0x6E7A8C, 0.7f * s);    // lit side
    const Pt a = P(9, 0, 0, 0), b = P(-3.4f, 0, 0, 0);
    draw::capsule(cv, a.x, a.y, b.x, b.y, 0.3f * s, hex(0x9AA6B6), 220);  // fold line
}

// ---- golden acorn --------------------------------------------------------------------------------------------

void golden_acorn(const Canvas& cv, float x, float y, float s, std::uint32_t t) noexcept {
    const Stage p{cv, x, y + 0.5f * s, s, 1.0f};

    // Glow halo, low alpha, breathing slowly.
    const float glow = 0.5f + 0.5f * sin_ph(t, 2400);
    draw::circle(cv, x, y, 22 * s, hex(0xFFE27A), 14);
    draw::circle(cv, x, y, 17 * s, hex(0xFFE27A), 26 + static_cast<int>(18 * glow));

    // Acorn: gold body, darker cap with a little stem, as on the squirrel's acorn.
    p.oval(0, -1.5f, 5.6f, 6.4f, 0xFFD54A, 0xB8861E);
    p.oval(-2.0f, -0.6f, 1.6f, 2.8f, 0xFFF0A8, 0xFFF0A8, 0, 200);
    p.glint(-2.4f, 1.6f, 0.7f, 220);
    p.oval(0, 3.2f, 6.6f, 3.3f, 0xC9962A, 0x7E5A12);
    p.disc(-3.0f, 3.2f, 0.5f, 0x8A6212, 0x8A6212, 0);
    p.disc(0.0f, 4.0f, 0.5f, 0x8A6212, 0x8A6212, 0);
    p.disc(3.0f, 3.1f, 0.5f, 0x8A6212, 0x8A6212, 0);
    p.stroke(0.3f, 6.8f, 1.4f, 8.6f, 0.9f, 0xC9962A, 0x7E5A12, 0.6f);

    // Four twinkling sparkles around it, each on its own timing.
    for (std::uint32_t i = 0; i < 4; ++i) {
        const float ang = 0.7f + static_cast<float>(i) * 1.57f + 0.4f * sin_ph(t, 6000);
        const float tw = 0.5f + 0.5f * sin_ph(t + i * 230u, 1000);
        const float r = (1.6f + 1.8f * tw) * s;
        const int alpha = 110 + static_cast<int>(146 * tw);
        draw::sparkle(cv, x + 13.5f * s * std::cos(ang), y + 12.0f * s * std::sin(ang), r,
                      hex(i % 2 != 0 ? 0xFFF2A0 : 0xFFFFFF), alpha);
    }
}

}  // namespace screen::props
