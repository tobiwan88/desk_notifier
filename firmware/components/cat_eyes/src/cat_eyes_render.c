/*
 * Frame builder + strip rasteriser.
 *
 * Cost model (ESP32-C61 has no FPU, so floats are only used per frame /
 * per column, never per pixel):
 *   per frame : lid curves per column (int), pupil row tables, 64-entry iris
 *               LUT, highlight + decoration shape lists
 *   per pixel : integer compares, one multiply for iris LUT index,
 *               occasional RGB565 blend (highlights, third eyelid, shadow)
 */
#include "cat_eyes_internal.h"
#include <math.h>
#include <string.h>

static int16_t s_shape[CAT_STYLE_COUNT][2 * CE_EYE_HALF_W + 1]; /* sin^exp, Q15 */
static uint64_t s_heart[64];
static uint32_t s_star[32];
static uint8_t s_fibre[128];

/* ---------------- colour helpers ---------------- */
uint16_t ce_rgb(uint32_t c) { return RGB565((c >> 16) & 255, (c >> 8) & 255, c & 255); }

uint16_t ce_blend(uint16_t a, uint16_t b, uint16_t al)
{
    if (al >= 256) return b;
    uint32_t ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
    uint32_t br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    uint32_t r = (ar * (256 - al) + br * al) >> 8;
    uint32_t g = (ag * (256 - al) + bg * al) >> 8;
    uint32_t bl = (ab * (256 - al) + bb * al) >> 8;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

uint32_t ce_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1;
        b >>= 2;
    }
    return r;
}

static uint32_t lerp_rgb(uint32_t a, uint32_t b, float u)
{
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return ((uint32_t)(ar + (br - ar) * u) << 16) | ((uint32_t)(ag + (bg - ag) * u) << 8) | (uint32_t)(ab + (bb - ab) * u);
}

/* ---------------- init: masks and tables ---------------- */
void ce_render_init(void)
{
    for (int s = 0; s < CAT_STYLE_COUNT; s++)
        for (int i = 0; i <= 2 * CE_EYE_HALF_W; i++) {
            float u = i / (2.f * CE_EYE_HALF_W);
            s_shape[s][i] = (int16_t)(32767.f * powf(fmaxf(0.f, sinf(3.14159265f * u)), g_ce_styles[s].shape_exp));
        }
    for (int y = 0; y < 64; y++) {
        uint64_t row = 0;
        for (int x = 0; x < 64; x++) {
            float X = (x - 31.5f) / 31.5f * 1.3f, Y = -(y - 31.5f) / 31.5f * 1.3f + .12f;
            float a = X * X + Y * Y - 1.f;
            if (a * a * a - X * X * Y * Y * Y <= 0) row |= 1ull << x;
        }
        s_heart[y] = row;
    }
    float px[8], py[8];
    for (int i = 0; i < 8; i++) {
        float a = i * 3.14159265f / 4, r = (i & 1) ? .28f : 1.f;
        px[i] = cosf(a) * r; py[i] = sinf(a) * r;
    }
    for (int y = 0; y < 32; y++) {
        uint32_t row = 0;
        for (int x = 0; x < 32; x++) {
            float X = (x - 15.5f) / 15.5f, Y = (y - 15.5f) / 15.5f; bool in = false;
            for (int i = 0, j = 7; i < 8; j = i++)
                if (((py[i] > Y) != (py[j] > Y)) && (X < (px[j] - px[i]) * (Y - py[i]) / (py[j] - py[i]) + px[i])) in = !in;
            if (in) row |= 1u << x;
        }
        s_star[y] = row;
    }
    uint32_t r = 0x1234567u;
    for (int i = 0; i < 128; i++) { r ^= r << 13; r ^= r >> 17; r ^= r << 5; s_fibre[i] = (uint8_t)(r % 60); }
}

