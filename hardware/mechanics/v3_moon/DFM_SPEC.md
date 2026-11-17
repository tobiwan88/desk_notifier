# DFM spec for the "Moon on a plinth" housing (FDM, 0.4 mm nozzle, 0.2 mm layers, Bambu class)

Basis: analysis of `C_moon/stl/moon_{face,plinth,back}_print.stl` with trimesh and manifold3d (scripts in this folder: `analyse.py`, `neck_snap.py`, `neck_opts.py`, `usb.py`). Rules are written so that they still apply after the tilt, the slimmer plinth and the vent are added. "Print frame" = Z up from the bed. In the current parts, print Z = assembly Y for FACE and PLINTH (front face on the bed), and print Z = assembly -Y for BACK.

## 0. What the current parts show (numbers)

| Part (print orientation) | Footprint x height | First layer | Overhang beyond 45 deg | Bridges | Thin features |
|---|---|---|---|---|---|
| FACE, front down | 53.0 x 56.0 x 16.15 | 1007 mm2, large and flat | 340 mm2 (195 visible: 65 mm2 neck setback ledge, about 100 mm2 steps of the 1 mm bed-edge rounds; 145 hidden) | neck ledge 4.8 mm (visible); lip underside 16.5 mm (see bug A) | tongues 1.0 mm, hooks 0.4 mm |
| PLINTH, front down | 53.0 x 12.4 x 55.8 | 153 mm2 (thin ring), slenderness 4.5:1 | 420 mm2, 0.4 visible (297 mm2 inside rear roof dome, shelf front ends 2 x 20 mm2) | rear roof 37 x 6.7 mm = 7.1 mm span, hidden | none below 0.8 mm (min wall 1.6); USB roof is a knife edge, see D |
| BACK, rear down | 38.3 x 43.5 x 1.85 | 1265 mm2 | 8.6 mm2 hidden | none | 3 spring beams 0.65 mm thick, pads 0.25 mm high |

Problems, in order of severity:

A. **FACE contains a detached shell.** The lip ring and the two snap tongues (581 mm3) are a separate body: `lip2d` is offset -0.15 mm inside the pocket wall, so it never touches the face block (measured gap 0.15 mm). A slicer prints it floating in mid-air (underside 130 mm2, 16.5 mm span). Fix: overlap the lip root with the wall by at least 1.0 mm (outer boundary at offset +0.3 over the first 0.6 mm), and add a root fillet R 1.0. Also the tongue top (Z 9.65) is flush with the lip top, which produced 6 non-manifold edges at (Y 10.4, Z 9.65); keep tongue and lip overlapped by at least 0.4 mm and avoid coplanar tops. After the fix the STL must report one shell and `is_watertight`.
B. **Snap tongues are vertical in print.** They run along print Z, so bending puts tension across the layers (the weakest direction). Strain at the hook is 1.7 % (t 1.0, L 4.7, deflection 0.25), above 1.5 %, and 2.4 % with a 0.1 tolerance. Pull-out with the 45 degree retention face is only 4 to 7 N per tongue, which is about 3.7 N at the disc centre (the disc releases if someone tips it forward).
C. **BACK spring beams** are 0.65 mm thick (less than 2 perimeters), carried at both ends over a 10.5 mm arc; strain is 3.5 % at the 0.5 mm interference. Beyond the PLA limit. The bump has square faces (no lead-in, no release), the pads are 1 layer high (0.25 mm).
D. **Neck is too weak for a knock.** At the slit (Z 11.4 to 12.45) the neck is an open U of 54.8 mm2 with S(fore-aft) = 22.8 mm3. A 10 N push at the disc top gives 18 MPa, 20 N gives 36 MPa (PLA breaks at 40 to 50 MPa in-plane, with notch and impact margin gone). Design allowance 8 MPa gives only 4.8 N at the disc top. Sideways it is fine (332 mm3).
E. **PLINTH is a tall thin hollow tube on the bed**: 153 mm2 first layer for a 55.8 mm tall part (4.5:1). Needs a brim and slow outer walls. Rear plan corners R 10.5 become a dome at the print top: terracing on the last 5 mm of each corner. USB plug pocket cuts the rounded rear shoulder: roof above the pocket is 1.0 mm at Y 61.6 and goes to zero at Y 61.8 (knife edge, chips in handling).
F. **Neck setback ledge (visible)**: the neck front is 1.5 mm behind the disc face, so a 26 x 5.6 mm flat face hangs 1.5 mm above the bed and must be bridged (4.8 mm span). It prints, but it is rough and visible in the shadow slit.
G. **Zero clearances**: the board slot is shelf Z 3.60 to rib Z 5.17 = 1.57 mm for a 1.51 mm board (0.06 mm). Not printable, see section 5. Ribs are 0.10 mm from the board edge in X.
H. Window 33.0 against active area 32.4: 0.3 mm per side, and an FDM hole prints 0.1 to 0.2 mm small. The module floats 0.25 mm in its pocket, so pixels can clip.

