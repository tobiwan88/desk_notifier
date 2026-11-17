# Housing v3 "Moon" (C4c)

A disc around the round display on a low, pillowed plinth that holds the notifier-v1 board, like a small table clock: the disc and the plinth front form one face with an S-curve shoulder, an R30 saddle sweeps from the plinth up into the back of the disc, and the vents hide in the saddle and the floor. 53.4 x 63.9 x 56.2 mm, three PETG parts (face, plinth, back plate), clipped together without screws.

- Source: `moon4c.py` (manifold3d). Rebuild: `uv run --with manifold3d --with trimesh --with numpy --with matplotlib python -I moon4c.py` (writes `stl/` next to the script).
- STLs: `stl/moon4c_<part>_assembled.stl` (as on the desk) and `_print.stl` (print orientation).
- **Print files: `print/`** (`plate2_coupons.3mf` first, then `plate1_parts.3mf`; settings and order in `print/PRINT.md`). Regenerate with `make_print_files.py`.
- Print rules: `DFM_SPEC.md` (PETG, orientations, snap dimensions, coupons C1 to C8, slicer settings). Brim on the plinth.
- Design notes: `concept_c4.md` (written for C4; C4b raised the USB ceiling, C4c added the display-tab relief). Renders: `render4c_*.png`.
- Fusion: hub document `desk_notifier_v3_moon` holds these parts as mesh bodies (not parametric).

Checked by independent review: no overlap with the board parts (STEP heights: J2 top Z 7.76, ESP32 top Z 8.32) or with a display whose lower tab carries full-thickness glass, parts do not overlap, one watertight shell per part, no mid-air print starts, 2.0 mm over the ESP32, 1.2 mm over the USB-C receptacle, connected air path from the floor slots past the sensors and the ESP32 to the saddle, snap strain 1.19 %, plate section modulus at least 80 mm3 at the neck, tip angle at least 53 deg. Design review 4/5.

Open before a full print:
- Print the heat coupon and coupons C6/C7 first (USB fit, 21 N snap-in together with the J2 plug, tab zone).
- Untested on paper: snap feel, bridge sag under the plinth roof, BME688 response with the passive vent path.