/* ---------------- shape list helpers ---------------- */
static ce_shape_t *sh_add(ce_frame_t *f, int layer)
{
    uint8_t *n; ce_shape_t *arr; int cap;
    if (layer == CE_L_FX) { n = &f->n_fx; arr = f->fx; cap = CE_MAX_FX; }
    else if (layer == CE_L_OVER) { n = &f->n_over; arr = f->over; cap = CE_MAX_SHAPES; }
    else { n = &f->n_under; arr = f->under; cap = CE_MAX_SHAPES; }
    if (*n >= cap) return NULL;
    ce_shape_t *s = &arr[(*n)++];
    memset(s, 0, sizeof(*s));
    return s;
}
void ce_sh_circle(ce_frame_t *f, int layer, int x, int y, int r, uint16_t c)
{
    if (r <= 0) return;
    ce_shape_t *s = sh_add(f, layer); if (!s) return;
    s->kind = SH_CIRCLE; s->color = c; s->x0 = x; s->y0 = y; s->r = r;
    s->bx0 = x - r; s->bx1 = x + r; s->by0 = y - r; s->by1 = y + r;
}
void ce_sh_ellipse(ce_frame_t *f, int layer, int x, int y, int rx, int ry, uint16_t c)
{
    if (rx <= 0 || ry <= 0) return;
    ce_shape_t *s = sh_add(f, layer); if (!s) return;
    s->kind = SH_ELLIPSE; s->color = c; s->x0 = x; s->y0 = y; s->r = rx; s->r2 = ry;
    s->bx0 = x - rx; s->bx1 = x + rx; s->by0 = y - ry; s->by1 = y + ry;
}
void ce_sh_capsule(ce_frame_t *f, int layer, int x0, int y0, int x1, int y1, int r, uint16_t c)
{
    if (r <= 0) return;
    ce_shape_t *s = sh_add(f, layer); if (!s) return;
    s->kind = SH_CAPSULE; s->color = c; s->x0 = x0; s->y0 = y0; s->x1 = x1; s->y1 = y1; s->r = r;
    s->bx0 = (x0 < x1 ? x0 : x1) - r; s->bx1 = (x0 > x1 ? x0 : x1) + r;
    s->by0 = (y0 < y1 ? y0 : y1) - r; s->by1 = (y0 > y1 ? y0 : y1) + r;
}
void ce_sh_tri(ce_frame_t *f, int layer, int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c)
{
    ce_shape_t *s = sh_add(f, layer); if (!s) return;
    s->kind = SH_TRI; s->color = c; s->x0 = x0; s->y0 = y0; s->x1 = x1; s->y1 = y1; s->x2 = x2; s->y2 = y2;
    int a = x0 < x1 ? x0 : x1; s->bx0 = a < x2 ? a : x2; a = x0 > x1 ? x0 : x1; s->bx1 = a > x2 ? a : x2;
    a = y0 < y1 ? y0 : y1; s->by0 = a < y2 ? a : y2; a = y0 > y1 ? y0 : y1; s->by1 = a > y2 ? a : y2;
}
void ce_sh_rect(ce_frame_t *f, int layer, int x0, int y0, int x1, int y1, uint16_t c)
{
    ce_shape_t *s = sh_add(f, layer); if (!s) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    s->kind = SH_RECT; s->color = c; s->x0 = x0; s->y0 = y0; s->x1 = x1; s->y1 = y1;
    s->bx0 = x0; s->bx1 = x1; s->by0 = y0; s->by1 = y1;
}
void ce_sh_ring(ce_frame_t *f, int layer, int x, int y, int r_out, int r_in, uint16_t c)
{
    if (r_out <= 0) return;
    if (r_in < 0) r_in = 0;
    if (r_in >= r_out) return;
    ce_shape_t *s = sh_add(f, layer); if (!s) return;
    s->kind = SH_RING; s->color = c; s->x0 = x; s->y0 = y; s->r = r_out; s->r2 = r_in;
    s->bx0 = x - r_out; s->bx1 = x + r_out; s->by0 = y - r_out; s->by1 = y + r_out;
}
void ce_sh_glyph(ce_frame_t *f, int layer, int x, int y, int scale, int glyph, uint16_t c)
{
    if (scale <= 0 || glyph < 0 || glyph >= CE_GLYPH_COUNT) return;
    ce_shape_t *s = sh_add(f, layer); if (!s) return;
    s->kind = SH_GLYPH; s->color = c; s->x0 = x; s->y0 = y; s->r = scale; s->r2 = glyph;
    s->bx0 = x; s->bx1 = x + 5 * scale - 1; s->by0 = y; s->by1 = y + 7 * scale - 1;
}
void ce_sh_clip(ce_frame_t *f, int layer, int first, int y0, int y1)
{
    uint8_t n = layer == CE_L_FX ? f->n_fx : layer == CE_L_OVER ? f->n_over : f->n_under;
    ce_shape_t *arr = layer == CE_L_FX ? f->fx : layer == CE_L_OVER ? f->over : f->under;
    for (int i = first < 0 ? 0 : first; i < n; i++) {
        if (arr[i].kind == SH_TRI) continue;
        arr[i].clip = 1; arr[i].x2 = (int16_t)y0; arr[i].y2 = (int16_t)y1;
    }
}