## 1. Material

1. **Use PETG for all three parts** (matte or HF PETG in warm grey). Reasons: (a) the ESP32 module and the buck converter sit 1.5 mm under the plinth ceiling in a closed shell; expect 40 to 50 degC at the ceiling in Wi-Fi use, close to PLA's glass transition (about 58 degC, heat deflection about 55 degC). Snaps under constant preload creep and relax in PLA at that temperature. PETG: Tg about 80 degC. (b) PETG has about 4 % elongation and much better layer adhesion, so snaps survive more cycles; PLA snaps are brittle and break in a few cycles. (c) No desk sun, so PETG's UV weakness does not matter.
2. **PLA is allowed for fit prints and for the first look prototype**, and for the final only if a heat test shows the plinth ceiling stays below 40 degC (coupon C8). If PLA is used, take the PLA strain limits below and add 0.05 mm clearance to sliding fits.
3. Do not mix materials between parts: the brief asks for one colour, and two "warm greys" from two filament lines will not match.
4. Dry the filament (PETG 65 degC 6 h, PLA 50 degC 6 h); wet PETG strings and shows blobs on the hero face.

## 2. Colour and finish (calm matte)

5. Warm grey or off-white, **matte** filament; avoid glossy and silk. Dark colours show layer lines and fingerprints more; light grey hides them.
6. **Print the disc face on textured PEI** (face down on the bed). The bed-facing side gets a uniform fine grain, which is the matte hero surface. Smooth PEI gives a mirror gloss that shows every fingerprint. Textured PEI at 70 degC for PETG (glue stick not needed), 55 degC for PLA.
7. **Ironing: no.** The hero face is the bed side (cannot be ironed and does not need to be). The only top surfaces are hidden. Ironing on PETG leaves shiny streaks.
8. **Seam**: aligned, rear (Bambu "Aligned back"; print rear = assembly bottom for FACE and PLINTH because print Y = -assembly Z). Use scarf seam if the slicer offers it. The seam then runs along the underside of the plinth and the bottom of the disc, never on the top or the face. Check after the slicer preview: no seam on the R4.5 top edge.
9. Layer lines on the plinth run around the section (like contour lines of a vertical extrusion): this is the best possible pattern on the long visible top. Do not add anything that breaks that extrusion on the outside (no bumps, vents with horizontal axes on the top).
10. Light sanding (400 grit, wet) only on the 1 mm bed-edge round if needed; do not sand the textured face.
11. Do not paint or clear-coat; one material, honest surface.

## 3. Print orientation (per part, and the rule behind it)

12. **FACE: front face (window side) on the bed.** Layers are parallel to the face, the window ring is perfectly flat, the disc and block are vertical extrusions (smooth), and the neck, pocket and lip need no supports. Keep this orientation after the disc is tilted: put the disc front plane on the bed; the plinth block then leans by the tilt angle, which is an overhang of only that angle (20 degrees or less is fine; above 45 degrees from vertical add a 45 degree chamfer).
13. **PLINTH: open front (seam side) on the bed**, standing on its edge. The cavity is open at the bed, so no supports and no roof bridging except the small hidden rear roof (7 mm). For a slimmer asymmetric plinth keep one rule: **the open end of any hollow shell goes on the bed**. If the vent adds a closed top, keep bridges at most 8 mm.
14. **BACK: rear face down, flat.** All spring arms lie in the layer plane (stress along the filaments), which is the strongest direction.
15. **Snaps: the long axis of every spring beam lies in the print plane** (perpendicular to print Z), and the deflection direction also lies in the print plane. Then strain runs along the filaments. A beam standing vertical (like the current tongues) is stressed across layers: halve the strain limit (table in section 6).
16. No supports anywhere (set support off). Anything that needs support gets a 45 degree chamfer, a teardrop, or an extra seam.
17. Whenever a face points down in print and is visible: at most 45 degrees from vertical; 0.5 mm steps at 90 degrees are the limit for hidden hooks (coupon C2 confirms).

