#include "eyes/core/eyes.hpp"

namespace eyes {
namespace {

using gfx::Rect;
using gfx::Rgb565be;

constexpr int kC = gfx::kScreen / 2;  // eye centre (120)

constexpr int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
constexpr int absi(int v) { return v < 0 ? -v : v; }

// half-width (1/16 px) of a circle of radius r at row offset dy (all in px, dy measured at row centre in half-px)
// dyq: row-centre offset in half pixels; rq: radius in half pixels.
int circle_hw16(int dyq, int rq) {
    const int v = rq * rq - dyq * dyq;
    return v <= 0 ? 0 : 8 * gfx::isqrt(v);
}

// Paint [x0_16, x1_16) (1/16 px units) with anti-aliased end pixels, clipped to [cl, cr).
void paint(Rgb565be* row, int x0_16, int x1_16, int cl, int cr, Rgb565be c) {
    if (x1_16 <= x0_16) return;
    int p0 = x0_16 >> 4, p1 = (x1_16 - 1) >> 4;  // first/last touched pixel
    if (p1 < cl || p0 >= cr) return;
    auto cover = [&](int p) { return clampi(x1_16, p * 16, p * 16 + 16) - clampi(x0_16, p * 16, p * 16 + 16); };
    auto put = [&](int p) {
        if (p < cl || p >= cr) return;
        const int cv = cover(p);
        if (cv >= 16) row[p] = c;
        else if (cv > 0) row[p] = gfx::blend(row[p], c, static_cast<std::uint8_t>(cv * 255 / 16));
    };
    if (p0 == p1) { put(p0); return; }
    put(p0);
    const int a = p0 + 1 < cl ? cl : p0 + 1, b = p1 > cr ? cr : p1;
    for (int p = a; p < b; ++p) row[p] = c;
    put(p1);
}

struct Span { int a, b; };  // 1/16 px
Span circle_span(int cx, int cy, int r, int y) {
    const int dyq = 2 * y + 1 - 2 * cy;
    const int hw = circle_hw16(dyq, 2 * r);
    if (hw == 0) return {0, 0};
    return {cx * 16 - hw, cx * 16 + hw};
}
Span rrect_span(const EyeGeom& g, int y) {
    const int dyq = 2 * y + 1 - 2 * g.scy;
    const int halfh_q = g.sh;                   // half height in half-px = sh
    if (absi(dyq) >= halfh_q) return {0, 0};
    const int inner_q = halfh_q - 2 * g.sr;     // straight part
    int hw16;
    if (absi(dyq) <= inner_q) hw16 = g.sw * 8;
    else hw16 = (g.sw / 2 - g.sr) * 16 + circle_hw16(absi(dyq) - inner_q, 2 * g.sr);
    return {g.scx * 16 - hw16, g.scx * 16 + hw16};
}
Span pupil_span(const EyeGeom& g, int y) {
    if (!g.slit) return circle_span(g.icx, g.icy, g.pr, y);
    const int dyq = 2 * y + 1 - 2 * g.icy;
    const int b = 2 * g.pr;
    const int v = b * b - dyq * dyq;
    if (v <= 0) return {0, 0};
    const int hw = 8 * gfx::isqrt(v) * g.pslit / (2 * (g.pr > 0 ? g.pr : 1));
    return {g.icx * 16 - hw, g.icx * 16 + hw};
}

// lid edge (px, 1/1 y coordinate) at column x for the top lid; covers y < edge
int top_edge_at(const EyeGeom& g, int x) {
    const int half = (g.box_r - g.box_l) / 2 > 0 ? (g.box_r - g.box_l) / 2 : 1;
    const int u = clampi((x - (g.box_l + g.box_r) / 2) * 256 / half, -256, 256);   // q8
    return g.top_edge + g.curve * (65536 - u * u) / 65536 + g.tilt_px * (-u) / 256;
}
int bot_edge_at(const EyeGeom& g, int x) {
    const int half = (g.box_r - g.box_l) / 2 > 0 ? (g.box_r - g.box_l) / 2 : 1;
    const int u = clampi((x - (g.box_l + g.box_r) / 2) * 256 / half, -256, 256);
    return g.bot_edge - g.curve * (65536 - u * u) / 65536;
}

}  // namespace

Rgb565be scale_brightness(Rgb565be c, std::uint8_t pct) noexcept {
    if (pct >= 100) return c;
    const std::uint32_t v = static_cast<std::uint16_t>((c << 8) | (c >> 8));
    const std::uint32_t r = ((v >> 11) & 31) * pct / 100, g = ((v >> 5) & 63) * pct / 100, b = (v & 31) * pct / 100;
    const std::uint16_t o = static_cast<std::uint16_t>((r << 11) | (g << 5) | b);
    return static_cast<Rgb565be>((o << 8) | (o >> 8));
}

EyeGeom layout(const EyeStyle& st, const State& s, engine::Eye eye) noexcept {
    EyeGeom g{};
    const int pct = eye == engine::Eye::Left ? s.scale_l : s.scale_r;
    const int br = s.brightness;
    auto col = [&](Rgb565be c) { return scale_brightness(c, static_cast<std::uint8_t>(br)); };
    auto sc = [&](int v) { return static_cast<std::int16_t>(v * pct / 100); };

    const int lx = s.look_x * st.look.max_x / 256, ly = s.look_y * st.look.max_y / 256;
    g.bg = col(st.bg);
    g.sclera_on = st.sclera.on;
    g.sw = sc(st.sclera.w); g.sh = sc(st.sclera.h); g.sr = sc(st.sclera.radius);
    if (g.sr > g.sw / 2) g.sr = g.sw / 2;
    if (g.sr > g.sh / 2) g.sr = g.sh / 2;
    g.sclera_c = col(st.sclera.color);
    const bool moves_sclera = !st.iris.on;
    g.scx = static_cast<std::int16_t>(kC + (moves_sclera ? lx : 0));
    g.scy = static_cast<std::int16_t>(kC + (moves_sclera ? ly : 0));

    g.iris_on = st.iris.on;
    g.ir = sc(st.iris.r); g.iring = st.iris.ring_w;
    g.icx = static_cast<std::int16_t>(kC + lx); g.icy = static_cast<std::int16_t>(kC + ly);
    g.iris_c = col(st.iris.color); g.ring_c = col(st.iris.ring);

    g.pupil_on = st.pupil.on;
    g.pr = static_cast<std::int16_t>(sc(st.pupil.r) * s.pupil / 140);
    g.slit = st.pupil.shape == PupilShape::Slit;
    g.pslit = sc(st.pupil.slit_w);
    g.pupil_c = col(st.pupil.color);

    g.hl_on = st.highlight.on;
    g.hr = sc(st.highlight.r);
    g.hx = static_cast<std::int16_t>(g.icx + st.highlight.dx); g.hy = static_cast<std::int16_t>(g.icy + st.highlight.dy);
    g.hl_c = col(st.highlight.color);

    // lids
    g.lid_c = col(st.lid.color);
    g.box_l = static_cast<std::int16_t>(kC - g.sw / 2); g.box_r = static_cast<std::int16_t>(kC + g.sw / 2);
    g.box_t = static_cast<std::int16_t>(kC - g.sh / 2); g.box_b = static_cast<std::int16_t>(kC + g.sh / 2);
    const int H = g.sh;
    int top = eye == engine::Eye::Left ? s.lid_top : s.lid_top_r;
    const int blink = eye == engine::Eye::Left ? s.blink_l : s.blink_r;
    top = top + (255 - top) * blink / 256;
    g.top_closed = top >= 255;
    g.bot_closed = s.lid_bot >= 255;
    g.top_edge = static_cast<std::int16_t>(g.box_t + H * top / 255);
    g.bot_edge = static_cast<std::int16_t>(g.box_b - H * s.lid_bot / 255);
    g.curve = st.lid.curve;
    const int tilt = eye == engine::Eye::Left ? s.tilt : -s.tilt;  // + = outer corner up
    g.tilt_px = static_cast<std::int16_t>(tilt * (g.sw / 4) / 127);
    return g;
}

engine::StaticVec<gfx::Rect, 3> diff(const EyeGeom& a, const EyeGeom& b) noexcept {
    engine::StaticVec<Rect, 3> out;
    if (a == b) return out;
    auto box = [](int cx, int cy, int r) {
        return Rect{static_cast<std::int16_t>(cx - r - 2), static_cast<std::int16_t>(cy - r - 2),
                    static_cast<std::int16_t>(2 * r + 4), static_cast<std::int16_t>(2 * r + 4)};
    };
    auto sclera_box = [](const EyeGeom& g) {
        return Rect{static_cast<std::int16_t>(g.scx - g.sw / 2 - 2), static_cast<std::int16_t>(g.scy - g.sh / 2 - 2),
                    static_cast<std::int16_t>(g.sw + 4), static_cast<std::int16_t>(g.sh + 4)};
    };
    auto lid_rect = [](const EyeGeom& g, const EyeGeom& h) {  // union of lid bands
        const int bt = h.box_t < g.box_t ? h.box_t : g.box_t, bb = h.box_b > g.box_b ? h.box_b : g.box_b;
        const int ext = (g.curve > h.curve ? g.curve : h.curve) + absi(g.tilt_px) + absi(h.tilt_px) + 2;
        const int ye = [&] { int m = g.top_edge > h.top_edge ? g.top_edge : h.top_edge; return m + ext; }();
        const int yb = [&] { int m = g.bot_edge < h.bot_edge ? g.bot_edge : h.bot_edge; return m - ext; }();
        const int l = (g.box_l < h.box_l ? g.box_l : h.box_l) - 1, r = (g.box_r > h.box_r ? g.box_r : h.box_r) + 1;
        return std::pair{Rect{static_cast<std::int16_t>(l), static_cast<std::int16_t>(bt - 1), static_cast<std::int16_t>(r - l),
                              static_cast<std::int16_t>(ye - bt + 2)},
                         Rect{static_cast<std::int16_t>(l), static_cast<std::int16_t>(yb), static_cast<std::int16_t>(r - l),
                              static_cast<std::int16_t>(bb - yb + 2)}};
    };
    // structural change (colours, sclera, style) -> redraw the whole sclera area
    EyeGeom a2 = a, b2 = b;
    a2.icx = b2.icx; a2.icy = b2.icy; a2.hx = b2.hx; a2.hy = b2.hy; a2.pr = b2.pr; a2.hr = b2.hr;
    a2.top_edge = b2.top_edge; a2.bot_edge = b2.bot_edge; a2.tilt_px = b2.tilt_px; a2.curve = b2.curve;
    a2.top_closed = b2.top_closed; a2.bot_closed = b2.bot_closed;
    if (!(a2 == b2)) {
        (void)out.push_back(gfx::clip_screen(gfx::unite(sclera_box(a), sclera_box(b))));
        return out;
    }
    const int ra = a.ir + a.iring, rb = b.ir + b.iring;
    const Rect ib = gfx::unite(box(a.icx, a.icy, ra > a.sw / 2 ? a.sw / 2 : ra), box(b.icx, b.icy, rb > b.sw / 2 ? b.sw / 2 : rb));
    const bool iris_changed = a.icx != b.icx || a.icy != b.icy || a.pr != b.pr || a.hr != b.hr;
    const bool lid_changed = a.top_edge != b.top_edge || a.bot_edge != b.bot_edge || a.tilt_px != b.tilt_px ||
                             a.curve != b.curve || a.top_closed != b.top_closed || a.bot_closed != b.bot_closed;
    if (iris_changed) (void)out.push_back(gfx::clip_screen(ib));
    if (lid_changed) {
        auto [t, bo] = lid_rect(a, b);
        (void)out.push_back(gfx::clip_screen(t));
        (void)out.push_back(gfx::clip_screen(bo));
    }
    return out;
}

void render(const EyeGeom& g, Rect r, std::span<Rgb565be> out) noexcept {
    for (int y = r.y; y < r.y + r.h; ++y) {
        Rgb565be* row = out.data() + static_cast<std::ptrdiff_t>(y - r.y) * r.w - r.x;
        const int cl = r.x, cr = r.x + r.w;
        for (int x = cl; x < cr; ++x) row[x] = g.bg;
        Span sc{0, 0};
        if (g.sclera_on) { sc = rrect_span(g, y); paint(row, sc.a, sc.b, cl, cr, g.sclera_c); }
        if (g.iris_on) {
            Span o = circle_span(g.icx, g.icy, g.ir, y);
            if (sc.b > sc.a) { if (o.a < sc.a) o.a = sc.a; if (o.b > sc.b) o.b = sc.b; }
            paint(row, o.a, o.b, cl, cr, g.ring_c);
            if (g.iring > 0 && g.ir > g.iring) {
                Span in = circle_span(g.icx, g.icy, g.ir - g.iring, y);
                if (in.a < o.a) in.a = o.a;
                if (in.b > o.b) in.b = o.b;
                paint(row, in.a, in.b, cl, cr, g.iris_c);
            } else {
                paint(row, o.a, o.b, cl, cr, g.iris_c);
            }
        }
        if (g.pupil_on && g.pr > 0) { Span p = pupil_span(g, y); paint(row, p.a, p.b, cl, cr, g.pupil_c); }
        if (g.hl_on && g.hr > 0) { Span h = circle_span(g.hx, g.hy, g.hr, y); paint(row, h.a, h.b, cl, cr, g.hl_c); }
        // lids
        const int lo = g.box_l < cl ? cl : g.box_l, hi = g.box_r > cr ? cr : g.box_r;
        if (g.top_closed || g.bot_closed) {
            if ((g.top_closed && y <= g.box_b) || (g.bot_closed && y >= g.box_t))
                for (int x = lo; x < hi; ++x) row[x] = g.lid_c;
            continue;
        }
        for (int x = lo; x < hi; ++x) {
            if (y < top_edge_at(g, x) || y >= bot_edge_at(g, x)) row[x] = g.lid_c;
        }
    }
}

}  // namespace eyes