/* 5x7 font, one byte per row, bit 4 = leftmost pixel */
const uint8_t g_ce_font[CE_GLYPH_COUNT][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, /* 2 */
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}, /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, /* 5 */
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, /* 9 */
    {0x19, 0x1A, 0x02, 0x04, 0x08, 0x0B, 0x13}, /* % */
    {0x07, 0x08, 0x1E, 0x08, 0x1E, 0x08, 0x07}, /* euro */
    {0x1F, 0x04, 0x0E, 0x15, 0x04, 0x04, 0x04}, /* kana-ish glyphs */
    {0x0A, 0x0A, 0x12, 0x12, 0x02, 0x04, 0x04},
    {0x08, 0x1E, 0x08, 0x0E, 0x12, 0x12, 0x0C},
    {0x1F, 0x01, 0x05, 0x0A, 0x04, 0x0A, 0x11},
    {0x04, 0x1F, 0x04, 0x0E, 0x15, 0x04, 0x0C},
    {0x1E, 0x02, 0x1F, 0x02, 0x0E, 0x02, 0x1E},
    {0x0E, 0x00, 0x1F, 0x04, 0x0A, 0x11, 0x00},
    {0x11, 0x0A, 0x04, 0x1F, 0x04, 0x04, 0x08},
};

static void sh_row(const ce_shape_t *s, int n, int y, uint16_t *d)
{
    for (int i = 0; i < n; i++, s++) {
        if (y < s->by0 || y > s->by1) continue;
        if (s->clip && (y < s->x2 || y > s->y2)) continue;
        int dy = y - s->y0, xa, xb;
        switch (s->kind) {
        case SH_RECT: xa = s->x0; xb = s->x1; break;
        case SH_RING: {
            int ady = dy < 0 ? -dy : dy;
            if (ady > s->r) continue;
            int xo = (int)ce_isqrt((uint32_t)(s->r * s->r - dy * dy));
            if (ady >= s->r2) { xa = s->x0 - xo; xb = s->x0 + xo; break; }
            int xi = (int)ce_isqrt((uint32_t)(s->r2 * s->r2 - dy * dy));
            for (int x = s->x0 - xo; x <= s->x0 - xi - 1; x++) if (x >= 0 && x < CAT_EYES_W) d[x] = s->color;
            for (int x = s->x0 + xi + 1; x <= s->x0 + xo; x++) if (x >= 0 && x < CAT_EYES_W) d[x] = s->color;
            continue;
        }
        case SH_GLYPH: {
            int gy = dy / s->r;
            if (gy < 0 || gy > 6) continue;
            uint8_t bits = g_ce_font[s->r2][gy];
            for (int c = 0; c < 5; c++) {
                if (!((bits >> (4 - c)) & 1)) continue;
                int xs = s->x0 + c * s->r, xe = xs + s->r - 1;
                if (xs < 0) xs = 0;
                if (xe > 239) xe = 239;
                for (int x = xs; x <= xe; x++) d[x] = s->color;
            }
            continue;
        }
        case SH_CIRCLE: {
            int hw = (int)ce_isqrt((uint32_t)(s->r * s->r - dy * dy));
            xa = s->x0 - hw; xb = s->x0 + hw; break;
        }
        case SH_ELLIPSE: {
            int hw = s->r * (int)ce_isqrt((uint32_t)(s->r2 * s->r2 - dy * dy)) / s->r2;
            xa = s->x0 - hw; xb = s->x0 + hw; break;
        }
        case SH_CAPSULE: {
            int ex = s->x1 - s->x0, ey = s->y1 - s->y0, l2 = ex * ex + ey * ey;
            int xs = s->bx0 < 0 ? 0 : s->bx0, xe = s->bx1 > 239 ? 239 : s->bx1;
            for (int x = xs; x <= xe; x++) {
                int qx = x - s->x0, qy = dy, t = l2 ? ((qx * ex + qy * ey) * 256) / l2 : 0;
                if (t < 0) t = 0; else if (t > 256) t = 256;
                int rx = qx - ex * t / 256, ry = qy - ey * t / 256;
                if (rx * rx + ry * ry <= s->r * s->r) d[x] = s->color;
            }
            continue;
        }
        default: { /* triangle via edge functions, either winding */
            int xs = s->bx0 < 0 ? 0 : s->bx0, xe = s->bx1 > 239 ? 239 : s->bx1;
            for (int x = xs; x <= xe; x++) {
                int e0 = (s->x1 - s->x0) * (y - s->y0) - (s->y1 - s->y0) * (x - s->x0);
                int e1 = (s->x2 - s->x1) * (y - s->y1) - (s->y2 - s->y1) * (x - s->x1);
                int e2 = (s->x0 - s->x2) * (y - s->y2) - (s->y0 - s->y2) * (x - s->x2);
                if ((e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0)) d[x] = s->color;
            }
            continue;
        }
        }
        if (xa < 0) xa = 0;
        if (xb > 239) xb = 239;
        for (int x = xa; x <= xb; x++) d[x] = s->color;
    }
}