## 4. Wall, fillet and chamfer rules

18. Wall thicknesses are multiples of the 0.42 mm line width: **1.26 / 1.68 / 2.1**. Use 1.68 for shell walls (4 perimeters), 2.1 at snaps. Nothing structural below 1.2 mm; **no feature below 0.8 mm anywhere** (the current tongues at 1.0 and the 0.65 mm back beams violate this for load-bearing use).
19. Front wall behind the window: 1.2 mm is enough (3 perimeters) and gives a shallower look into the glass; keep the 0.25 mm gap to the glass.
20. **Bed-side edge: 0.6 mm x 45 degree chamfer**, not the 1 mm round now modelled (the stepped round gives first-layer steps of 0.28 mm, about 54 degrees, rough on the hero face). The chamfer also hides elephant foot (0.1 to 0.2 mm). Set slicer "elephant foot compensation 0.10".
21. Edges that are print-vertical (plinth top R4.5, disc rim): any radius, they print as smooth extrusions. Keep R4.5.
22. Edges that end at the print top or sweep through the print plane (plinth rear corners R10.5, the BACK 1 mm rear round): the surface goes flat toward the top and terraces. Either cut the rear plan radius to 3 mm or less, or accept visible terracing and use variable layer height 0.10 mm for the last 6 mm. Prefer a 1 x 45 degree chamfer to a small round at print top.
23. Minimum radii that print cleanly: convex vertical corners at least 0.8 mm (two line widths); concave corners at least 0.5 mm; fillets at the root of a snap or neck at least 1.0 mm (reduces notch stress by about 30 %).
24. Slots and vents: at least 1.2 mm wide, at least 1.8 mm for sensor vents. Slots whose axis is the print Z (vertical) need nothing special (current floor slots are such slots: fine). Slots or holes with a horizontal axis in a vertical wall: use a teardrop or 45 degree top, or make them a short 1.0 mm bridge. A louvre or side vent with horizontal slats: slats at 45 degrees, pitch at least 3 mm.
25. No horizontal flat visible face that hangs above the bed (like the neck setback ledge). Replace the 1.5 mm setback with a 45 degree ramp from the disc face to the neck, or set the neck flush with the face. A flat bridge on a visible face always shows ropes.

## 5. Tolerances, pockets, fits (Bambu class, 0.4 nozzle; measure with coupon C1 first, these are the defaults)

Printed sizes: outside dimensions come out 0.00 to -0.10 mm, holes and pockets 0.10 to 0.20 mm small. Features in the layer plane are more accurate than features along Z.

