/* Style table. Each style is pure data: add a new look by adding a row. */
#include "cat_eyes_internal.h"

const ce_palette_t g_ce_palettes[] = {
    [CAT_IRIS_AMBER]  = {0xffe27a, 0xf2a91f, 0x6e4205},
    [CAT_IRIS_GREEN]  = {0xeaff9e, 0x8fc23a, 0x2b5410},
    [CAT_IRIS_BLUE]   = {0xd9f3ff, 0x5aa9e6, 0x1a3a72},
    [CAT_IRIS_COPPER] = {0xffc48a, 0xd2691e, 0x552606},
    [CAT_IRIS_VIOLET] = {0xf1d9ff, 0xa46be0, 0x3d1a6e},
    [CE_PAL_FIRE]     = {0xfff3a0, 0xff6a00, 0x8a0000},
    [CE_PAL_XMAS]     = {0xd8ffb0, 0x2fa34a, 0x0b4a1c},
};

#define LIDS_STD .yo = -16, .yi = 10, .ht = 74, .hb = 58, .shape_exp = 0.8f, .iris_r = 86
#define NICT_SKIN .nict_rgb = 0xe4dacd, .nict_alpha = 225

const ce_style_t g_ce_styles[CAT_STYLE_COUNT] = {
    [CAT_STYLE_REALISTIC] = {
        .name = "realistic", LIDS_STD,
        .bg = 0x4a3a2c, .ink = 0x140d08, .opening = 0x1a1208,
        .iris = IRIS_GRADIENT, .pupil = PUPIL_SLIT, .hl = HL_SOFT,
        .pupil_min = 0, .pupil_cap = 1,
        .top_ink = 6, .bot_ink = 4, .closed_ink = 7,
        .fibres = true, .lid_shadow = true, .corner_flick = true, .fur = true,
        .fixed_palette = -1, NICT_SKIN,
    },
    [CAT_STYLE_FLAT] = {
        .name = "flat", LIDS_STD,
        .bg = 0xe59a4c, .ink = 0x3a2210, .opening = 0,
        .iris = IRIS_FLAT, .pupil = PUPIL_SLIT, .hl = HL_SOFT,
        .pupil_min = 0, .pupil_cap = 1,
        .top_ink = 6, .bot_ink = 4, .closed_ink = 7,
        .corner_flick = true, .fixed_palette = -1, NICT_SKIN,
    },
    [CAT_STYLE_LINE_ART] = {
        .name = "line", LIDS_STD,
        .bg = 0x000000, .ink = 0, .opening = 0x000000,
        .iris = IRIS_RING, .pupil = PUPIL_SLIT, .hl = HL_SOFT,
        .pupil_min = 0, .pupil_cap = 1,
        .top_ink = 3, .bot_ink = 2, .closed_ink = 3,
        .fixed_palette = -1, .nict_rgb = 0xffffff, .nict_alpha = 64,
    },
    [CAT_STYLE_ANIME] = {
        .name = "anime", .special_name = "sparkle",
        .yo = -10, .yi = 8, .ht = 84, .hb = 62, .shape_exp = 0.62f, .iris_r = 94,
        .bg = 0xf6dcc6, .ink = 0x24131f, .opening = 0xffffff,
        .iris = IRIS_ANIME, .pupil = PUPIL_OVAL, .hl = HL_ANIME,
        .pupil_min = 0.3f, .pupil_cap = 1,
        .top_ink = 9, .bot_ink = 3, .closed_ink = 7,
        .corner_flick = true, .lashes = true, .smile_arcs = true,
        .fixed_palette = -1, NICT_SKIN,
    },
    [CAT_STYLE_CUTE] = {
        .name = "cute", .special_name = "heart_eyes",
        .yo = 0, .yi = 0, .ht = 84, .hb = 74, .shape_exp = 0.5f, .iris_r = 96,
        .bg = 0xffd3df, .ink = 0x4a2a36, .opening = 0xffffff,
        .iris = IRIS_CUTE, .pupil = PUPIL_ROUND, .hl = HL_CUTE,
        .pupil_min = 0.5f, .pupil_cap = 1,
        .top_ink = 6, .bot_ink = 4, .closed_ink = 7,
        .blush = true, .smile_arcs = true, .fixed_palette = -1, NICT_SKIN,
    },
    [CAT_STYLE_HALLOWEEN] = {
        .name = "halloween", .special_name = "demon_flare",
        .yo = -26, .yi = 14, .ht = 60, .hb = 46, .shape_exp = 1.05f, .iris_r = 80,
        .bg = 0x1b0f26, .ink = 0x000000, .opening = 0x1a1208,
        .iris = IRIS_FIRE, .pupil = PUPIL_SLIT, .hl = HL_DIM,
        .pupil_min = 0, .pupil_cap = 0.5f, .tilt_bias = 0.3f, .upper_bias = 0.06f,
        .top_ink = 6, .bot_ink = 4, .closed_ink = 7,
        .lid_shadow = true, .corner_flick = true, .demon_brow = true, .moon_bats = true,
        .fixed_palette = CE_PAL_FIRE, .nict_rgb = 0x783c8c, .nict_alpha = 218,
    },
    [CAT_STYLE_CHRISTMAS] = {
        .name = "christmas", .special_name = "twinkle", LIDS_STD,
        .bg = 0xb3192b, .ink = 0x2a0a0a, .opening = 0x1a1208,
        .iris = IRIS_GRADIENT, .pupil = PUPIL_SLIT, .hl = HL_STAR,
        .pupil_min = 0, .pupil_cap = 1,
        .top_ink = 6, .bot_ink = 4, .closed_ink = 7,
        .fibres = true, .lid_shadow = true, .santa_trim = true, .snow = true,
        .fixed_palette = CE_PAL_XMAS, NICT_SKIN,
    },
};