/* ---------------- frame builder ---------------- */
static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

static void band(const int16_t *line, const uint8_t *ok, int w, int16_t *lo, int16_t *hi)
{
    for (int x = 0; x < CAT_EYES_W; x++) {
        if (!ok[x]) { lo[x] = 1; hi[x] = 0; continue; }
        int mn = line[x], mx = line[x];
        if (x > 0 && ok[x - 1]) { if (line[x - 1] < mn) mn = line[x - 1]; if (line[x - 1] > mx) mx = line[x - 1]; }
        if (x < 239 && ok[x + 1]) { if (line[x + 1] < mn) mn = line[x + 1]; if (line[x + 1] > mx) mx = line[x + 1]; }
        lo[x] = (int16_t)(mn - w / 2); hi[x] = (int16_t)(mx + (w - w / 2) - 1);
    }
}

void ce_build_frame(int eye, const float *p, uint32_t T)
{
    ce_frame_t *f = &g_ce.f[eye];
    const ce_style_t *st = &g_ce_styles[g_ce.style];
    memcpy(f->p, p, sizeof(f->p));
    f->st = st;
    f->n_fx = 0; f->tint_shift = 0; f->tint_add = 0; f->fx_lo = CAT_EYES_H; f->fx_hi = 0;
    f->side = eye == 0 ? 1 : -1;
    int side = f->side;

    int pal_id = st->fixed_palette >= 0 ? st->fixed_palette
               : g_ce.iris == CAT_IRIS_ODD ? (eye ? CAT_IRIS_AMBER : CAT_IRIS_BLUE) : (int)g_ce.iris;
    const ce_palette_t *pal = &g_ce_palettes[pal_id];
    f->c_light = ce_rgb(pal->light); f->c_mid = ce_rgb(pal->mid); f->c_dark = ce_rgb(pal->dark);
    f->bg = ce_rgb(st->bg);
    f->ink = st->iris == IRIS_RING ? f->c_mid : ce_rgb(st->ink);
    f->opening = st->iris == IRIS_FLAT ? f->c_mid : ce_rgb(st->opening);

    /* ---- lid curves, one evaluation per column ---- */
    int up = clampi((int)(p[P_UPPER] * 256), 0, 256), lo = clampi((int)(p[P_LOWER] * 256), 0, 256);
    int tilt = (int)(p[P_TILT] * 256);
    int open_q8 = 0;
    f->x_min = 239; f->x_max = 0;
    for (int x = 0; x < CAT_EYES_W; x++) {
        int lx = (x - CE_CX) * side;
        f->col_ok[x] = 0;
        if (lx <= -CE_EYE_HALF_W || lx >= CE_EYE_HALF_W) continue;
        int ui = lx + CE_EYE_HALF_W;
        int s = s_shape[g_ce.style][ui];
        int b = st->yo * 256 + ((st->yi - st->yo) * ui * 256) / (2 * CE_EYE_HALF_W);
        int t0 = b - ((st->ht * s) >> 7), b0 = b + ((st->hb * s) >> 7);
        int c = b + ((((st->hb * s) >> 7) * 90) >> 8);
        int a = tilt * (ui - CE_EYE_HALF_W) * 40 / (2 * CE_EYE_HALF_W);
        int t = t0 + (((c - t0) * up) >> 8) + ((a * s) >> 15);
        if (t < t0) t = t0;
        int bt = b0 - (((b0 - c) * lo) >> 8);
        if (t > bt) t = bt;
        if (bt - t > open_q8) open_q8 = bt - t;
        f->top[x] = (int16_t)((t + CE_CY * 256 + 128) >> 8);
        f->bot[x] = (int16_t)((bt + CE_CY * 256 + 128) >> 8);
        f->cl[x] = (int16_t)((c + CE_CY * 256 + 128) >> 8);
        f->col_ok[x] = 1;
        if (x < f->x_min) f->x_min = (int16_t)x;
        if (x > f->x_max) f->x_max = (int16_t)x;
    }
    f->open = open_q8 >= 384;
    f->smile_mode = st->smile_arcs && p[P_SMILE] > .5f;

    if (f->smile_mode) {
        /* "^" eyes: quadratic arc, x is linear in the curve parameter */
        uint8_t ok[CAT_EYES_W];
        for (int x = 0; x < CAT_EYES_W; x++) {
            int lx = (x - CE_CX) * side; ok[x] = lx >= -70 && lx <= 70;
            float s = (lx + 70) / 140.f;
            f->smile[x] = (int16_t)lrintf((1 - s) * (1 - s) * 20 + 2 * s * (1 - s) * -50 + s * s * 20 + CE_CY);
        }
        band(f->smile, ok, 8, f->top_lo, f->top_hi);
        memcpy(f->col_ok, ok, sizeof(ok));
        f->open = false;
    } else if (f->open) {
        band(f->top, f->col_ok, st->top_ink, f->top_lo, f->top_hi);
        band(f->bot, f->col_ok, st->bot_ink, f->bot_lo, f->bot_hi);
    } else {
        band(f->cl, f->col_ok, st->closed_ink, f->top_lo, f->top_hi);
    }

    /* ---- iris ---- */
    f->icx = (int16_t)lrintf(CE_CX + p[P_GX] * 38);
    f->icy = (int16_t)lrintf(CE_CY + p[P_GY] * 26 + 4);
    f->ir = st->iris_r; f->ir2 = (uint32_t)(f->ir * f->ir); f->inv_ir2 = (64u << 16) / f->ir2;
    float fx = p[P_FX] < 0 ? 0 : p[P_FX] > 1 ? 1 : p[P_FX];
    if (st->iris == IRIS_GRADIENT || st->iris == IRIS_FIRE) {
        float s1 = .55f, s2 = .92f; uint32_t rim = 0x120a02;
        if (st->iris == IRIS_FIRE) {
            float fl = .85f + .15f * sinf(T / 63.f) * sinf(T / 151.f);
            s1 = .45f * fl + fx * .3f; s2 = .9f; rim = 0x1a0000;
        }
        for (int i = 0; i < 64; i++) {
            float r = sqrtf((i + .5f) / 64.f); uint32_t c;
            if (r < s1) c = lerp_rgb(pal->light, pal->mid, r / s1);
            else if (r < s2) c = lerp_rgb(pal->mid, pal->dark, (r - s1) / (s2 - s1));
            else c = lerp_rgb(pal->dark, rim, (r - s2) / (1 - s2));
            uint16_t c5 = ce_rgb(c);
            if (st->iris == IRIS_FIRE && fx > 0) c5 = ce_blend(c5, RGB565(255, 240, 120), (uint16_t)(fx * 90));
            f->lut[i] = c5;
        }
    }
    int R = f->ir;
    f->e1y = (int16_t)(-R * 95 / 100); f->e1rx = (int16_t)(R * 11 / 10); f->e1ry = (int16_t)(R * 3 / 4);
    f->e2y = (int16_t)(R * 95 / 100);  f->e2rx = (int16_t)(R * 4 / 5);   f->e2ry = (int16_t)(R * 45 / 100);
    if (st->iris == IRIS_CUTE) { f->e1y = (int16_t)(R * 35 / 100); f->e1rx = f->e1ry = (int16_t)(R * 6 / 10); }

    /* ---- pupil row tables ---- */
    float pp = p[P_PUPIL] < 0 ? 0 : p[P_PUPIL] > 1 ? 1 : p[P_PUPIL];
    for (int i = 0; i < 96; i++) f->pup_a[i] = f->pup_b[i] = -1;
    f->heart = false; f->pup_inner = 0; f->pup_outline = 0;
    f->pup_fill = RGB565(5, 5, 5);
    if (st->pupil == PUPIL_SLIT) {
        float hw = (st->iris == IRIS_FIRE ? 1.5f : 3.f) + 44.f * powf(pp, 1.2f), hh = 70 - 6 * pp;
        f->pup_hh = (int16_t)hh;
        for (int dy = 0; dy <= f->pup_hh && dy < 96; dy++) {
            float m = 1.f - (dy * dy) / (hh * hh); if (m < 0) m = 0;
            f->pup_a[dy] = (int16_t)lrintf(hw * ((1 - pp) * m + pp * sqrtf(m)));
        }
        if (st->iris == IRIS_RING) { f->pup_fill = ce_blend(0, f->c_mid, 85); f->pup_outline = f->c_light; }
    } else if (st->pupil == PUPIL_OVAL) {
        float rx = 12 + 30 * pp, ry = 44 + 12 * pp, rx2 = 6 + 26 * pp, ry2 = 38 + 12 * pp;
        f->pup_hh = (int16_t)ry;
        for (int dy = 0; dy <= f->pup_hh && dy < 96; dy++) {
            f->pup_a[dy] = (int16_t)lrintf(rx * sqrtf(fmaxf(0, 1 - dy * dy / (ry * ry))));
            if (dy <= ry2) f->pup_b[dy] = (int16_t)lrintf(rx2 * sqrtf(fmaxf(0, 1 - dy * dy / (ry2 * ry2))));
        }
        f->pup_fill = f->c_dark; f->pup_inner = RGB565(0x12, 0x08, 0x12);
    } else {
        float r = 26 + 34 * pp;
        if (fx > .3f) { f->heart = true; f->heart_half = (int16_t)(r * .9f * fx + 8); f->heart_color = RGB565(255, 61, 110); }
        f->pup_hh = (int16_t)r;
        for (int dy = 0; dy <= f->pup_hh && dy < 96; dy++) f->pup_a[dy] = (int16_t)lrintf(sqrtf(fmaxf(0, r * r - dy * dy)));
    }

    /* ---- highlights (screen space: light from top-left for both eyes) ---- */
    f->n_hl = 0;
#define HL(k, ox, oy, rx_, ry_, a) do { ce_hl_t *h = &f->hl[f->n_hl++]; h->kind = (k); \
    h->x = (int16_t)(f->icx + (ox)); h->y = (int16_t)(f->icy + (oy)); h->rx = (int16_t)(rx_); h->ry = (int16_t)(ry_); \
    h->color = 0xffff; h->alpha = (uint16_t)(a); } while (0)
    switch (st->hl) {
    case HL_ANIME:
        HL(0, -30, -34, 22, 15, 256); HL(0, 28, 30, 8, 8, 256); HL(0, 8, -46, 4, 4, 256);
        if (fx > .02f) { int s = (int)(16 + 6 * sinf(T / 90.f)); HL(1, 22, -20, s, s, fx * 256); HL(1, -26, 24, 10, 10, fx * 256); }
        break;
    case HL_CUTE: HL(0, -28, -30, 18, 18, 256); HL(0, 24, 24, 8, 8, 256); HL(0, -6, -48, 5, 5, 256); break;
    case HL_STAR: { int s = (int)(14 * (1 + fx * (.8f + .4f * sinf(T / 70.f)))); HL(1, -28, -32, s, s, 256); HL(0, 22, 28, 4, 4, 256); break; }
    case HL_DIM: HL(0, -22, -28, 4, 4, 128); break;
    default: { int a = st->iris == IRIS_GRADIENT ? 218 : 256; HL(0, -28, -34, 13, 10, a); HL(0, 22, 28, 5, 5, a); break; }
    }