26. Display pocket: 38.0 mm module circle, pocket **38.5** (0.25 per side) in the print plane. Tab 23.0, pocket **23.5**. Pocket depth in front: 0.25 to the glass. Keep as in the brief (they are correct for this process). Check that the pocket does not print undersize: accept 38.35 to 38.55 measured.
27. **Window: 33.4 mm** (the nominal 33 prints 32.8 to 32.9). Keep the 0.5 chamfer; minimum 0.5 mm margin to the active area (32.4) on each side after a 0.25 mm module float.
28. **Board slot** (shelf to rib, PCB 1.6 +/- 0.16, brief says 1.51 measured): slot height = measured PCB maximum + 0.20, so about **1.9 mm** (not 1.57). Make the hold-down ribs 0.4 mm crush ribs (triangular), not flat 0.05 mm gaps; board stays tight without a gap. Rib edge at least 0.3 mm from the board edge in X, with a 45 degree lead-in.
29. Fit table (per side, 0.2 layers): **press fit** (peg into hole): hole = peg - 0.05 total diameter, with a 0.4 mm lead-in chamfer; **sliding fit** 0.20 per side; **loose / visible gap** 0.30; **push-on lip (FACE lip into PLINTH)** 0.20 per side, current 0.15 is too tight for 4 perimeter walls (use 0.20, make the lip 1.26 thick).
30. **USB-C opening: 9.6 x 3.8 as in the brief becomes 10.0 x 4.2** (receptacle 9 x 3.3, shell 8.94 x 3.26). Plug pocket 12.6 x 7.6 becomes **12.8 x 7.8**. Do not cut the plug pocket into a rounded shoulder: the pocket roof goes to zero thickness. Either flatten the rear face locally (a 14 x 9 mm flat spot, R1 into the round), or make the plug exit through a vertical flat; minimum 1.2 mm of material above the pocket everywhere.
31. Stacked gaps in print Z must be a multiple of 0.2 and at least 0.3 mm (the 0.8 mm skid gap and the 1.05 mm slit are fine as shapes; they are open in the layer plane).
32. Ceiling over the ESP shield: at least **2.0 mm** gap to the shield (now 1.5 mm) and an air path; this also limits ceiling temperature.

## 6. Snap design with numbers

Formula: strain = 1.5 t d / L^2 (end load, rectangular beam), force W = 3 E I d / L^3, with E = 2500 N/mm2 (PETG 2100, PLA 3000). d = undercut + 0.10 tolerance. Pull-out force = W tan(retention - friction angle), friction angle 17 degrees (mu 0.3), insertion = W tan(lead-in + 17).

Strain limits (few cycles): **PLA 1.5 % in plane, 0.7 % across layers; PETG 2.5 % in plane, 1.5 % across layers.**

Current state:

| Feature | t, L, d | Strain | Verdict |
|---|---|---|---|
| FACE tongue (vertical in print) | 1.0, 4.7, 0.25 | 1.70 % (2.4 % with 0.1 tolerance) | Fail for PLA across layers (0.7 %), marginal for PETG. Pull-out 4.5 to 7 N, 45/45 degree. |
| BACK beam (arc, both ends fixed) | 0.65, 10.5, 0.5 | 3.5 % | Fail. Thin beam, no lead-in. |

Proposed snaps:

33. **Housing-to-plinth snaps (2 or 3 per side): in-plane arm.** Beam long axis along assembly X (print plane), deflection in assembly Z (print plane), hook on the arm tip. Dimensions: **t 1.26 (3 perimeters), width (depth along print Z) 6.0, free length L 9.0, undercut 0.5, d 0.6, root fillet R1.0**. Strain 1.3 % (PLA 1.5 % in plane limit: pass; PETG 2.5 %: comfortable). Force W 6 N (PETG), 8.5 N (PLA). **Lead-in 30 degrees, retention face 60 degrees** (pull-out 5.6 N PETG, 8 N PLA per arm). With 3 arms: about 17 N total, which holds the disc against a tilt of about 7 N at the disc centre. Retention 70 degrees gives 8 / 11 N per arm if the face points up in print or if the undercut is at most 0.5 mm (print check C2).
34. **If vertical tongues must stay**: t 1.26, width 6.0, **L at least 10**, undercut 0.4, d 0.5: strain 1.1 % (PETG across layers 1.5 %: pass; PLA 0.7 %: fail, use in-plane arms). Pull-out per tongue 3.7 N (60 degree retention). Use **4 tongues** (two per side, 25 mm apart in X). The tongue length is available: from the seam at Y 6.8 the board has room to Y 25 before the ESP shield (keep 0.5 mm above the shield, Z 8.8).
35. Hook geometry: undercut 0.4 to 0.5 mm, hook length along the beam 1.5 mm, lead-in 30 degrees (long shallow ramp, no 45 degree), retention 60 degrees, **0.1 mm clearance in the groove** (groove 0.2 longer than hook, 0.1 deeper than undercut), groove walls at least 1.2 mm thick. A flat 0.3 mm land between the two ramps carries load without creep.
36. Beam root: never a sharp inside corner: fillet R at least 1.0 or taper the beam from t to 1.4 t. Beams must **not** share a wall thinner than 1.26 with another snap.
37. **BACK plate retention: replace the three 0.65 mm arc beams with three tangential in-plane arms**: radial thickness **1.26**, arc length **12**, free end with bump **0.5 high x 1.2 long**, **lead-in 30 degrees on the insertion side, retention 60 degrees**, clearance slot behind the arm 1.4 mm wide (deflection 0.6 plus 0.8). Strain 0.75 % at L 12, d 0.6: passes for every limit. Insertion force about 1.3 N per arm (PETG): feels like a light click. Pads: **0.4 mm high** (2 layers) not 0.25.
38. The groove in the disc cavity wall (0.7 deep) is a hidden 90 degree overhang of 0.7 mm: use a 45 degree top face on the groove (the groove then prints without sag).
39. Test every snap for 20 insert/remove cycles (coupon C2) and look for white stress lines; no white lines is pass.
40. Keep the J2 header plug stroke in mind: the display plugs into J2 in the same stroke as the face snaps. Plan 10 to 15 N insertion for the header; design the snap insertion force (sum of arms) below 15 N and use a stop (the seam) so the header is never the only retention.

