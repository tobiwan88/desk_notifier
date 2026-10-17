// screen: shop items for the home, see props.hpp. Pure C++.
#include "screen/core/props.hpp"

#include <algorithm>
#include <cmath>

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

// Design units: u to the right (mirrored by f), v up from the row `by`. 1 unit = 1 px at s = 1.
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

// ---- fairy lights --------------------------------------------------------------------------------------------

void fairy_lights(const Canvas& cv, float x0, float x1, float y, std::uint32_t t) noexcept {
    constexpr int kN = 8;
    constexpr float kSag = 7.0f;
    static const std::uint32_t kBulb[4] = {0xFFD27A, 0xFF9A3C, 0xFF7FB0, 0x9AD4FF};
    auto wire_y = [&](float x) noexcept {
        const float u = (x - x0) / (x1 - x0);
        return y + kSag * 4.0f * u * (1.0f - u);
    };
    // The wire: short segments along the sag, dark with a lighter core.
    constexpr int kSeg = 24;
    for (int i = 0; i < kSeg; ++i) {
        const float xa = x0 + (x1 - x0) * static_cast<float>(i) / kSeg;
        const float xb = x0 + (x1 - x0) * static_cast<float>(i + 1) / kSeg;
        draw::capsule(cv, xa, wire_y(xa), xb, wire_y(xb), 0.9f, hex(0x2E2A36));
    }
    for (int i = 0; i < kN; ++i) {
        const float u = (static_cast<float>(i) + 0.5f) / kN;
        const float bx = x0 + (x1 - x0) * u, by = wire_y(bx) + 2.2f;
        // Twinkle: each bulb on its own phase; the glow swells and fades.
        const float tw = 0.5f + 0.5f * sin_ph(t + static_cast<std::uint32_t>(i) * 397u, 1700);
        const std::uint32_t c = kBulb[i % 4];
        draw::circle(cv, bx, by, 5.5f + 2.0f * tw, hex(c), 26 + static_cast<int>(40 * tw));  // soft halo
        draw::circle(cv, bx, by, 2.3f, hex(0x5A4A3A));                                    // socket outline
        draw::circle(cv, bx, by, 1.9f, hex(draw::lerp_rgb(c, 0xFFFFFF, static_cast<int>(60 * tw))));
        draw::circle(cv, bx - 0.6f, by - 0.7f, 0.6f, hex(0xFFFFFF), 200);                  // highlight
    }
}

// ---- curtains ------------------------------------------------------------------------------------------------

void curtains(const Canvas& cv, const Win& w, std::uint32_t t) noexcept {
    constexpr std::uint32_t kCloth = 0xEE8FA3, kFold = 0xC4637D, kEdge = 0x8C3F58, kRod = 0x7A5A3A;
    const float top = w.y0 - 7.0f, bot = w.y1 + 8.0f, mid = (w.y0 + w.y1) * 0.5f;
    // A gentle sway of the lower edge, the two curtains moving a little out of step.
    const float sway = 1.2f * std::sin(2 * kPi * ph(t, 4200));
    const float sway2 = 1.2f * std::sin(2 * kPi * ph(t + 900, 4200) + 0.8f);
    // Rod with finials, over both curtains.
    draw::capsule(cv, w.x0 - 14.0f, top - 1.0f, w.x1 + 14.0f, top - 1.0f, 1.3f, hex(kEdge));
    draw::capsule(cv, w.x0 - 14.0f, top - 1.0f, w.x1 + 14.0f, top - 1.0f, 0.8f, hex(kRod));
    draw::circle(cv, w.x0 - 15.0f, top - 1.0f, 1.6f, hex(kRod));
    draw::circle(cv, w.x1 + 15.0f, top - 1.0f, 1.6f, hex(kRod));
    draw::circle(cv, w.x0 - 15.4f, top - 1.6f, 0.5f, hex(0xFFFFFF), 180);

    for (const int side : {-1, 1}) {
        // Slim curtain: outer edge at `ox`, inner edge at `ix` (both just outside the glass).
        const float ox = side < 0 ? w.x0 - 11.0f : w.x1 + 11.0f;
        const float ix = side < 0 ? w.x0 - 1.0f : w.x1 + 1.0f;
        const float sw = side < 0 ? sway : sway2;
        const float tie_x = side < 0 ? w.x0 - 6.0f : w.x1 + 6.0f;  // where the band holds it
        // Curtain body (a quad, its lower edge swaying).
        const Pt body[4] = {{ox, top}, {ix, top}, {ix + sw * side, bot}, {ox + sw * side, bot}};
        outlined(cv, body, 4, kCloth, kEdge, 0.8f);
        // Vertical folds, darker stripes that fan out toward the hem.
        for (int k = 1; k <= 2; ++k) {
            const float f = static_cast<float>(k) / 3.0f;
            const float fx0 = ox + (ix - ox) * f, fx1 = ox + (ix - ox) * f + sw * side * 0.6f;
            draw::capsule(cv, fx0, top + 3.0f, fx1, bot - 2.0f, 0.7f, hex(kFold), 170);
        }
        // Light catching the outer fold.
        draw::capsule(cv, ox + 1.5f, top + 3.0f, ox + 1.5f + sw * side * 0.4f, bot - 3.0f, 0.5f, hex(0xFFFFFF), 110);
        // The tie-back: a band around the middle, with the curtain gathered inward above and below it.
        const float gather = side < 0 ? 1.8f : -1.8f;
        const Pt gath[4] = {{ox, mid - 3.0f}, {ix, mid - 3.0f}, {ix + gather, mid + 3.0f}, {ox + gather, mid + 3.0f}};
        draw::polygon(cv, gath, 4, hex(kFold), 120);
        draw::capsule(cv, tie_x - 2.4f, mid - 1.5f, tie_x + 2.4f, mid + 1.5f, 1.4f, hex(0xF2D27A));
        draw::capsule(cv, tie_x - 2.4f, mid - 1.5f, tie_x + 2.4f, mid + 1.5f, 0.5f, hex(0xFFFFFF), 120);
    }
}