#undef HL

    /* ---- third eyelid ---- */
    float nict = p[P_NICT];
    f->nict_on = nict > .01f;
    f->nict_xe = (int16_t)lrintf(CE_EYE_HALF_W - nict * 2 * CE_EYE_HALF_W * .8f);
    f->nict_color = ce_rgb(st->nict_rgb); f->nict_alpha = st->nict_alpha;
    f->nict_edge = RGB565(150, 120, 100);

    /* ---- decoration shapes ---- */
    f->n_under = f->n_over = 0;
    if (st->fur) {
        uint16_t c = ce_blend(f->bg, 0, 46);
        for (int i = 0; i < 9; i++) {
            int x = 30 + i * 22;
            ce_sh_capsule(f, CE_L_UNDER, x, 0, x + 6, 26, 1, c); ce_sh_capsule(f, CE_L_UNDER, x + 6, 26, x, 52, 1, c);
        }
    }
    if (st->moon_bats) {
        int mx = eye ? 200 : 40;
        ce_sh_circle(f, CE_L_UNDER, mx, 40, 16, RGB565(0xf4, 0xc5, 0x42));
        ce_sh_circle(f, CE_L_UNDER, mx + 7, 35, 14, f->bg);
        int flap = (int)lrintf(sinf(T / 80.f) * 4);
        for (int i = 0; i < CE_BATS; i++) {
            float a = g_ce.bats[i].a + T * g_ce.bats[i].v * .06f;
            int bx = (int)lrintf(120 + cosf(a) * g_ce.bats[i].r), by = (int)lrintf(60 + sinf(a * 2) * 25);
            ce_sh_tri(f, CE_L_UNDER, bx, by - 1, bx - 14, by - 2 + flap, bx - 4, by + 3, 0);
            ce_sh_tri(f, CE_L_UNDER, bx, by - 1, bx + 14, by - 2 + flap, bx + 4, by + 3, 0);
            ce_sh_circle(f, CE_L_UNDER, bx, by, 3, 0);
        }
    }
    if (st->snow)
        for (int i = 0; i < CE_SNOW; i++)
            ce_sh_circle(f, CE_L_UNDER, (int)g_ce.snow[eye][i].x, (int)g_ce.snow[eye][i].y, (int)(g_ce.snow[eye][i].r + .5f), 0xffff);
    if (st->blush) ce_sh_ellipse(f, CE_L_UNDER, CE_CX - side * 40, 220, 26, 12, ce_blend(f->bg, RGB565(255, 110, 140), 115));

    /* line the decorations follow: open upper lid, or the closed line */
    const int16_t *tl = f->open ? f->top : f->cl;
