# Handover prompt: integrate `cat_eyes` into the ESP eyes project

Paste everything below the line into your coding agent (for example Claude Code), opened in the root of the ESP-IDF project. Put the unzipped `cat_eyes/` folder in `components/` first.

---

You are integrating a ready-made ESP-IDF component, `components/cat_eyes`, into this project. It draws animated cat eyes on two 240×240 round GC9A01 displays, one display per eye. The component is already written and tested on a PC. Your job is to wire it in, verify it on the hardware, and tune it. Do not rewrite the renderer.

## Project context
- MCU: ESP32-C61 (single-core RISC-V at 160 MHz, **assumed no FPU**; verify this). The firmware uses ESP-IDF, not Arduino.
- Displays: 2 × GC9A01 240×240 RGB565 over SPI. The plan is one SPI bus shared by both panels, each with its own CS pin, at 40 MHz. Eye 0 is the display on the viewer's left and eye 1 is on the viewer's right.
- The architecture already planned for the project: a render task at about 30 fps that draws strips with DMA and dirty rects, an HTTP API (`POST /api/eye` taking JSON) whose handlers only push commands onto a queue, mDNS, and a token. Follow whatever the repo actually contains. If a compositor or "source" abstraction exists, add cat_eyes as a source instead of a separate task.

## What the component provides (`components/cat_eyes/include/cat_eyes.h`)
- `cat_eyes_init(seed)`, `cat_eyes_tick(now_ms)` once per frame, `cat_eyes_dirty_rows(eye,&y0,&y1)`, and `cat_eyes_render_strip(eye,y0,h,buf)`. The strip call fills full 240-px-wide RGB565 rows, already byte-swapped for GC9A01 unless you call `set_swap_bytes(false)`.
- Styles: realistic, flat, line art, anime, cute, halloween (demon), christmas.
- Iris colours: amber, green, blue, copper, violet, odd-eyed.
- Animations: wink, slow_blink, roll, side_eye, hunt, startle, nictitate (third eyelid), sleepy, cross_eyed, happy, glance, and a style-specific special: sparkle, heart_eyes, demon_flare or twinkle.
- Random idle runs by default. It does saccades, blinks (sometimes a double blink) and weighted random animations, never repeating the same one twice in a row.
- Name helpers for the HTTP layer: `cat_eyes_style_from_name`, `_iris_from_name`, `_anim_from_name`.
- **Not thread-safe:** call every function from the render task only.
- The core is plain C with no IDF includes. `host/` builds a PC preview: `make -C host && ./host/preview out.rgb && python3 host/sheet.py out.rgb sheet.png`.

## Tasks
1. **Build:** add `cat_eyes` to `REQUIRES` (or `PRIV_REQUIRES`) of the component that owns the render loop, then run `idf.py build`. Fix only integration issues; report anything inside the component instead of silently changing behaviour.
2. **Render loop:** use `examples/cat_eyes_task.c` as the reference.
   - Use 3 DMA-capable strip buffers of 24 rows each and a counting semaphore given from `on_color_trans_done`. A buffer is never reused while it's still on the wire.
   - Each frame: drain the command queue, run `cat_eyes_tick`, then for each eye push only `dirty_rows` in strips through `esp_lcd_panel_draw_bitmap`.
   - If the repo already has a render task or compositor, merge into it rather than adding a second task that touches the displays.
3. **HTTP:** extend the existing `POST /api/eye` handler. Use the `cat_eyes_handle_json` snippet at the bottom of the example: style, iris, play, auto, and `look:[gx,gy,hold_ms]`. Handlers must only post to the queue.
4. **Default style by date (optional):** once SNTP has synced, choose halloween in October, christmas in December, and the configured default otherwise. An explicit API style overrides this until reboot.
5. **Verify on hardware:**
   - Both eyes animate, mirrored correctly, with the outer corners pointing outward and the inner corners toward the nose. If they are swapped, swap the panel order and do not change the renderer.
   - Colours are correct (amber looks amber). If red and blue are swapped, set the panel's RGB/BGR order. If colours look like noise, the byte swap is wrong.
   - The example task logs the average frame time every 5 s. **Target: under 33 ms per frame with Wi-Fi active.** Report the numbers for realistic, anime, halloween and christmas. Halloween and christmas redraw the full screen every frame because of the bats and snow, so they are the worst case.
   - Leave it running with random idle for 30 minutes. Check for no watchdog resets and no stalls during HTTP requests.
6. **If too slow, in this order:**
   1. Lower the frame rate only for halloween and christmas: `FRAME_MS` to 50.
   2. Raise the SPI clock to 80 MHz. Earlier notes flag a strapping-pin concern for 80 MHz, so check the pin before doing this.
   3. Render only the dirty x-range. This needs a column-limited `render_strip` variant; ask first.
   4. Move snow and bats to the dirty rows of only the eye region.
   Do not convert the per-frame float code to fixed point unless profiling shows it matters. Per-pixel code is already integer-only.

## Assumptions to check, and report back what you found
- C61 has no hardware FPU (the design assumes this; floats are only used per frame).
- The GC9A01 driver version in the repo (for example `espressif/esp_lcd_gc9a01`) supports `on_color_trans_done` in the panel IO config.
- Free internal DMA RAM is enough for 3 × 11.5 KB buffers next to Wi-Fi.
- Mirroring and colour order look right on the real modules.

## Deliverables
- A working build and a short report: the frame times per style, what you changed, and any issues inside the component, with file and line.
- Do not commit generated images. Keep commits small: integration, HTTP, then optional date logic.
