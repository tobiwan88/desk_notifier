# Housing v4 "House" (A-frame, now v4d)

The notifier as the mascot pet's house: a soft, reduced A-frame body whose front gable holds the round screen as the doorway, a 30 x 8 mm name-plaque frame below the door, and a separate lift-off roof cap in a second colour. 56.4 x 60.7 x 66.3 mm, three PLA parts (PETG also works) (FACE, BODY, roof CAP), no screws.

- Source: `aframe_b.py` (manifold3d). Rebuild: `uv run --with manifold3d --with trimesh --with numpy --with matplotlib python -I aframe_b.py`.
- STLs: `stl/aframe_b_<part>_assembled.stl` and `_print.stl`; FACE press-fit variants `stl/aframe_b_face_rib010/030_print.stl`.
- **Real PCB: `pcb/`.** `export_pcb.py` exports the assembled notifier-v1 board from KiCad (`hardware/notifier-v1/notifier-v1.kicad_pcb`) to `pcb_board_frame.npz` (one convex hull per part, board frame) and `notifier_v1_pcb_board_frame.stl`; `aframe_b.py` places it in the housing (`PCB_X0`, `PCB_Y0`), takes the USB hole position from J1, checks overlap, gaps and a slide-in sweep, and writes `notifier_v1_pcb_housing_frame.stl` (load it next to the STLs in a slicer or viewer). `fit_views.py` draws `fit_real_pcb.png`. KiCad 10's bundled 3D library lacks the USB-C model: pass `--extra-3d DIR` with `Connector_USB.3dshapes/USB_C_Receptacle_HRO_TYPE-C-31-M-12.step` (e.g. from KiCad's royalblue54L_feather demo); the SHT40 has no model and is a box in `aframe_b.py`.
- **Print file: `print/housing_v4d.3mf`**, one plate with all three parts (FACE, BODY, roof); settings, assembly and checks in `print/PRINT.md`. Regenerate with `make_print_files.py`.
- Print rules: `DFM_V4.md` (its part numbers come from the first A-frame; BODY is the slender part: 59 mm tall, 5 mm brim) (roof print, roof click, plaque, colour alignment) on top of `../v3_moon/DFM_SPEC.md` (general print rules, slicer settings).
- Design notes: `concept_b.md`. Renders: `render_b_*.png` (v4b; the shape is the same, 3 mm narrower).

Assembly: plug the display into J2, slide the board into the BODY cradle from the front, push FACE on (crush ribs + 3 arms; J2 mates in the same stroke and pushes the board back), press the roof CAP on (4 click fingers, about 8 N on, 10 N off by hand).

## v4d changes (after the v4c print, 2026-11-15)

The v4c print: board and FACE fit, but the screen was tight in its pocket, sat about 6 mm behind the front and leaned back. Cause, from the seller's drawing of the display module (1.28" GC9A01, 38 x 45.5 mm PCB): the display hangs on J2, and the model had it wrong. It had no 2.54 mm black header spacer (the spacer bottoms on the J2 face) and put the pin row 2.5 mm above the J2 pins. So the real screen sat 3.2 mm behind and 2.5 mm under its pocket, missed it and tipped back on the pins.

| | v4c | v4d |
|---|---|---|
| Display model | glass + PCB 3.1, pins straight from the PCB back, pin row at Z 8.96 but J2 pins at Z 6.48 | drawing: glass front to PCB back 3.1 (3.0 nominal), header spacer 2.54, pin row 1.76 above the PCB bottom edge |
| Board position | shelf Z 3.60, rear edge Y 60.46 | **shelf Z 6.08 (+2.48), rear edge Y 57.27 (-3.19)**: computed from the display (`Z_SHELF`, `PCB_Y0`, asserted against the KiCad J2 model) |
| Glass front to housing front | 5.0 (real), 1.85 (model) | 1.85 |
| Display pocket | 38.5 (0.25 per side), ribs 0.10 into the PCB | **39.0 (0.5 per side)**, ribs 0.05 into the PCB; tab pocket 24.0 |
| Body depth | 63.9 | 60.7 (rear wall, cradle, ramps, side vents moved forward 3.19) |
| Vents | rear intakes Z 7.6-14.0, side Z 7.8/10.9/14.0 | rear Z 9.6-14.6, side Z 9.0/11.8/14.6 (board higher; the split plane is at Z 16 at the rear) |

