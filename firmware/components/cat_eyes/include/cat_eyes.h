/*
 * cat_eyes - procedural cat eyes for two 240x240 round displays (GC9A01).
 *
 * Pure C, no ESP-IDF dependencies in the core: it renders RGB565 rows into a
 * caller-provided buffer, so it plugs into any strip / DMA display driver.
 *
 * Threading: NOT thread-safe. Call every function from the render task.
 * Other tasks (HTTP etc.) should post commands to the render task's queue
 * and the render task calls the setters below.
 *
 * Typical loop (see examples/cat_eyes_task.c):
 *   cat_eyes_init(esp_random());
 *   for (;;) {
 *       cat_eyes_tick(now_ms);
 *       for (eye = 0..1) if (cat_eyes_dirty_rows(eye, &y0, &y1))
 *           for (y = y0; y < y1; y += STRIP)
 *               cat_eyes_render_strip(eye, y, h, buf); -> push to panel[eye]
 *   }
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAT_EYES_W 240
#define CAT_EYES_H 240

/* Eye 0 = the display on the viewer's LEFT, eye 1 = viewer's RIGHT.
 * The renderer mirrors eye 1 so both inner corners point at the nose. */

typedef enum {
    CAT_STYLE_REALISTIC = 0,
    CAT_STYLE_FLAT,
    CAT_STYLE_LINE_ART,
    CAT_STYLE_ANIME,
    CAT_STYLE_CUTE,
    CAT_STYLE_HALLOWEEN, /* demon Halloween: fixed fire palette, bats, moon */
    CAT_STYLE_CHRISTMAS, /* fixed green palette, snow, Santa trim         */
    CAT_STYLE_COUNT
} cat_style_t;

typedef enum {
    CAT_IRIS_AMBER = 0,
    CAT_IRIS_GREEN,
    CAT_IRIS_BLUE,
    CAT_IRIS_COPPER,
    CAT_IRIS_VIOLET,
    CAT_IRIS_ODD, /* odd-eyed: eye 0 blue, eye 1 amber */
    CAT_IRIS_COUNT
} cat_iris_t;

typedef enum {
    CAT_ANIM_NONE = -1,
    CAT_ANIM_WINK = 0,
    CAT_ANIM_SLOW_BLINK,
    CAT_ANIM_ROLL,
    CAT_ANIM_SIDE_EYE,
    CAT_ANIM_HUNT,
    CAT_ANIM_STARTLE,
    CAT_ANIM_NICTITATE, /* third eyelid */
    CAT_ANIM_SLEEPY,
    CAT_ANIM_CROSS_EYED,
    CAT_ANIM_HAPPY,
    CAT_ANIM_GLANCE,
    CAT_ANIM_SPECIAL, /* style specific: sparkle / heart eyes / flare / twinkle */
    CAT_ANIM_COUNT
} cat_anim_t;

/* Lifecycle ------------------------------------------------------------- */
void cat_eyes_init(uint32_t seed);

/* Settings (take effect on the next tick) -------------------------------- */
void cat_eyes_set_style(cat_style_t style);
void cat_eyes_set_iris(cat_iris_t iris);
void cat_eyes_set_auto(bool random_idle);      /* default true            */
void cat_eyes_set_swap_bytes(bool swap);       /* default true: big-endian
                                                  RGB565 as GC9A01 expects */
cat_style_t cat_eyes_get_style(void);
cat_iris_t cat_eyes_get_iris(void);            /* additions for the host project */
bool cat_eyes_get_auto(void);
void cat_eyes_force_full(int eye);             /* next dirty_rows() reports the whole eye (-1 = both) */

/* Control --------------------------------------------------------------- */
void cat_eyes_play(cat_anim_t anim);            /* interrupts current anim */
void cat_eyes_blink(void);
/* Hold gaze at a point (-1..1, +y = down) for hold_ms, then resume idle.  */
void cat_eyes_look(float gx, float gy, uint32_t hold_ms);

/* Name helpers for an HTTP / JSON layer. Return -1 if unknown. */
int cat_eyes_style_from_name(const char *name); /* "anime","halloween",...   */
int cat_eyes_iris_from_name(const char *name);  /* "amber","odd",...         */
int cat_eyes_anim_from_name(const char *name);  /* "wink","slow_blink",...  */
const char *cat_eyes_anim_name(cat_anim_t anim);/* "special" -> style's name */
cat_anim_t cat_eyes_current_anim(void);

/* Frame ----------------------------------------------------------------- */
/* Advance animations and build the next frame for both eyes. Call once per
 * frame (target 30 fps) before rendering any strip.                        */
void cat_eyes_tick(uint32_t now_ms);

/* Rows of `eye` that changed since the previous tick: [*y0, *y1).
 * Returns false when nothing changed (skip the SPI push entirely).         */
bool cat_eyes_dirty_rows(int eye, int *y0, int *y1);

/* Render rows [y0, y0+h) of `eye`, full 240 px width, into dst
 * (240*h uint16_t, RGB565). Safe to call for any rows, in any order.       */
void cat_eyes_render_strip(int eye, int y0, int h, uint16_t *dst);

#ifdef __cplusplus
}
#endif