void photo_frame(const Canvas& cv, float x, float y, float s) noexcept {
    // Units: 1 = 1 px at s = 1, centred on (x, y); the frame is 22 x 18 px.
    auto P = [&](float u, float v) noexcept { return Pt{x + u * s, y + v * s}; };
    const Pt outer[4] = {P(-11, -9), P(11, -9), P(11, 9), P(-11, 9)};
    outlined(cv, outer, 4, 0x9A6A3C, 0x5E3D1E, 0.9f * s);
    // Wood grain: a lighter bevel along the top and left.
    draw::capsule(cv, x + -9.5f * s, y - 7.5f * s, x + 9.5f * s, y - 7.5f * s, 0.5f * s, hex(0xB98852), 200);
    // The photo: pale sky with a soft white border.
    const Pt photo[4] = {P(-8.5f, -6.5f), P(8.5f, -6.5f), P(8.5f, 6.5f), P(-8.5f, 6.5f)};
    draw::polygon(cv, photo, 4, hex(0xDDEEFB));
    // Two squirrel heads side by side: ears, a cream face, eyes, a nose.
    for (const float sx : {-4.1f, 4.1f}) {
        const float hx = sx, hy = 1.6f;
        draw::circle(cv, x + (hx - 0.9f) * s, y + (hy - 2.6f) * s, 1.2f * s, hex(0x8A5A2A));  // ears
        draw::circle(cv, x + (hx + 0.9f) * s, y + (hy - 2.6f) * s, 1.2f * s, hex(0x8A5A2A));
        draw::circle(cv, x + hx * s, y + hy * s, 2.9f * s, hex(0xA0683A));
        draw::circle(cv, x + hx * s, y + (hy + 1.0f) * s, 1.8f * s, hex(0xF2DDB8));  // muzzle
        draw::circle(cv, x + (hx - 1.0f) * s, y + (hy - 0.4f) * s, 0.45f * s, hex(0x1E1410));
        draw::circle(cv, x + (hx + 1.0f) * s, y + (hy - 0.4f) * s, 0.45f * s, hex(0x1E1410));
        draw::circle(cv, x + hx * s, y + (hy + 0.7f) * s, 0.4f * s, hex(0x1E1410));
    }
    // A small red heart above the squirrels, two lobes and a point.
    const float hx = 0.0f, hy = -3.4f;
    draw::circle(cv, x + (hx - 0.7f) * s, y + (hy - 0.3f) * s, 1.0f * s, hex(0xE23B4E));
    draw::circle(cv, x + (hx + 0.7f) * s, y + (hy - 0.3f) * s, 1.0f * s, hex(0xE23B4E));
    const Pt tip[3] = {P(-1.6f, hy - 0.1f), P(1.6f, hy - 0.1f), P(0.0f, hy + 1.9f)};
    draw::polygon(cv, tip, 3, hex(0xE23B4E));
    draw::circle(cv, x + (hx - 0.9f) * s, y + (hy - 0.6f) * s, 0.35f * s, hex(0xFFFFFF), 200);
}

