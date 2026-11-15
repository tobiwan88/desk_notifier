# Housing v4 "House" (A-frame, now v4c)

The notifier as the mascot pet's house: a soft, reduced A-frame body whose front gable holds the round screen as the doorway, a 30 x 8 mm name-plaque frame below the door, and a separate lift-off roof cap in a second colour. 56.4 x 63.9 x 66.0 mm, three PLA parts (PETG also works) (FACE, BODY, roof CAP), no screws.

- Source: `aframe_b.py` (manifold3d). Rebuild: `uv run --with manifold3d --with trimesh --with numpy --with matplotlib python -I aframe_b.py`.
- STLs: `stl/aframe_b_<part>_assembled.stl` and `_print.stl`; FACE press-fit variants `stl/aframe_b_face_rib010/030_print.stl`.
- **Real PCB: `pcb/`.** `export_pcb.py` exports the assembled notifier-v1 board from KiCad (`hardware/notifier-v1/notifier-v1.kicad_pcb`) to `pcb_board_frame.npz` (one convex hull per part, board frame) and `notifier_v1_pcb_board_frame.stl`; `aframe_b.py` places it in the housing (`PCB_X0`, `PCB_Y0`), takes the USB hole position from J1, checks overlap, gaps and a slide-in sweep, and writes `notifier_v1_pcb_housing_frame.stl` (load it next to the STLs in a slicer or viewer). `fit_views.py` draws `fit_real_pcb.png`. KiCad 10's bundled 3D library lacks the USB-C model: pass `--extra-3d DIR` with `Connector_USB.3dshapes/USB_C_Receptacle_HRO_TYPE-C-31-M-12.step` (e.g. from KiCad's royalblue54L_feather demo); the SHT40 has no model and is a box in `aframe_b.py`.
- **Print file: `print/housing_v4c.3mf`**, one plate with all three parts (FACE, BODY, roof); settings, assembly and checks in `print/PRINT.md`. Regenerate with `make_print_files.py`.
- Print rules: `DFM_V4.md` (its part numbers come from the first A-frame; BODY is the slender part: 62 mm tall, 5 mm brim) (roof print, roof click, plaque, colour alignment) on top of `../v3_moon/DFM_SPEC.md` (general print rules, slicer settings).
- Design notes: `concept_b.md`. Renders: `render_b_*.png` (v4b; the shape is the same, 3 mm narrower).

Assembly: plug the display into J2, slide the board into the BODY cradle from the front, push FACE on (crush ribs + 3 arms; J2 mates in the same stroke and pushes the board back), press the roof CAP on (4 click fingers, about 8 N on, 10 N off by hand).

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