#define SX(lx) (CE_CX + side * (lx))
#define TL(lx) (tl[clampi(SX(lx), 0, 239)])
    if (f->smile_mode) {
        ce_sh_circle(f, CE_L_OVER, SX(-70), 20 + CE_CY, 4, f->ink); ce_sh_circle(f, CE_L_OVER, SX(70), 20 + CE_CY, 4, f->ink);
    } else {
        if (st->lashes)
            for (int k = 0, i = 1; i <= 7; i += 3, k++) {
                int lx = -CE_EYE_HALF_W + 2 * CE_EYE_HALF_W * i / 36, y = TL(lx);
                ce_sh_capsule(f, CE_L_OVER, SX(lx), y, SX(lx - 18 - 2 * k), y - 6 - 6 * k, 3, f->ink);
            }
        if (st->demon_brow) {
            for (int i = 8; i <= 24; i += 8) {
                int lx = -CE_EYE_HALF_W + 2 * CE_EYE_HALF_W * i / 36, y = TL(lx);
                ce_sh_tri(f, CE_L_OVER, SX(lx - 8), y + 2, SX(lx + 2), y - 16, SX(lx + 8), y + 2, 0);
            }
            ce_sh_capsule(f, CE_L_OVER, SX(-110), TL(-100) - 34, SX(104), TL(100) - 40 + (int)(p[P_TILT] * 30), 6, 0);
        }
        if (st->santa_trim) {
            for (int i = 1; i < 36; i += 2) {
                int lx = -CE_EYE_HALF_W + 2 * CE_EYE_HALF_W * i / 36, y = TL(lx);
                ce_sh_circle(f, CE_L_OVER, SX(lx), y - 4, 8, RGB565(0xdd, 0xdd, 0xdd));
                ce_sh_circle(f, CE_L_OVER, SX(lx), y - 4, 7, 0xffff);
            }
            ce_sh_circle(f, CE_L_OVER, SX(-118), TL(-100) - 14, 11, 0xffff);
        }
        if (st->corner_flick)
            ce_sh_capsule(f, CE_L_OVER, SX(-104), st->yo + CE_CY, SX(-120), st->yo + CE_CY - 12, 2, f->ink);
    }
