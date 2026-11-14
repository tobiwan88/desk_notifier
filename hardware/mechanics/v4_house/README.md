# Housing v4 "House" (A-frame v4b)

The notifier as the mascot pet's house: a soft, reduced A-frame body whose front gable holds the round screen as the doorway, a 30 x 8 mm name-plaque frame below the door, and a separate lift-off roof cap in a second colour. 53.4 x 63.9 x 65.9 mm, three PETG parts (FACE, BODY, roof CAP), no screws.

- Source: `aframe_b.py` (manifold3d). Rebuild: `uv run --with manifold3d --with trimesh --with numpy --with matplotlib python -I aframe_b.py`.
- STLs: `stl/aframe_b_<part>_assembled.stl` and `_print.stl`.
- **Print files: `print/`** (`plate3_click_coupon.3mf` and `plate4_face_coupon.3mf` first, then `plate1_house.3mf` in the main colour and `plate2_roof.3mf` in the roof colour; settings in `print/PRINT.md`). Regenerate with `make_print_files.py`.
- Print rules: `DFM_V4.md` (its part numbers come from the first A-frame; v4b first layers are FACE 1804, BODY 550, roof 330 mm2, and BODY is the slender part: 62 mm tall, 5 mm brim) (roof print, roof click, plaque, colour alignment) on top of `../v3_moon/DFM_SPEC.md` (general PETG rules, slicer settings).
- Design notes: `concept_b.md`. Renders: `render_b_*.png` (`roofoff` and `explode` show the access).

Assembly: plug the display into J2, set the board in the BODY cradle, push FACE on (3 arms, about 15 N; J2 mates in the same stroke), press the roof CAP on (4 click fingers, about 8 N on, 10 N off by hand).

Checked by independent review (welded meshes): no overlap with the board parts or a full-glass display tab (only the intended 0.1 mm display crush ribs), window centred on the display (X 20.5, Z 33.7, 33.4 mm), parts do not overlap, roof lifts straight off (fingers rub for about 12 mm by design), print STLs watertight single shells, no mid-air starts, no visible overhang, roof first layer 330 mm2, air in low at the rear and the sensor corner, out high at the rear gable and the ridge. Tip angle 48.6 deg sideways with the display mass (52.3 deg with 12 g at the board); the optional 6.8 g steel plate in the floor recess brings it to about 51 deg. Design review 4.5/5.

Open before a full print:
- Print the roof click coupon first (plate 3).
- The display is held by friction ribs only; check it does not rattle.
- The 1.6 mm gable above the door is a cantilever; check it does not flex when the roof is pressed on.