void poster(const Canvas& cv, float x, float y, float s) noexcept {
    // A tilted sheet: local units rotated by a small angle about (x, y). 20 x 26 px.
    constexpr float kTilt = -0.07f;
    const float c = std::cos(kTilt), sn = std::sin(kTilt);
    auto P = [&](float u, float v) noexcept {
        const float dx = u * s, dy = v * s;
        return Pt{x + dx * c - dy * sn, y + dx * sn + dy * c};
    };
    const Pt paper[4] = {P(-10, -13), P(10, -13), P(10, 13), P(-10, 13)};
    outlined(cv, paper, 4, 0xFFFFFF, 0x9AA8B8, 0.7f * s);
    // Pale sky.
    const Pt sky[4] = {P(-8, -11), P(8, -11), P(8, 11), P(-8, 11)};
    draw::polygon(cv, sky, 4, hex(0xDDF0FF));
    // Sun, upper right.
    draw::circle(cv, P(4.6f, -6.0f).x, P(4.6f, -6.0f).y, 2.6f * s, hex(0xFFD23F));
    // Two mountains with snowy peaks.
    const Pt back[3] = {P(-1, 11), P(4.5f, -0.5f), P(8, 11)};
    draw::polygon(cv, back, 3, hex(0x7D93BE));
    const Pt back_snow[3] = {P(4.5f, -0.5f), P(2.6f, 2.6f), P(6.4f, 2.6f)};
    draw::polygon(cv, back_snow, 3, hex(0xFFFFFF));
    const Pt front[3] = {P(-8, 11), P(-2.2f, -2.4f), P(4.5f, 11)};
    draw::polygon(cv, front, 3, hex(0x5E74A6));
    const Pt front_snow[3] = {P(-2.2f, -2.4f), P(-4.4f, 1.6f), P(0.0f, 1.6f)};
    draw::polygon(cv, front_snow, 3, hex(0xFFFFFF));
    // A red pin at the top edge, with a glint.
    const Pt pin = P(0, -13.5f);
    draw::circle(cv, pin.x, pin.y, 2.0f * s, hex(0x9E2A2A));
    draw::circle(cv, pin.x, pin.y, 1.5f * s, hex(0xE23B4E));
    draw::circle(cv, pin.x - 0.5f * s, pin.y - 0.6f * s, 0.5f * s, hex(0xFFFFFF), 210);
}