## 7. Neck: disc-to-plinth joint under a knock

Load path: a knock at the disc top (Z about 53, lever 41 mm from the slit) produces stress along Z in the neck. In FACE print orientation, the neck walls are extruded along print Z, and the stress along assembly Z lies in the layer plane: this is the strong direction. Keep this: **never print the neck so the stress is across layers**.

Current section: open U, 26.7 wide x 5.55 deep, walls 1.6, area 54.8 mm2, S(fore-aft) 22.8 mm3. A 10 N push gives 18 MPa. The neck will survive a finger push, not a desk knock.

41. **Design load: 15 N at the disc top at 8 MPa design stress** (safety about 5 against PLA/PETG in-plane strength including notch and impact). Required **S(fore-aft) at least 77 mm3**, round up to **80 mm3** (3.5 x the current), and keep sideways S at least 150 mm3.
42. Section options (computed, `neck_opts.py`):
    - 30 x 8 U, front wall 2.0, sides 2.4, open at the rear: A 89 mm2, S 73 mm3 (14 N at the disc top).
    - 26.7 x 8 U, front 2.0, sides 2.4: S 72 mm3 (14 N).
    - Same 26.7 x 5.55 U **closed with a 1.6 mm rear wall** (the BACK plate keyed into the walls and glued, or a printed rear web): S 128 mm3 (25 N). The module tab must still be inserted from the rear, so this needs the BACK plate to be a keyed insert with 0.1 mm clearance and a dab of cyanoacrylate, or a front-loaded module.
    Depth in assembly Y is the strongest lever (S grows with depth squared): give the neck at least 8 mm depth (disc thickness 7.05 becomes about 9) or make the side walls 2.4 and 8 deep.
43. **Fillet the neck**: R at least 2.0 at both the plinth top and the disc bottom, so the 1.05 mm slit becomes a shadow visible only from the front and the section does not step. A stress riser at the sharp corner doubles the local stress.
44. Add a **mechanical stop for the disc**: two 1.2 mm bosses (or the seam face itself) so a backward push (+Y) closes the seam in compression, and a forward pull (-Y) is held by at least 3 snaps (section 6). Without that, forward tilt releases the hooks at about 4 N at the disc centre.
45. After the disc is tilted back by an angle, the knock load is perpendicular to the face and the arm to the neck is the same; the section calculation uses the lever along the disc axis. Re-run `neck_snap.py` with the new geometry; the rule S at least 80 mm3 stays.
46. Test (coupon C3): clamp the plinth block, pull a luggage scale at the disc top (41 mm lever). Pass: at least 25 N before cracking, no cracking at 15 N, no visible creep after 1 minute at 15 N.

## 8. Test-coupon list (print in this order, each under about 40 minutes)

Print in the final material (PETG), on textured PEI, in the **same orientation** as the real part. Record all measurements in a table next to the model.