| Board support | 2 mm edge ledges, left hold-down full length, right hold-down only behind the ESP32 module | same, plus **2 stilts** (floor rails 1.2 wide at X 12 and 29, Y 19 to 52/57, top 0.05 under the board; 45 deg ends, vertical in print) so the board, now 4.7 mm above the floor, cannot bow |

Cradle: the edge ledges carry the board, the left jaw holds its left edge along the whole slide, the right jaw only behind the ESP32 module (the antenna overhangs the right edge and sweeps the space in front of it during slide-in, so no hold-down fits there), and both jaws come down to 0.3 mm over the last 6 mm (USB end). The stilts start behind the J2 tails. The cradle still starts behind the FACE arms (Y 11.2), so the board's front 3.9 mm with J2 overhang the shelf; with the display in its pocket that end is held by the display. Model checks unchanged: no overlap, slide-in clear, smallest gaps C5 0.45, U1 0.74, U4 0.95; USB hole follows J1 (Z 9.31); antenna gap 2.17; tip angle sideways 49.6 deg with the display. `pcb/fit_views.py` redrawn.

## v4c changes (after the first v4b print, 2026-11-15)

The printed v4b showed: the FACE fell out, the USB-C hole missed the connector, and the board slid in too tight. Checked against the real board:

| | v4b | v4c |
|---|---|---|
| USB hole | centred (X 20.85), 12.8 x 7.8; J1 hit the rear wall | on J1 (X 14.85, Z 6.83, from the KiCad model), 13.2 x 8.0 + 0.8 funnel |
| Board slot (shelf to jaw) | 1.9, crush bump down to 1.52 (a 1.6 board was 0.08 too thick) | 2.6 along the slide, ramps to 1.9 over the last 6 mm at the rear (locates the USB end), no bump |
| Board side clearance | 0.3 / 0.4 | 0.6 / 0.6 |
| Antenna to wall | 0.67 | 2.17 (body +1.5 per side, 53.4 -> 56.4 wide) |
| Board rear edge to rear wall | 0.15 (and J1 0.1 into the wall): the FACE could not seat, so the 0.4 mm hooks never latched | 0.5 float (rear wall 2.64 thick, outer face unchanged) |
| FACE hold | 3 snap arms; registration ring 0.2 loose | ring with 6 crush ribs (1.0 wide, 0.20 interference for PLA, 45 deg start, lead-in), plus the arms |
| Vents | rear intakes left and right of a centred plug; side vent in the left wall | the sensors (BME688, SHT40) sit at the rear right: 5 rear intakes right of the plug, side vent in the right wall |

Model checks (`aframe_b.py` output): no overlap of any board part with the housing (board at 1.76 worst case); slide-in sweep clear; smallest gaps C5 0.45, J2 0.50, U1 0.74 (to the cradle wall behind it), U4 0.95; J1 face 2.89 inside the rear face (the plug overmold enters the hole); display window unchanged; roof lift-off unchanged; print STLs watertight single shells, no mid-air starts. Tip angle sideways 49.7 deg with the display mass (53.3 deg with 12 g at the board), 52.8 deg with the steel plate.

Board size: KiCad says 50.0 x 36.0 x 1.6 (USB-C and J2 stick out 0.25, the antenna 6.0); a caliper measurement of the bare board is still open. The 0.6 side clearance takes a board up to 37.0 wide.

Open: print the plate and report board slide, USB fit, FACE hold and roof click (see `print/PRINT.md`); the 1.6 mm gable above the door is a cantilever, check it does not flex when the roof is pressed on.