void aquarium(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept {
    const Stage p{cv, x, base_y, s, 1.0f};
    constexpr float kCy = 11.0f, kR = 9.2f;  // bowl centre (units up from the foot) and water radius
    // Foot, then the glass: dark rim, tinted glass, water.
    p.oval(0, 0.9f, 4.8f, 0.9f, 0x6A5A8A, 0x3E3256, 0.5f);
    draw::circle(cv, p.X(0), p.Y(kCy), 10.3f * s, hex(0x6E8FA8), 200);
    draw::circle(cv, p.X(0), p.Y(kCy), 10.0f * s, hex(0xCFEFFF), 90);
    draw::circle(cv, p.X(0), p.Y(kCy), kR * s, hex(0x4FA8DE), 150);
    // Sand: the lower chord of the water circle.
    Pt sand[5];
    for (int i = 0; i < 5; ++i) {
        const float a = kPi / 2 - 0.8f + 1.6f * static_cast<float>(i) / 4.0f;
        sand[i] = {kR * std::cos(a), kCy - kR * std::sin(a)};
    }
    p.poly(sand, 5, 0xE8C77A, 0xB08A3E, 0.4f);
    // A little green plant rooted in the sand: two leaves on a stem.
    p.stroke(-3.6f, 3.2f, -3.6f, 6.2f, 0.6f, 0x3E9A4E, 0x23602E, 0.4f);
    draw::ellipse(cv, p.X(-5.2f), p.Y(6.4f), 2.6f * s, 1.2f * s, hex(0x23602E), -0.5f);
    draw::ellipse(cv, p.X(-5.2f), p.Y(6.4f), 2.2f * s, 0.9f * s, hex(0x4CB35E), -0.5f);
    draw::ellipse(cv, p.X(-2.2f), p.Y(7.4f), 2.6f * s, 1.2f * s, hex(0x23602E), 0.5f);
    draw::ellipse(cv, p.X(-2.2f), p.Y(7.4f), 2.2f * s, 0.9f * s, hex(0x5FC46C), 0.5f);
    // One orange fish swimming left and right; it turns at each end.
    const float ang = 2 * kPi * ph(t, 5200);
    const float fu = 5.2f * std::sin(ang), fv = 10.0f + 1.2f * std::sin(2 * ang);
    const float dir = std::cos(ang) >= 0 ? 1.0f : -1.0f;
    const Pt tail[3] = {{fu - dir * 2.0f, fv}, {fu - dir * 4.6f, fv + 1.9f}, {fu - dir * 4.6f, fv - 1.9f}};
    p.poly(tail, 3, 0xFF8A2A, 0xB8520F, 0.4f);
    p.oval(fu, fv, 2.7f, 1.7f, 0xFF8A2A, 0xB8520F, 0.5f);
    p.disc(fu + dir * 1.3f, fv + 0.3f, 0.5f, 0x1E1410, 0x1E1410, 0);
    p.glint(fu + dir * 0.6f, fv + 0.9f, 0.35f, 170);
    // Bubbles rising from the plant, wobbling, fading out near the top.
    for (int i = 0; i < 3; ++i) {
        const std::uint32_t off = static_cast<std::uint32_t>(i) * 1100u;
        const float bp = ph(t + off, 3200);
        const float bu = 2.6f - 1.4f * static_cast<float>(i) + 0.6f * std::sin(2 * kPi * bp * 2 + static_cast<float>(i));
        const float bv = 3.5f + 15.5f * bp;
        const int a = static_cast<int>(230.0f * (1.0f - bp * bp));
        const float r = (1.0f + 0.15f * static_cast<float>(i)) * s;
        draw::circle(cv, p.X(bu), p.Y(bv), r, hex(0xDDF6FF), a);
        draw::circle(cv, p.X(bu) - 0.3f * r, p.Y(bv) - 0.35f * r, 0.35f * s, hex(0xFFFFFF), a);
    }
    // Glass highlight along the upper left.
    draw::arc(cv, p.X(0), p.Y(kCy), 7.8f * s, 3.5f, 4.1f, 0.45f * s, hex(0xFFFFFF), 150, 6);
}

void lava_lamp(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept {
    const Stage p{cv, x, base_y, s, 1.0f};
    // A soft pink glow behind the lamp.
    draw::circle(cv, p.X(0), p.Y(13), 17.0f * s, hex(0xFF6EC7), 16);
    draw::circle(cv, p.X(0), p.Y(13), 11.0f * s, hex(0xFF6EC7), 24);
    // Metal foot and neck.
    p.oval(0, 1.2f, 6.0f, 1.3f, 0xB8C0CC, 0x5E6672, 0.5f);
    const Pt neck[4] = {{-2.6f, 2.4f}, {2.6f, 2.4f}, {2.2f, 4.2f}, {-2.2f, 4.2f}};
    p.poly(neck, 4, 0xB8C0CC, 0x5E6672, 0.5f);
    // Tapered glass body, then the liquid inside it.
    const Pt glass[6] = {{-4.0f, 4.0f}, {4.0f, 4.0f}, {3.2f, 14.0f}, {2.6f, 20.0f}, {-2.6f, 20.0f}, {-3.2f, 14.0f}};
    p.poly(glass, 6, 0xB9D9F0, 0x5E7F9C, 0.7f);
    const Pt liquid[6] = {{-3.2f, 4.6f}, {3.2f, 4.6f}, {2.6f, 13.6f}, {2.1f, 19.0f}, {-2.1f, 19.0f}, {-2.6f, 13.6f}};
    p.poly(liquid, 6, 0x9B3FC2, 0x6A2A8E, 0.0f);
    // Two or three orange blobs rising and sinking, each on its own cycle.
    for (int i = 0; i < 3; ++i) {
        const float bp = ph(t + static_cast<std::uint32_t>(i) * 2100u, 6400u + static_cast<std::uint32_t>(i) * 900u);
        const float bv = 6.5f + 10.5f * (0.5f - 0.5f * std::cos(2 * kPi * bp));
        const float bu = -1.2f + 1.2f * static_cast<float>(i) + 0.5f * std::sin(2 * kPi * bp * 2 + static_cast<float>(i));
        p.disc(bu, bv, 1.9f - 0.2f * static_cast<float>(i), 0xFF8A3D, 0xC85A1A, 0.4f);
        p.glint(bu - 0.5f, bv + 0.5f, 0.35f, 150);
    }
    // Glass highlight, a metal cap and a knob on top.
    draw::capsule(cv, p.X(-2.2f), p.Y(6.0f), p.X(-2.2f), p.Y(17.0f), 0.5f * s, hex(0xFFFFFF), 150);
    const Pt cap[4] = {{-3.2f, 20.0f}, {3.2f, 20.0f}, {2.4f, 22.2f}, {-2.4f, 22.2f}};
    p.poly(cap, 4, 0xB8C0CC, 0x5E6672, 0.5f);
    p.disc(0, 22.8f, 1.0f, 0xB8C0CC, 0x5E6672, 0.5f);
}

void record_player(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept {
    const Stage p{cv, x, base_y, s, 1.0f};
    // Wooden box with a cream-coloured deck, a little knob on the front.
    const Pt box[4] = {{-12.0f, 0.0f}, {12.0f, 0.0f}, {12.0f, 9.0f}, {-12.0f, 9.0f}};
    p.poly(box, 4, 0x9A6A3C, 0x5E3D1E, 0.8f);
    draw::capsule(cv, p.X(-11.0f), p.Y(8.6f), p.X(11.0f), p.Y(8.6f), 0.5f * s, hex(0xC9955A), 220);
    p.disc(9.6f, 4.5f, 0.9f, 0xF2D27A, 0x8A6A22, 0.4f);
    // Dark record, seen at a slant, with grooves and a red label.
    p.oval(-3.0f, 10.2f, 6.0f, 2.6f, 0x222233, 0x0E0E18, 0.5f);
    draw::ellipse(cv, p.X(-3.0f), p.Y(10.2f), 4.4f * s, 1.9f * s, hex(0x2E2E42), 0, 120);
    draw::ellipse(cv, p.X(-3.0f), p.Y(10.2f), 3.2f * s, 1.4f * s, hex(0x2E2E42), 0, 120);
    p.disc(-3.0f, 10.2f, 1.6f, 0xE0544A, 0x9E2A2A, 0.2f);
    p.disc(-3.0f, 10.2f, 0.4f, 0xF4F4F4, 0xF4F4F4, 0);
    // A sheen that rotates with the record: two opposite highlight streaks.
    const float a = 2 * kPi * ph(t, 1800);
    for (const float d : {0.0f, kPi}) {
        const float q0 = a + d, q1 = a + d + 0.5f;
        draw::capsule(cv, p.X(-3.0f + 4.6f * std::cos(q0)), p.Y(10.2f + 2.2f * std::sin(q0)),
                      p.X(-3.0f + 4.6f * std::cos(q1)), p.Y(10.2f + 2.2f * std::sin(q1)), 0.4f * s, hex(0xFFFFFF), 120);
    }
    // Tonearm: a pivot on the right, a bent arm, a small head over the record edge.
    p.disc(8.6f, 12.4f, 1.1f, 0xC8CCD4, 0x6E7682, 0.4f);
    p.stroke(8.6f, 12.4f, 4.4f, 11.2f, 0.35f, 0xC8CCD4, 0x6E7682, 0.3f);
    p.stroke(4.4f, 11.2f, 2.2f, 10.4f, 0.5f, 0x8A9098, 0x4A4F58, 0.3f);
    // Music notes floating up and fading, each on its own cycle.
    for (int i = 0; i < 3; ++i) {
        const std::uint32_t off = static_cast<std::uint32_t>(i) * 1400u;
        const float np = ph(t + off, 4200);
        const float nu = -3.0f + 2.5f * static_cast<float>(i) + 1.6f * std::sin(2 * kPi * np + static_cast<float>(i));
        const float nv = 13.0f + 14.0f * np;
        const int a2 = static_cast<int>(230.0f * (1.0f - np));
        if (a2 <= 0) continue;
        draw::circle(cv, p.X(nu), p.Y(nv), 1.2f * s, hex(0x5B3A8A), a2);
        draw::capsule(cv, p.X(nu) + 1.0f * s, p.Y(nv), p.X(nu) + 1.0f * s, p.Y(nv + 3.6f), 0.35f * s, hex(0x5B3A8A), a2);
        draw::capsule(cv, p.X(nu) + 1.0f * s, p.Y(nv + 3.6f), p.X(nu) + 2.4f * s, p.Y(nv + 2.6f), 0.5f * s, hex(0x5B3A8A), a2);
    }
}

void cactus(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept {
    (void)t;  // still: a cactus does not move
    const Stage p{cv, x, base_y, s, 1.0f};
    // Terracotta pot with a rim.
    const Pt pot[4] = {{-3.8f, 0.0f}, {3.8f, 0.0f}, {5.0f, 6.0f}, {-5.0f, 6.0f}};
    p.poly(pot, 4, 0xC8673A, 0x8A3E1E, 0.7f);
    const Pt rim[4] = {{-5.4f, 5.0f}, {5.4f, 5.0f}, {5.4f, 7.2f}, {-5.4f, 7.2f}};
    p.poly(rim, 4, 0xD9774A, 0x8A3E1E, 0.6f);
    // Green body and two arms bent up at the elbows.
    p.stroke(0.0f, 6.5f, 0.0f, 14.5f, 2.6f, 0x3E9A4E, 0x23602E, 0.8f);
    p.stroke(-0.4f, 9.0f, -4.2f, 9.0f, 1.5f, 0x3E9A4E, 0x23602E, 0.7f);
    p.stroke(-4.2f, 9.0f, -4.2f, 11.8f, 1.5f, 0x3E9A4E, 0x23602E, 0.7f);
    p.stroke(0.4f, 7.6f, 4.0f, 7.6f, 1.5f, 0x3E9A4E, 0x23602E, 0.7f);
    p.stroke(4.0f, 7.6f, 4.0f, 10.2f, 1.5f, 0x3E9A4E, 0x23602E, 0.7f);
    // Tiny cream spines.
    for (const Pt sp : {Pt{-1.2f, 8.2f}, Pt{1.2f, 11.0f}, Pt{-1.2f, 12.6f}, Pt{1.2f, 6.8f}, Pt{-4.2f, 13.6f}, Pt{4.0f, 9.4f}}) {
        draw::capsule(cv, p.X(sp.x - 0.5f), p.Y(sp.y), p.X(sp.x + 0.5f), p.Y(sp.y + 0.6f), 0.3f * s, hex(0xF2F0C8), 230);
    }
    // Pink flower on top: five petals around a yellow centre.
    const float fu = 0.0f, fv = 15.6f;
    for (int k = 0; k < 5; ++k) {
        const float a = 2 * kPi * static_cast<float>(k) / 5.0f - kPi / 2;
        p.disc(fu + 1.5f * std::cos(a), fv + 1.5f * std::sin(a), 1.2f, 0xFF7FB0, 0xC2407E, 0.4f);
    }
    p.disc(fu, fv, 1.0f, 0xFFD23F, 0xB8861A, 0.3f);
}

void hamster_wheel(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept {
    const Stage p{cv, x, base_y, s, 1.0f};
    constexpr float kHv = 13.0f, kR = 10.0f;  // hub height and wheel radius, units
    // Stand: two legs from the hub to a foot bar, behind the wheel.
    p.stroke(0.0f, kHv, -6.5f, 0.9f, 0.7f, 0x8A9098, 0x4A4F58, 0.4f);
    p.stroke(0.0f, kHv, 6.5f, 0.9f, 0.7f, 0x8A9098, 0x4A4F58, 0.4f);
    p.stroke(-7.5f, 0.8f, 7.5f, 0.8f, 0.8f, 0x8A9098, 0x4A4F58, 0.4f);
    // Wire wheel: a dark ring with a light core, spokes turning once per 900 ms.
    draw::arc(cv, p.X(0), p.Y(kHv), kR * s, 0, 2 * kPi, 1.0f * s, hex(0x4A4F58), 255, 28);
    draw::arc(cv, p.X(0), p.Y(kHv), kR * s, 0, 2 * kPi, 0.45f * s, hex(0xC8CED8), 255, 28);
    const float spin = 2 * kPi * ph(t, 900);
    for (int k = 0; k < 6; ++k) {
        const float a = spin + static_cast<float>(k) * kPi / 3.0f;
        p.stroke(0.0f, kHv, kR * 0.92f * std::cos(a), kHv + kR * 0.92f * std::sin(a), 0.3f, 0x9AA2AE, 0x9AA2AE, 0);
    }
    p.disc(0.0f, kHv, 1.2f, 0x6E7682, 0x3A3F48, 0.3f);
    // The hamster, running: body bob and legs stepping on the rim (gait 450 ms).
    const float g = std::sin(2 * kPi * ph(t, 450));
    const float bob = 0.3f * std::fabs(g);
    const float bv = 9.2f + bob;
    p.stroke(-3.2f, bv + 0.4f, -4.8f, bv + 1.4f, 0.5f, 0xD1904A, 0x8A5226, 0.3f);  // tail
    const float fx1 = 1.6f + 1.8f * g, fy1 = 4.6f + 0.6f * std::max(g, 0.0f);
    const float fx2 = -1.4f - 1.8f * g, fy2 = 4.6f + 0.6f * std::max(-g, 0.0f);
    p.stroke(1.0f, 6.8f + bob, fx1, fy1, 0.7f, 0xB87A3E, 0x6E4826, 0.4f);
    p.stroke(-1.0f, 6.8f + bob, fx2, fy2, 0.7f, 0xB87A3E, 0x6E4826, 0.4f);
    p.oval(0.0f, bv, 3.6f, 2.9f, 0xD1904A, 0x8A5226, 0.6f);
    p.oval(0.6f, bv - 0.8f, 2.2f, 1.7f, 0xF2D2A0, 0xF2D2A0, 0);
    p.disc(1.4f, bv + 2.2f, 0.9f, 0xD1904A, 0x8A5226, 0.4f);  // ears
    p.disc(3.4f, bv + 2.0f, 0.9f, 0xD1904A, 0x8A5226, 0.4f);
    p.disc(2.6f, bv + 1.8f, 2.3f, 0xD1904A, 0x8A5226, 0.6f);  // head
    p.disc(3.4f, bv + 0.8f, 1.1f, 0xF2D2A0, 0xF2D2A0, 0);     // cheek
    p.disc(3.5f, bv + 2.6f, 0.4f, 0x1E1410, 0x1E1410, 0);     // eye
    p.disc(4.8f, bv + 1.6f, 0.35f, 0xE8788A, 0xE8788A, 0);    // nose
}

void hedgehog_sleep(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept {
    const Stage p{cv, x, base_y, s, 1.0f};
    const float br = 1.0f + 0.04f * sin_ph(t, 3000);  // breathing: the ball swells a little
    constexpr float kCv = 7.0f;                        // ball centre height
    p.oval(0.0f, 0.6f, 9.0f, 1.2f, 0x8A6A44, 0x8A6A44, 0, 90);  // shadow
    // Spikes all round the ball, outward, alternating two browns.
    for (int i = 0; i < 12; ++i) {
        const float a = 2 * kPi * static_cast<float>(i) / 12.0f + 0.1f;
        const float rb = 4.9f * br, rt = 7.8f * br;
        const Pt tri[3] = {{rb * std::cos(a - 0.24f), kCv + rb * std::sin(a - 0.24f)},
                           {rt * std::cos(a), kCv + rt * std::sin(a)},
                           {rb * std::cos(a + 0.24f), kCv + rb * std::sin(a + 0.24f)}};
        p.poly(tri, 3, (i % 2) != 0 ? 0x9A6A3C : 0x6E4826, 0x3A2412, 0.5f);
    }
    p.disc(0.0f, kCv, 5.2f * br, 0x6E4826, 0x3A2412, 0.6f);
    // Face tucked in at the lower front: cream, a closed eye, a nose.
    p.oval(2.4f, 3.0f, 3.0f, 1.9f, 0xF6E7C8, 0x8A6A44, 0.4f);
    p.stroke(1.2f, 3.6f, 2.4f, 3.6f, 0.3f, 0x1E1410, 0x1E1410, 0);
    p.disc(4.6f, 3.4f, 0.7f, 0x1E1410, 0x1E1410, 0);
    // Zzz: three little z shapes drifting up and fading, each on its own cycle.
    for (int i = 0; i < 3; ++i) {
        const float zp = ph(t + static_cast<std::uint32_t>(i) * 1000u, 3000);
        const float zu = 8.0f + 2.6f * static_cast<float>(i), zv = 10.0f + 9.0f * zp;
        const float sz = (0.7f + 0.5f * zp) * 1.6f;
        const int a = static_cast<int>(230.0f * (1.0f - zp));
        if (a <= 0) continue;
        draw::capsule(cv, p.X(zu - sz), p.Y(zv + sz), p.X(zu + sz), p.Y(zv + sz), 0.4f * s, hex(0x5A6FA8), a);
        draw::capsule(cv, p.X(zu + sz), p.Y(zv + sz), p.X(zu - sz), p.Y(zv - sz), 0.4f * s, hex(0x5A6FA8), a);
        draw::capsule(cv, p.X(zu - sz), p.Y(zv - sz), p.X(zu + sz), p.Y(zv - sz), 0.4f * s, hex(0x5A6FA8), a);
    }
}

void nest(const Canvas& cv, float x, float base_y, float s, std::uint32_t t) noexcept {
    const Stage p{cv, x, base_y, s, 1.0f};
    // Twig bowl: a brown rim, a darker cavity, twigs across the front.
    p.oval(0.0f, 4.0f, 11.0f, 5.0f, 0x9A6A3C, 0x5A3618, 0.8f);
    p.oval(0.0f, 6.4f, 8.4f, 2.8f, 0x5A3618, 0x3A2412, 0);
    p.stroke(-9.0f, 2.2f, -6.0f, 4.6f, 0.45f, 0xB27E44, 0x6E4826, 0.2f);
    p.stroke(-3.0f, 1.6f, -1.0f, 4.2f, 0.45f, 0xB27E44, 0x6E4826, 0.2f);
    p.stroke(3.0f, 1.8f, 5.0f, 4.4f, 0.45f, 0xB27E44, 0x6E4826, 0.2f);
    p.stroke(7.5f, 2.2f, 10.0f, 4.0f, 0.45f, 0xB27E44, 0x6E4826, 0.2f);
    // Two fluffy chicks sitting in the bowl, each opening its beak in short pulses on its own timing.
    for (int i = 0; i < 2; ++i) {
        const float cu = i == 0 ? -3.6f : 2.6f, hv = 9.6f;
        const float open = std::pow(std::max(0.0f, std::sin(2 * kPi * ph(t + static_cast<std::uint32_t>(i) * 1300u, 2800))), 4.0f);
        p.oval(cu, 6.8f, 3.4f, 2.8f, 0xFFD84A, 0xC9A21A, 0.5f);  // body
        p.disc(cu + 0.4f, hv, 2.6f, 0xFFE46A, 0xC9A21A, 0.5f);   // head
        p.disc(cu - 0.6f, hv + 1.0f, 0.4f, 0xFFFFFF, 0xFFFFFF, 0);
        p.disc(cu + 1.1f, hv + 0.6f, 0.45f, 0x1E1410, 0x1E1410, 0);  // eye
        p.glint(cu + 0.8f, hv + 0.9f, 0.25f, 200);
        // Beak: upper part fixed, lower part drops as it opens.
        const Pt upper[3] = {{cu + 2.2f, hv + 0.3f}, {cu + 4.0f, hv - 0.1f}, {cu + 2.2f, hv - 0.6f}};
        p.poly(upper, 3, 0xFF8A2A, 0xB8520F, 0.3f);
        const float lo = 1.6f * open;
        const Pt lower[3] = {{cu + 2.2f, hv - 0.6f}, {cu + 3.6f, hv - 0.8f - lo}, {cu + 2.2f, hv - 1.2f - lo * 0.5f}};
        p.poly(lower, 3, 0xFF8A2A, 0xB8520F, 0.3f);
    }
    // A little speckled egg in front, on the right.
    p.oval(9.0f, 4.6f, 2.2f, 2.7f, 0xF6EEDD, 0xB8A88A, 0.4f);
    p.disc(8.4f, 5.4f, 0.4f, 0xA0785A, 0xA0785A, 0);
    p.disc(9.6f, 3.8f, 0.3f, 0xA0785A, 0xA0785A, 0);
}

}  // namespace screen::props