| # | Coupon | Contents | Measure |
|---|---|---|---|
| C1 | Fit plate, 60 x 40 x 3, flat | holes 38.3 / 38.4 / 38.5 / 38.6 / 38.7 (a 6 mm deep ring, 1.6 wall), tab slots 23.3 to 23.7, slots 1.7 / 1.8 / 1.9 / 2.0 for the PCB, peg and hole pairs at 0.05 / 0.10 / 0.15 / 0.20 / 0.30 | the real display module in the rings, the real PCB in the slots (hand feel: slides, rubs, tight); caliper the hole diameters; choose compensation |
| C2 | Snap coupon, printed standing and flat | in-plane arm (t 1.26, L 9, hook 0.5, retention 45 / 60 / 70) with its mating groove; vertical tongue (t 1.26, L 10, hook 0.4) with groove; 3 BACK arms in a 40 mm disc | 20 cycles each; insertion and pull-out force with a kitchen scale; look for white stress lines and cracks; check the 0.5 mm hook overhang printed clean |
| C3 | Neck coupon | disc bottom arc + neck (final section) + plinth top block, 20 mm of plinth, in print orientation | load the disc top with a luggage scale at a 41 mm lever: crack force, 15 N hold for 60 s |
| C4 | Face finish coupon | 45 mm disc, 1.2 mm front wall, 33.4 window with 0.5 chamfer, bed-edge 0.6 chamfer vs 1 mm round, textured vs smooth PEI | window diameter, look at the grain, elephant foot, window edge in a raking light |
| C5 | Vent and slot coupon | floor slots 1.2 / 1.5 / 1.8 wide, teardrop holes 2.0 / 3.0, a 7 mm and a 10 mm roof bridge | clean-through width, bridge sag, stringing |
| C6 | USB coupon | rear wall piece with the 10.0 x 4.2 hole and the flat plug pocket 12.8 x 7.8 | real cable with its overmould; wall thickness above the pocket at least 1.2 |
| C7 | Plinth segment | 53.4 x 17.2 x 20 mm slice (as generated for C4c) of the plinth with shelves, ribs and the board slot at 1.9 | slide the real PCB in; hold-down feel; wall stiffness; brim need |
| C8 | Heat test | the real board in a plinth printed from the final material, ESP in Wi-Fi mode 2 hours, thermocouple on the ceiling above the shield | steady temperature; if below 40 degC PLA is allowed, else PETG |

## 9. Slicer settings (Bambu Studio, PETG; PLA values in brackets)

| Setting | Value |
|---|---|
| Layer height | 0.20 mm (variable 0.12 for the first 1.2 mm of FACE and PLINTH for the chamfer/elephant foot zone) |
| Line width | 0.42 mm outer wall, 0.45 inner |
| Walls | **4 perimeters (1.68 mm)** for FACE and PLINTH, 3 for BACK and snap arms; "ensure vertical shell thickness" on |
| Wall order | inner / outer (outer last for the surface), "precise wall" on |
| Top / bottom shell | 4 layers (0.8 mm) |
| Infill | 15 % gyroid (most volumes are pure walls); 100 % in snap arms through modifier "walls 4+" |
| Seam | aligned, rear (assembly bottom), scarf seam if available; seam gap off for the face |
| Ironing | off |
| First layer | 0.2 mm, 0.45 mm line, 70 degC bed [55 degC] on textured PEI, 25 mm/s |
| Elephant foot compensation | 0.10 mm |
| Cooling | PETG: fan 60 % after layer 3, 100 % on bridges and overhangs; [PLA: 100 % after layer 3] |
| Speed | outer wall 60 mm/s, inner 100, bridges 30; acceleration 2000 mm/s2 for the tall PLINTH (avoids wobble at 56 mm) |
| Brim | outer brim 5 mm, gap 0.1 on the PLINTH (thin ring first layer); 3 mm on FACE; none on BACK |
| Supports | off |
| Bridge | flow 0.95, thick bridges off, internal bridges "auto" |
| Nozzle temperature | PETG 245 degC [PLA 215] |
| Arc fitting | on |

Check list before printing a new revision: one watertight shell per part; first layer area FACE at least 800 mm2; no overhang beyond 45 degrees on visible faces; bridges at most 8 mm; no feature below 0.8 mm; hook strain by the formulas above; neck S at least 80 mm3; tolerances from section 5 and coupon C1 results written into the parameters.