#undef TL
#undef SX

    /* ---- rows touched this frame (static background excluded) ---- */
    int ylo = 239, yhi = 0;
    for (int x = 0; x < CAT_EYES_W; x++) {
        if (!f->col_ok[x]) continue;
        if (f->top_lo[x] < ylo) ylo = f->top_lo[x];
        int h = f->open ? f->bot_hi[x] : f->top_hi[x];
        if (h > yhi) yhi = h;
    }
    f->y_lo = (int16_t)(ylo - 45); f->y_hi = (int16_t)(yhi + 12);
    if (st->blush && f->y_hi < 233) f->y_hi = 233; /* keep cheeks in the pushed region */
}

/* ---------------- per-pixel eye interior ---------------- */
static inline bool in_ell(int dx, int dy, int rx, int ry)
{
    return (int32_t)dx * dx * ry * ry + (int32_t)dy * dy * rx * rx <= (int32_t)rx * rx * ry * ry;
}

static uint16_t interior(const ce_frame_t *f, int x, int y)
{
    const ce_style_t *st = f->st;
    int dx = x - f->icx, dy = y - f->icy;
    uint32_t d2 = (uint32_t)(dx * dx + dy * dy);
    uint16_t c = f->opening;
    int R = f->ir;

    if (d2 < f->ir2) {
        switch (st->iris) {
        case IRIS_GRADIENT: case IRIS_FIRE: {
            uint32_t i = (d2 * f->inv_ir2) >> 16; c = f->lut[i > 63 ? 63 : i];
            if (st->fibres && d2 * 25 > f->ir2) {
                int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, s = ax + ay;
                int pa = s ? dy * 32 / s : 0, idx = (dx >= 0 ? pa + 32 : 96 - pa) & 127;
                c = ce_blend(c, 0, s_fibre[idx]);
            }
            break;
        }
        case IRIS_FLAT: c = d2 > (uint32_t)((R - 6) * (R - 6)) ? f->c_dark : f->c_mid; break;
        case IRIS_RING: c = (d2 * 4 >= (uint32_t)((2 * R - 11) * (2 * R - 11)) && d2 * 4 <= (uint32_t)((2 * R - 5) * (2 * R - 5))) ? f->c_mid : f->opening; break;
        case IRIS_ANIME:
            c = f->c_mid;
            if (in_ell(dx, dy - f->e1y, f->e1rx, f->e1ry)) c = f->c_dark;
            else if (in_ell(dx, dy - f->e2y, f->e2rx, f->e2ry)) c = f->c_light;
            if (d2 * 4 >= (uint32_t)((2 * R - 9) * (2 * R - 9))) c = f->c_dark;
            break;
        case IRIS_CUTE:
            c = f->c_dark;
            if (in_ell(dx, dy - f->e1y, f->e1rx, f->e1ry)) c = f->c_mid;
            break;
        }
    }

    /* pupil */
    int ady = dy < 0 ? -dy : dy, adx = dx < 0 ? -dx : dx;
    if (f->heart) {
        int h = f->heart_half;
        if (adx <= h && ady <= h) {
            int mx = 32 + dx * 32 / h, my = 32 + dy * 32 / h;
            if (mx >= 0 && mx < 64 && my >= 0 && my < 64 && ((s_heart[my] >> mx) & 1)) c = f->heart_color;
        }
    } else if (ady <= f->pup_hh && ady < 96) {
        int w = f->pup_a[ady];
        if (w >= 0 && adx <= w) {
            c = f->pup_fill;
            if (f->pup_inner && f->pup_b[ady] >= 0 && adx <= f->pup_b[ady]) c = f->pup_inner;
            if (f->pup_outline && (adx >= w - 1 || ady >= f->pup_hh - 1)) c = f->pup_outline;
        }
    }

    /* highlights */
    for (int i = 0; i < f->n_hl; i++) {
        const ce_hl_t *h = &f->hl[i];
        int hx = x - h->x, hy = y - h->y;
        if (hx < -h->rx || hx > h->rx || hy < -h->ry || hy > h->ry) continue;
        bool in;
        if (h->kind == 0) in = in_ell(hx, hy, h->rx, h->ry);
        else { int mx = 16 + hx * 16 / h->rx, my = 16 + hy * 16 / h->ry;
               in = mx >= 0 && mx < 32 && my >= 0 && my < 32 && ((s_star[my] >> mx) & 1); }
        if (in) c = ce_blend(c, h->color, h->alpha);
    }

    /* third eyelid, sweeping in from the inner corner */
    if (f->nict_on) {
        int lx = (x - CE_CX) * f->side, dyl = y - CE_CY;
        int edge = f->nict_xe - 2 + 24 * dyl * dyl / 10000;
        if (lx > edge + 1) c = ce_blend(c, f->nict_color, f->nict_alpha);
        else if (lx >= edge - 1) c = ce_blend(c, f->nict_edge, 204);
    }

    if (st->lid_shadow && y - f->top[x] < 9) c = ce_blend(c, 0, 102);
    return c;
}

