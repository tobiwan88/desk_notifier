/*
 * Host preview: renders cat_eyes frames on a PC so styles and animations can
 * be checked without hardware. Build: make -C host   Run: ./host/preview out.rgb
 * Writes raw RGB888 cells (rows = styles, cols = animation snapshots);
 * host/sheet.py turns that into a PNG contact sheet.
 */
#include "cat_eyes.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct { int anim; int at_ms; const char *label; } snap_t;
static const snap_t snaps[] = {
    {-1, 0, "idle"}, {CAT_ANIM_WINK, 300, "wink"}, {CAT_ANIM_SLOW_BLINK, 1100, "slow blink"},
    {CAT_ANIM_ROLL, 550, "roll"}, {CAT_ANIM_SIDE_EYE, 1000, "side-eye"}, {CAT_ANIM_HUNT, 1000, "hunt"},
    {CAT_ANIM_NICTITATE, 220, "3rd lid"}, {CAT_ANIM_SLEEPY, 3300, "sleepy"},
    {CAT_ANIM_CROSS_EYED, 1000, "cross"}, {CAT_ANIM_HAPPY, 800, "happy"}, {CAT_ANIM_SPECIAL, 1000, "special"},
};
#define NS (int)(sizeof(snaps) / sizeof(snaps[0]))

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "out.rgb";
    FILE *fp = fopen(out, "wb");
    if (!fp) return 1;
    static uint16_t buf[240 * 240];
    for (int s = 0; s < CAT_STYLE_COUNT; s++) {
        for (int k = 0; k < NS; k++) {
            cat_eyes_init(12345 + k);
            cat_eyes_set_swap_bytes(false);
            cat_eyes_set_style((cat_style_t)s);
            cat_eyes_set_iris(s == CAT_STYLE_ANIME ? CAT_IRIS_VIOLET : CAT_IRIS_AMBER);
            cat_eyes_set_auto(false);
            uint32_t t = 0;
            for (; t < 1000; t += 33) cat_eyes_tick(t);
            if (snaps[k].anim >= 0) cat_eyes_play((cat_anim_t)snaps[k].anim);
            uint32_t end = t + (uint32_t)snaps[k].at_ms;
            for (; t <= end; t += 33) cat_eyes_tick(t);
            for (int e = 0; e < 2; e++) {
                int y0, y1; (void)cat_eyes_dirty_rows(e, &y0, &y1);
                for (int y = 0; y < 240; y += 20) cat_eyes_render_strip(e, y, 20, buf + y * 240);
                for (int i = 0; i < 240 * 240; i++) {
                    uint16_t c = buf[i];
                    unsigned char px[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)};
                    fwrite(px, 1, 3, fp);
                }
            }
        }
    }
    fclose(fp);
    printf("%d styles x %d snapshots x 2 eyes\n", CAT_STYLE_COUNT, NS);
    return 0;
}
