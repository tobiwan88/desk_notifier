#pragma once
/* Internal shared state of the cat_eyes component. Not part of the API. */
#include "cat_eyes.h"
#include <stdbool.h>
#include <stdint.h>

#define CE_CX 120       /* eye centre on the 240x240 display */
#define CE_CY 128
#define CE_EYE_HALF_W 104
#define CE_MAX_KEYS 14
#define CE_MAX_TRACKS 8
#define CE_MAX_SHAPES 48
#define CE_MAX_HL 5
#define CE_SNOW 24
#define CE_BATS 3

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

enum { P_GX, P_GY, P_UPPER, P_LOWER, P_PUPIL, P_TILT, P_NICT, P_FX, P_SMILE, P_COUNT };

typedef enum { IRIS_GRADIENT, IRIS_FLAT, IRIS_RING, IRIS_ANIME, IRIS_CUTE, IRIS_FIRE } ce_iris_kind_t;
typedef enum { PUPIL_SLIT, PUPIL_OVAL, PUPIL_ROUND } ce_pupil_kind_t;
typedef enum { HL_SOFT, HL_ANIME, HL_CUTE, HL_STAR, HL_DIM } ce_hl_kind_t;

typedef struct {
    const char *name;
    const char *special_name;      /* NULL = no special animation */
    int8_t yo, yi;                 /* outer / inner corner height (px, +down) */
    uint8_t ht, hb;                /* upper / lower lid curve height */
    float shape_exp;               /* almond exponent: lower = rounder */
    uint8_t iris_r;
    uint32_t bg, ink, opening;     /* RGB888 */
    ce_iris_kind_t iris;
    ce_pupil_kind_t pupil;
    ce_hl_kind_t hl;
    float pupil_min;               /* pupil = min + (1-min)*pupil */
    float pupil_cap;               /* <1: clamp then scale (halloween slit) */
    float tilt_bias, upper_bias;
    uint8_t top_ink, bot_ink, closed_ink;
    bool fibres, lid_shadow, corner_flick, lashes, demon_brow, santa_trim;
    bool blush, smile_arcs, moon_bats, snow, fur;
    int8_t fixed_palette;          /* -1 = follow iris setting */
    uint32_t nict_rgb; uint8_t nict_alpha;
} ce_style_t;

typedef struct { uint32_t light, mid, dark; } ce_palette_t;

extern const ce_style_t g_ce_styles[CAT_STYLE_COUNT];
extern const ce_palette_t g_ce_palettes[]; /* index: cat_iris_t (0..4), 5 fire, 6 xmas */
#define CE_PAL_FIRE 5
#define CE_PAL_XMAS 6

/* ---- animation ---- */
typedef struct { uint16_t t; float v; } ce_key_t;
typedef struct { uint8_t param, n, linear; ce_key_t k[CE_MAX_KEYS]; } ce_track_t;
typedef struct { uint8_t n; ce_track_t tr[CE_MAX_TRACKS]; } ce_tracks_t;
typedef struct { cat_anim_t id; uint16_t dur; bool jitter; ce_tracks_t eye[2]; } ce_anim_t;

/* ---- per-eye frame description built by tick, consumed by render ---- */
typedef enum { SH_CIRCLE, SH_ELLIPSE, SH_CAPSULE, SH_TRI } ce_shape_kind_t;
typedef struct {
    uint8_t kind; uint16_t color;
    int16_t x0, y0, x1, y1, x2, y2, r, r2;
    int16_t bx0, by0, bx1, by1;
} ce_shape_t;

typedef struct {
    uint8_t kind;         /* 0 ellipse/circle, 1 star mask */
    int16_t x, y, rx, ry; /* screen */
    uint16_t color; uint16_t alpha; /* 0..256 */
} ce_hl_t;

typedef struct {
    float p[P_COUNT];                 /* final params used for this frame */
    const ce_style_t *st;
    int side;                         /* +1 eye0, -1 eye1 (mirrored) */
    uint16_t bg, ink, opening;
    /* lid column tables (screen y), valid where col_ok */
    int16_t top[CAT_EYES_W], bot[CAT_EYES_W], cl[CAT_EYES_W];
    int16_t top_lo[CAT_EYES_W], top_hi[CAT_EYES_W], bot_lo[CAT_EYES_W], bot_hi[CAT_EYES_W];
    int16_t smile[CAT_EYES_W];
    uint8_t col_ok[CAT_EYES_W];
    int16_t x_min, x_max;
    bool open, smile_mode;
    /* iris */
    int16_t icx, icy, ir;
    uint32_t ir2, inv_ir2;            /* (64<<16)/r^2 */
    uint16_t lut[64];
    uint16_t c_light, c_mid, c_dark;
    /* pupil: half width per |dy|; -1 outside */
    int16_t pup_a[96], pup_b[96];     /* outer / inner (anime) */
    int16_t pup_hh;
    bool heart; int16_t heart_half;  uint16_t heart_color;
    uint16_t pup_fill, pup_inner, pup_outline;
    /* anime / cute two-tone ellipses (relative to iris centre) */
    int16_t e1y, e1rx, e1ry, e2y, e2rx, e2ry;
    /* highlights */
    uint8_t n_hl; ce_hl_t hl[CE_MAX_HL];
    /* third eyelid */
    bool nict_on; int16_t nict_xe; uint16_t nict_color, nict_edge; uint16_t nict_alpha;
    /* shapes drawn behind / above the eye */
    uint8_t n_under, n_over;
    ce_shape_t under[CE_MAX_SHAPES], over[CE_MAX_SHAPES];
    /* dirty rows */
    int16_t y_lo, y_hi;
} ce_frame_t;

typedef struct {
    bool inited, auto_idle, swap;
    cat_style_t style; cat_iris_t iris;
    uint32_t rng;
    uint32_t now, last_now, frame_no;
    /* animation */
    bool anim_on; ce_anim_t anim; uint32_t anim_start; cat_anim_t last_anim;
    uint32_t next_anim, next_blink, blink_start, next_sacc;
    float gx, gy, tgx, tgy; uint32_t look_until;
    float pupil_s[2];
    /* background particles */
    struct { float x, y, v, r; } snow[2][CE_SNOW];
    struct { float a, r, v; } bats[CE_BATS];
    ce_frame_t f[2];
    int16_t prev_lo[2], prev_hi[2];
    uint32_t prev_hash[2];
    bool force_full[2];
} ce_state_t;

extern ce_state_t g_ce;

/* helpers */
uint32_t ce_rand(void);
float ce_randf(float a, float b);
uint16_t ce_rgb(uint32_t rgb888);
uint16_t ce_blend(uint16_t a, uint16_t b, uint16_t alpha256); /* a->b */
uint32_t ce_isqrt(uint32_t v);

/* anim */
void ce_anim_build(cat_anim_t id, ce_anim_t *out);
float ce_track_eval(const ce_track_t *t, uint32_t ms);

/* render */
void ce_render_init(void);
void ce_build_frame(int eye, const float *params, uint32_t now);