void cat_eyes_render_strip(int eye, int y0, int h, uint16_t *dst)
{
    if ((unsigned)eye > 1 || !dst) return;
    const ce_frame_t *f = &g_ce.f[eye];
    if (!f->st) { memset(dst, 0, (size_t)h * CAT_EYES_W * 2); return; }
    for (int r = 0; r < h; r++) {
        int y = y0 + r;
        uint16_t *d = dst + r * CAT_EYES_W;
        for (int x = 0; x < CAT_EYES_W; x++) d[x] = f->bg;
        sh_row(f->under, f->n_under, y, d);
        for (int x = f->smile_mode ? 0 : f->x_min; x <= (f->smile_mode ? 239 : f->x_max); x++) {
            if (!f->col_ok[x]) continue;
            if (f->open) {
                if ((y >= f->top_lo[x] && y <= f->top_hi[x]) || (y >= f->bot_lo[x] && y <= f->bot_hi[x])) d[x] = f->ink;
                else if (y >= f->top[x] && y < f->bot[x]) d[x] = interior(f, x, y);
            } else if (y >= f->top_lo[x] && y <= f->top_hi[x]) {
                d[x] = f->ink;
            }
        }
        sh_row(f->over, f->n_over, y, d);
        if (f->tint_shift) {
            const uint16_t add = f->tint_add;
            if (f->tint_shift == 1) for (int x = 0; x < CAT_EYES_W; x++) d[x] = (uint16_t)(((d[x] >> 1) & 0x7BEF) + add);
            else for (int x = 0; x < CAT_EYES_W; x++) d[x] = (uint16_t)(((d[x] >> 2) & 0x39E7) + add);
        }
        if (f->n_fx) sh_row(f->fx, f->n_fx, y, d);
        if (g_ce.swap) for (int x = 0; x < CAT_EYES_W; x++) d[x] = (uint16_t)((d[x] >> 8) | (d[x] << 8));
    }
}
