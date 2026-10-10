# cat_eyes — procedural cat eyes for 2 × GC9A01 (ESP-IDF component)

![all styles and animations](docs/preview.png)

## Add it in 3 steps
1. Copy this folder to `<your project>/components/cat_eyes/`.
2. In the CMakeLists.txt of the component that owns your render loop: `REQUIRES cat_eyes`.
3. Render loop (full version with DMA buffers + HTTP hook: `examples/cat_eyes_task.c`):
```c
#include "cat_eyes.h"
cat_eyes_init(esp_random());
cat_eyes_set_style(CAT_STYLE_ANIME);
for (;;) {
    cat_eyes_tick(esp_timer_get_time() / 1000);
    for (int eye = 0; eye < 2; eye++) {
        int y0, y1;
        if (!cat_eyes_dirty_rows(eye, &y0, &y1)) continue;
        for (int y = y0; y < y1; y += 24) {
            int h = y1 - y < 24 ? y1 - y : 24;
            cat_eyes_render_strip(eye, y, h, dma_buf);          /* wait until dma_buf is free */
            esp_lcd_panel_draw_bitmap(panel[eye], 0, y, 240, y + h, dma_buf);
        }
    }
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(33));
}
```

## API cheat sheet
| Call | What |
|---|---|
| `cat_eyes_set_style(CAT_STYLE_HALLOWEEN)` | realistic, flat, line art, anime, cute, halloween, christmas |
| `cat_eyes_set_iris(CAT_IRIS_ODD)` | amber, green, blue, copper, violet, odd-eyed (themed styles ignore it) |
| `cat_eyes_play(CAT_ANIM_WINK)` | wink, slow_blink, roll, side_eye, hunt, startle, nictitate, sleepy, cross_eyed, happy, glance, **effects** (food, fly, paper_plane, bubble, spiral, slots, euro, fish, rain, snow, leaf, day_night, paw, spider, loading, pacman, matrix, disco, pumpkin), special |
| `cat_eyes_blink()` / `cat_eyes_look(gx, gy, ms)` | manual blink / hold gaze |
| `cat_eyes_set_auto(false)` | stop random idle (API-driven only) |
| `cat_eyes_*_from_name("...")` | string → enum for your JSON API |

Not thread-safe: call from the render task only; post commands from other tasks via a queue (see example).

## Add your own style
Add an enum value in `cat_eyes.h` and a row in `src/cat_eyes_styles.c` (lid shape, colours, pupil/iris/highlight kind, decoration flags). Check it on the PC with the host preview before flashing.

## Add your own animation
Add an enum value + name in `src/cat_eyes.c` (`k_anim_names`, `k_weights`) and a `case` in `src/cat_eyes_anim.c` using `K(eye, PARAM, t0, v0, t1, v1, ...)` keyframes. Params: GX, GY (gaze −1..1), UPPER, LOWER (lid 0 open..1 closed), PUPIL (0 slit..1 round), TILT, NICT (third eyelid), FX (style special), SMILE.

## Effects (`src/cat_eyes_fx.c`)
Effects are animations that draw shapes above the eye instead of (only) moving the lids and the iris. Each is one function
`fx_xxx(eye, t_ms, p, f)` that is called twice per eye and frame: with `p` to change gaze, lids and pupil, then with `f` to add
shapes (`ce_sh_circle / ellipse / capsule / tri / rect / ring / glyph`, layer `CE_L_FX`, at most `CE_MAX_FX` = 40 per eye) and an
optional whole-eye tint (`f->tint_shift`, `f->tint_add`). Both displays share one "world" x axis (eye 0 = 0..240, a 56 px gap,
eye 1 = 296..536), so a sprite can fly from one eye to the other.
The component works out the rows to push from the shape bounds, so nothing else needs to know about effects.

To add one: add an enum value between `CAT_ANIM_GLANCE` and `CAT_ANIM_SPECIAL` in `cat_eyes.h` (keep `SPECIAL` last, update
`CAT_ANIM_FX_FIRST/LAST`), a name and a weight in `src/cat_eyes.c`, a function plus its duration in `cat_eyes_fx.c`, and the name in
`firmware/components/eyes/core/cat.hpp` and `tools/mock_device.py`. A host test checks that the name tables match the component.

## PC preview (no hardware)
```
make -C host && ./host/preview out.rgb && python3 host/sheet.py out.rgb sheet.png   # needs Pillow
```
