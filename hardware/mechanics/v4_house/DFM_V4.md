# DFM_V4: A-frame and Kennel pet houses (PETG, 0.4 mm nozzle, 0.2 mm layers, Bambu class)

Basis: the print STLs of `v4_aframe/stl/aframe_{face,body}_print.stl` and `v4_kennel/{body,back,roof}_print.stl`, analysed with the scripts in this folder (`common.py`, `health.py`, `health2.py`, `analyse.py`, `thin.py`, `grooves.py`, `roofsec.py`, `roof_orient.py`, `hookfaces.py`, `snaps.py`; run with `./run.sh <script> [args]`). Rules are those of `DFM/DFM_SPEC.md`. Slicing was emulated with 0.2 mm layers cut at mid-layer height; "overhang" means a face more than 45 deg from vertical, "visible" means a ray along the face normal escapes the assembled house. Print frame: Z up from the bed. Assembled-to-print transforms were recovered by rigid fit (exact, cost below 1e-11).

## 0. Verdict

The A-frame is the cleaner print: both STLs are watertight single shells, no mid-air starts, no bridges, no visible overhang, no feature under 0.8 mm. Its open issues are function and handling (loose display, insertion force, side tip 46 deg, rear-only vents), not printability. The Kennel has a broken body STL, a roof that prints as a 44 mm tall 1.7 mm wall with its retention hooks at the worst place, grooves that mostly disappear, and press-fit pins that are not press fits. Recommendation: continue with the A-frame; use the Kennel only for ideas (plank lines done properly, name plaque).

## 1. Print orientation numbers (print frame, 0.2 mm layers)

| Part | Footprint x height (mm) | Height / sqrt(first layer) | First layer | Overhang > 45 deg (visible / hidden) | Bridges | Mid-air starts | Shells, watertight |
|---|---|---|---|---|---|---|---|
| A-frame FACE (front down) | 53.4 x 64.6 x 12.0 | 0.28 | 1829 mm2 | 0 / 43.9 mm2 (40 at 45-60 deg, 3.9 at 60 deg) | none | 0 | 1, yes |
| A-frame BODY (rear down) | 53.4 x 63.4 x 55.9 | 1.40 (h / short side 1.05) | 1591 mm2 | 0 / 3.8 mm2 | none | 0 | 1, yes |
| Kennel BODY (front down) | 53.1 x 61.2 x 64.0 | 1.77 (h / short side 1.21) | 1314 mm2 | 36 (all exactly 45 deg: bed chamfer and groove flanks) / 72 mm2 | 4 x 1.7-1.8 mm, hidden | 0 | NO (see K1) |
| Kennel BACK (back down) | 50.5 x 58.2 x 1.7 | 0.04 | 1954 mm2 | 1.9 / 28.8 mm2 (45-60 deg) | none | 0 | 1, yes |
| Kennel ROOF (slope flat) | 46.7 x 68.4 x 43.7 | 0.90 | 2378 mm2 | 0 / 42 mm2, all flat (85-90 deg) | 3 x 0.9..1.1 mm x 8 mm, hidden but functional | 0 | 1, yes after dropping 8 degenerate faces |

Tip stability of the print itself: A-frame BODY CoM is 21.9 mm inside the footprint at 23.6 mm height (43 deg), Kennel BODY 12.7 mm at 27.5 mm (25 deg), Kennel ROOF 9.3 mm at 11.7 mm (38 deg, but see K2: the problem is stiffness, not tipping).

## 2. A-frame: problems ranked

**A1. Display is loose until the house closes, and the closing stroke is heavy (medium-high, function).** The display sits in a 38.5 mm pocket with 0.25 mm per side and nothing holds it. Insertion budget in the one stroke: 3 arms 17.9 to 21 N (log: 21.2 N; my formula with a uniform 7 mm width gives 17.9 N), plus J2 header 10 to 15 N, plus the board crush jaws: about 35 N, more than the 15 N rule 40 allows for the arms alone.
Fix:
- Three crush ribs in the display pocket, 120 deg apart at 30 / 150 / 270 deg measured from the top (the tab is at the bottom), each triangular 0.8 base x 0.35 high (0.25 gap + 0.10 interference), 2.4 mm long along the module PCB (assembled Y 3.2 to 5.6), running in assembled Y (vertical in print, so they print clean). Retention about 2 to 4 N, enough to carry the loose display with the FACE in hand. Do not rib the glass-tab zone (Y 1.6 to 3.1).
- Arms: undercut 0.4 (groove 0.5), lead-in 25 deg, retention 55 deg. Result: W 4.4 N, strain 1.07 % (limit 2.5 % in-plane: arm axis is X, in the layer plane), insertion 12 N for 3 arms, pull-out 10.4 N for 3 arms (the 7 N tilt requirement is met).

**A2. Side tip angle 46 deg, CoM Z 21.8 mm (medium).** Measured 45.9 deg (left), 46.1 (right), front 51, rear 56.6. A cable tug or a knock tips it.
Fix: any of: base corners R6 to R2 (foot half width 23 to 25.5 mm, 49.5 deg); 8 g steel ballast in the floor at Z 3 (50 deg; 23 g for 55 deg); both together give more than 53 deg. Add four 0.8 mm tall rubber feet pockets only if needed (they raise the CoM by 0.8 mm).

**A3. Vents on the rear gable only, 193 mm2 total (medium).** Intake slots (80 mm2, Z 7.6 to 15) and exhaust slots (113 mm2, Z 31 to 40) are both in the 3.0 mm rear wall, about 20 mm apart, behind a cable and a plug. Short circuit risk, no cross flow past the sensor corner (X 3..14, Y 46..60).
Fix: add intake in the left side wall at the sensor corner: 3 slots 1.8 wide x 9 long, long axis along assembled Y (vertical in print: no bridges), at Z 6.5 / 10.1 / 13.7, Y 46 to 58 (27 mm2 each, 81 mm2; wall 1.68 mm). Keep the rear exhaust. Slots must stay at least 1.8 wide (spec rule 24). Check they do not intersect the cradle jaw (Z 5.5 to 7.0): move the lowest slot to Z 8.0 if they do.

**A4. Hook retention faces are 60 deg overhangs in print; arm plan diagonals 51 deg (low).** Hidden: 3 hooks x 1.3 mm2 at 60 deg (0.35 mm growth per layer), plus the 40 mm2 at 45 to 60 deg (arm plan diagonals, `ARM_M` 0.8, 51 deg, intentional so every layer grows from the previous one). The retention face sets the pull-out force; with a rough face the real undercut varies by about 0.1 mm of 0.5. Coupon C2 is mandatory; PETG at 100 % fan on overhangs.

**A5. Crush bead on the board jaws is thinner than 0.8 mm (low).** `thin.py` finds a 0.8 base x 0.38 high triangle running 49 mm in print Z (blob at assembled X 3.3, Z 6.5 and the mirror at X 38). A single-extrusion line of 0.4 or less may be skipped by the slicer. Fix: base 1.2, height 0.45, flat apex 0.4.

**A6. Everything else passes.** Wall 1.68 (4 perimeters), arms 1.26 (3), lip ring 1.26, window 33.4 with 0.5 chamfer, no feature under 0.8 mm (FACE: 0.0 mm2; BODY: 3 mm2 = the crush beads), 74 and 57 sliver faces under 1e-3 mm2 (harmless, closed). Bed-edge chamfers: FACE 1.0 x 1.0 at 45 deg (spec says 0.6; 1.0 prints fine and costs 150 mm2 of first layer: mean inset 0.69 mm in layer 1, 0.05 mm by layer 5); BODY 0.7 in x 1.2 high (30 deg from vertical, inset 0.27 mm mean) which also hides elephant foot. Elephant foot compensation 0.10 mm.

**Seam (A-frame).** FACE: y_print = -z_assembly + 64, so the house bottom is +Y: use "Aligned back" (seam under the house). BODY: y_print = z_assembly + 0.6, so the house bottom is -Y: use "Aligned front" (not back: back would put the seam on the ridge). Scarf seam on. Check the preview: no seam on the ridge R10, none on the window ring.

## 3. Kennel: problems ranked

**K1. BODY print STL is not watertight (high, slicer repair on every import).** From the raw file: 70 degenerate faces (area < 1e-9), 60 edges used by more than two faces, 3 face-connected components. Two components are zero-volume 4-face sheets at print Z 52.2, X 1.7..2.5 and X 50.6..51.4, Y 30.6..33.1 (the start plane of rib 3, `RIB_Y` 52.2). Cause: `r - wedge` in `build_body()` where the wedge face is coplanar with the rib start face at `y0`, leaving a zero-thickness fin. The other non-manifold edges (single points) sit at the name-pill ends at the bed layers (X asm +-6.5, print Z 0.2 to 0.4), at the board pins and at the back-plate dimples: tangent V-groove cutters ending on each other. After dropping the degenerate faces the main shell is watertight (5796 faces, volume 14 938 mm3, 0 bad edges), so only the two sheets remain. The ROOF has 8 degenerate faces (bed-edge chamfer corners at Y 0 and 68.4, hook tips); dropping them gives a watertight single shell. BACK has 3 faces under 1e-6 mm2 (harmless).
Fix (10 minutes): copy `keep()` (decompose, drop volumes under 1 mm3), `simplify(0.004)` and `finalize()` (1/1024 quantisation) plus `strict_stl_check()` from `v4_aframe/aframe.py`, which is how the A-frame passes; make the rib wedge start 0.5 mm before `y0` (extend the cutter, never coplanar); overlap V-groove cutters by 0.05 mm where they meet; give the pill ends one revolve cutter, not a revolve plus an extrusion with a common tangent.

**K2. ROOF print is a tall thin wall with the retention hooks at its tip, and left and right hooks print differently (high).** Section (`roof_sections.png`): the flat slope on the bed, the other slope leaning 6 deg beyond vertical: 1.7 mm thick, about 38 mm tall plus a 45 deg tab to Z 43.7, 68 mm long (22:1). Loaded over a 10 to 15 mm wide nozzle footprint the wall is 0.5 to 0.7 N/mm stiff, so 0.3 N of nozzle drag moves the top 0.4 to 0.6 mm (estimate, `snaps.py`); the three wall tabs and their hooks (Z 41.7 to 43.7, hook tip blobs under 0.8 mm) are exactly there. Worse, the two tab sets print different hook faces (`hookfaces.py`):
- Flat-plate tabs (3): the 45 deg retention face rotates by the 42 deg lean to a normal at 175 deg from up, an 85 deg overhang, a 0.9 to 1.1 mm x 8 mm flat bridge (3 x 7.4 mm2). This is the functional face; a 0.2 mm sag removes 40 % of the 0.5 mm undercut.
- Wall tabs (3): the same face prints 5 deg from vertical, clean.
So the left side holds with a sagged ledge and the right side with a clean 45 deg face: pull-out forces differ and the roof twists when lifted.
Fix: do not print this roof in one piece. Options, best first:
1. Two slope plates printed flat (each 1.7 x 34 x 68.4 with its 3 tabs) joined at the ridge by a 6 mm half-lap plus 0.3 mm CA, or a third part, a 8 x 68 mm ridge cap that clips over both edges (colour A or B). Each half prints like the good left half, with one catch: in a flat-plate print the tab leans 42 deg, so the 45 deg retention face lands at 85 deg overhang (a 1.1 mm bridge, hidden but functional). It is only clean if the retention face is 85 deg or more from the tab axis (a locked ledge, released by a 30 deg lead-in on the rib side), so halves fix the tall wall but not the hook face. The tent print (option 2) fixes both.
2. Tent print (ridge up, as assembled): all hook faces print clean (retention face 45 deg facing up, lead face 30 deg from vertical facing down), tabs stand as vertical blades, no leaning wall. Not possible as modelled: the tab tips are at asm Z 26.3, the eave at Z 35.1, so the roof would stand on 6 tab tips (first layer 60 mm2) with the eaves 8.8 mm in the air. Needs a rim at or below the tab tips: add 9 mm vertical skirts, or move the rib hooks to the eave line. Underside is 48 deg from vertical at pitch 42 (hidden), acceptable; use pitch 45 to be exact.
3. As built plus: brim 5 mm, outer wall 40 mm/s above Z 20, acceleration 1000, fan 100 %, hook coupon first. Not recommended.

**K3. Plank lines and door/plaque V-grooves print as hairlines (medium, visual).** `grooves.py` slices the real mesh: on the front face (bed side) each 0.4 deep V groove has a 0.60 mm gap in layer 1 (z 0.1), 0.20 mm in layer 2 (z 0.3) and none after. A 0.2 mm gap is below the 0.42 line width and is closed by the slicer; the groove prints as one 0.6 wide x 0.2 deep line, which the textured-PEI grain (0.1 to 0.2 mm) nearly hides. Same for the R21.2 door ring and the name-pill outline. On the side walls (vertical grooves in print) the V is 0.8 wide x 0.39 deep: this survives as a perimeter notch and shows in raking light; wall behind it is 1.3 mm. The 0.4 groove also thins the 1.35 mm front plate to 0.95 mm (rule 18: under 1.2). The name "plaque" is only an outline, with no recess for text. The pill bottom line (Z 7.3) is 1.3 mm from plank line 1 (Z 6.0), leaving a 0.5 mm ridge in layers 1 to 2 (6.6 mm2 thin blob).
Fix: on the bed face use a flat-bottom groove 1.0 wide x 0.4 deep (two layers) with 45 deg flanks outside (1.0 at the floor, 1.8 at the face), at least 2.5 mm from any other groove; or 0.6 deep with a 1.2 floor if the plate is 1.8 thick. On vertical side faces use 1.2 wide x 0.6 deep (V or trapezoid) and raise the wall to 2.1 there. Never rely on grooves narrower than 0.8 at the bed.

**K4. Board pins are not press fits (medium).** Pin diameter 2.1 (r 1.05) into 2.2 mm PCB holes: nominal 0.1 clearance; printed horizontally (axis = print Y) the pin comes out 0.1 to 0.15 undersize and sags at the underside (`analyse.py`: unsupported 0.8 x 1.8 mm patches, 2.0 mm deep, at print Z 12.5 and 56.5, X 11.9/41.9). Retention is nil. Fix: pin core diameter 1.9, three axial crush ribs 0.2 high x 0.6 wide at 120 deg (circumscribed 2.3, 0.1 interference on 2.2); orient one rib at the top (sag side stays clean), pin length at least 3.0 for 1.6 mm PCB plus lead-in cone 0.6. Add a 0.5 mm fillet at the post.

**K5. Roof tab retention is 45 deg in code, 60 deg in `concept.md` (low-medium).** `RET_DEG = 45.0` in `kennel.py`; pull-out is 1.5 N per tab, 8.9 N for 6 (my formula and the run.log agree), insertion 3.0 N per tab, 18 N total. Strain 0.76 % across layers is fine (limit 1.5 %; 1.78 % at the 48 deg axis angle of the flat-plate print). Keep 45 deg, fix the text. For comparison, the A-frame proposal in section 7 gets 6.8 N pull-out, 7.8 N insertion with 4 tabs.

**K6. Back plate arms are weak and thin (low).** t 1.4, in-plane width 1.7 (print layers 8), L 8.9, strain 1.59 % (limit 2.5 % in-plane, fine); pull-out 1.1 N per arm, 2.2 N for 2 plus friction. The arm is 1.4 wide in plane: 3 perimeters (1.26) only just; root has no fillet. Fix: arm 1.68, root R1.0, bump retention 55 deg, and a 0.5 mm stop ledge so the plate cannot be pulled out by the cable (cable tug 2.1 N tips the house before it moves the plate, but a plug yank does not).

**K7. Tip angle 40 deg sideways (medium).** CoM Z 23.5, foot hull X -0.8..41.5 (42 mm wide for a 53 mm house, feet at +-17.5 mm). Fix: feet at +-21.5 mm (outer edge 25.0): 46.8 deg. A cable tug tips it backwards at 2.1 N; rubber feet with 1 mm grip pockets help more than ballast.

**K8. Small items.** BODY hidden bridges are 1.7 to 1.8 mm (fine). Front plate 1.35 mm is 3.2 perimeters (set 1.26 or 1.68). Visible "overhangs" in the BODY report (36 mm2) are exactly 45 deg bed chamfer and groove flanks, not a defect. Kennel seams: BODY y_print = -z + 61.19 so use "Aligned back" (seam under the house); BACK y_print = z - 3, use "Aligned front"; ROOF seam on the inner (hidden) edge of the plate-to-wall corner, painted enforcer, never on the leaning outside wall.

## 4. Snap table (PETG, E 2100 N/mm2, d = undercut + 0.1, friction angle 17 deg)

Strain limits: in plane (beam axis in the layer plane) 2.5 %, across layers (axis vertical) 1.5 %, between by 1/sqrt(cos^2/2.5^2 + sin^2/1.5^2): 1.78 % at 48 deg from the layer plane.

| Feature | t / w / L / d | Strain | Limit | Insertion | Pull-out | Verdict |
|---|---|---|---|---|---|---|
| A-frame FACE arm x3 as built | 1.26 / 7 avg / 9.3 / 0.6, lead 30, ret 60 | 1.2 to 1.3 % | 2.5 % | 6 N each, 18 N | 5.2 N each, 15.6 N | strain fine, force too high with the header |
| A-frame FACE arm proposed | 1.26 / 7 avg / 9.4 / 0.5, lead 25, ret 55 | 1.07 % | 2.5 % | 4.0 each, 12 N | 3.5 each, 10.4 N | use |
| Kennel roof tab x6 as built | 1.26 / 8 / 12.2 / 0.6, lead 30, ret 45 | 0.76 % | 1.78 % (flat print) | 3.0 each, 18 N | 1.5 each, 8.9 N | strain fine; hook faces print badly (K2) |
| Kennel back arm x2 | 1.4 / 1.7 / 8.9 / 0.6 | 1.59 % | 2.5 % | 2.2 each | 1.1 each | weak retention (K6) |
| Roof tab proposed x4 (section 7) | 1.26 / 6 / 12 / 0.5, lead 30, ret 55 | 0.66 % | 1.5 % | 2.0 each, 7.8 N | 1.4 each, 5.7 N | use |

Roots: fillet R at least 1.0 or taper t to 1.4 t over 3 mm (the A-frame arms have the taper; the Kennel tab root has 0.8 and no fillet).

## 5. Wall thickness, thin features, chamfers

Measured by opening the layer rasters (0.1 mm pixel): nothing under 0.8 mm in either A-frame part except the crush beads (A5). Kennel BODY: 22 mm2 under 0.8 mm (all in layers 1 to 2: groove ridges and pill, 6.6 + 1.6 + 1.6 + 1.0 mm2 blobs; plus two 1.2 mm2 blobs, Z 10.3 to 54.9, X asm +-15, 1 mm2 from the board posts' teardrop tips). Kennel ROOF: hook tips under 0.8 wide at Z 9.9 to 12.5 (3 x 6.5 mm2) and Z 41.7 to 43.7 (3 x 4.0 mm2): these are the hook lands, acceptable only because the snap geometry needs them; make land 0.6 and tip 0.8. Material under 1.26 (not 3 perimeters): A-frame 69 + 88 mm2 (arm edges, hook tips, bead), Kennel 71 + 35 + 64 mm2. Skins thinner than 0.8 in print Z: none found in any part.

Bed-edge chamfers and elephant foot: set elephant-foot compensation 0.10 mm for all. FACE 1.0 x 1.0, BODY 0.7 x 1.2, Kennel body 0.6 on the front plate only (first-layer inset about 0.5), BACK 0.6 (mean inset 0.92 includes slits), ROOF 0.6 on the outer slope ends only (not on the long eave edges: add 0.6 there).

## 6. Plank lines and grooves (rules from the Kennel test)

1. At the bed face a groove must leave at least 0.8 mm gap in every layer it exists in; a V that reaches 0.2 mm in layer 2 closes. Use flat-bottom grooves, floor at least 0.8, depth a multiple of 0.2, at least 0.4 deep, at least 1.0 wide at the floor.
2. On vertical faces (print Z) a V or trapezoid at least 1.2 wide and 0.6 deep reads at arm length; 0.8 x 0.4 is visible only in raking light.
3. Front plate behind a groove: at least 1.2 mm remaining.
4. Pitch at least 2.5 mm between parallel grooves; 8.5 mm planks are fine.
5. Textured PEI grain (0.1 to 0.2 mm) swamps any feature under 0.3 mm deep on the bed face.

## 7. Next step: A-frame with a coloured lift-off roof and a name plaque

### 7.1 Splitting the roof so it prints without supports

1. **Roof = a cap (two 45 deg slopes with the R10 ridge and R5 eaves) over a planar rim, printed rim down, ridge up ("tent").** The 45 deg pitch gives an underside exactly 45 deg from vertical (use 47 deg or print 3 to 10 deg tilted to be safe). Do not print the cap on one slope: a flat slope puts the R10 ridge on the bed as a convex fillet, and that fillet is an overhang of 45 to 90 deg over about 7 mm x 2.9 mm, visible. Flat-on-slope also makes the second slope a vertical wall about 33 mm high that flexes.
2. **Rim planar.** The A-frame eave falls from Z 41.6 (front) to 31 (rear), a 9.5 deg rim angle. Either cut the cap with one plane (the rim) and print the cap rotated about X by that angle (underside then 44.2 deg from vertical, better than flat), or reduce the taper. Verify with `roof_orient.py` adapted to the new STL: overhang above 45 deg visible 0, flat hidden 0.
3. **Rim foot.** A knife-edge eave gives a first layer of 60 mm2 (measured on the Kennel roof tips). Cut the rim with a horizontal flat foot: at least 2.1 mm wide, so first layer at least 2 x 2.1 x 64 = 270 mm2; plus brim 3 mm.
4. **Inner ridge.** Concentric R7.9 inner arch leaves a crown with about 4 mm bridge (2 R sin 15 deg) and 15 to 45 deg corbel: works but rough. Prefer an inner profile of two 45 deg planes meeting at a 1.2 mm flat (bridge 1.2 mm); the crown thickens to about 4 mm; there is space (disc top Z 52.7 + 0.5 versus inner apex about 60).
5. **Nothing may go below the rim plane**: tab tips at least 1.0 mm above the rim plane (the Kennel lesson: tabs 8.8 mm below the eave make the tent impossible).
6. **Thickness** 2.1 (5 perimeters) on the slopes, 1.68 minimum; vent slits 1.8 wide with the long axis along the slope (vertical-ish in print: no bridging problem, top bridge 1.8).
7. **Seam**: aligned at the rim, behind the front gable; scarf on.
8. **Slicer**: outer wall 50 mm/s, fan 60 % (100 % on overhangs), supports off, elephant-foot 0.10, ironing off, brim 3 mm outer.

Alternative if one piece does not suit: two flat slope plates plus a ridge cap (clip-on); then every hook retention face must print at 85 deg or more from the tab axis or be a vertical-ish face (see K2); the tent print avoids that problem entirely, so prefer it.

### 7.2 Retention: a light, positive click that still lifts by hand

- 4 tabs (2 per side, Y at 12 and 46 from the front, symmetric about X 20.5), hanging from the roof underside inside the skirt, each: t 1.26, w 6.0, L 12 (free length root to hook), undercut 0.4, clearance to rib 0.1 (d 0.5), hook land 0.3, lead-in 30 deg (long ramp), retention 55 deg (60 deg if you want it firmer), root fillet R1.0. Result (`snaps.py`): strain 0.66 % (across-layer limit 1.5 %, margin 2.3), W 1.8 N, insertion 2.0 N per tab (7.8 N total), pull-out 1.4 N per tab at 55 deg (5.7 N total) or 1.7 N at 60 deg (6.8 N). That is a click you can feel and hear, and lifts straight off with two fingers; mass of the roof about 10 g.
- Ribs or grooves on the body: 0.5 deep, 6.6 long (0.3 longer than the hook plus 0.1 each side), walls at least 1.2, groove walls 45 deg at the top where they print as overhangs.
- Do not go below L 10 with the 0.4 undercut: strain 0.95 %, insertion 13.5 N total, too stiff.
- Locating without the snaps: perimeter lip on the body, 1.2 high x 1.2 wide, roof groove 1.6 wide (0.2 per side), 0.5 x 45 deg lead-in on both; two front/back stops with 0.3 gap.
- Never magnets near the ESP32 antenna (brief: no metal near it); not needed.

### 7.3 Name plaque

Where: the front gable band below the door has about 4.9 mm of usable height (Z 1.5 to 6.4: chamfer 1.0 at the bottom, display tab pocket from Z 6.95), up to 40 mm wide; cap height at most 3.5 mm there. The space in the gable peak above the door is 5 to 6 mm tall and about 18 mm wide at best. If the name needs more, move the plaque to the roof front fascia or the rear (rear wall 3.0 mm: debossing 0.6 is safe there).

| Option | How it prints | Minimum sizes | Notes |
|---|---|---|---|
| A. Colour inlay in the first layers (recommended for the front band) | Text colour B only in layers 1 to 4 (0.8 mm), rest colour A; the front face is the bed face so the text is flush and crisp, no structural cost | stroke at least 0.8 (1.0 preferred), cap height at least 3.2 for a 0.8 stroke (bold, stroke/cap 0.25), 3.5 comfortable, 5 clean; letter gap at least 0.8; counters at least 1.0; layers at least 4 (0.8) so colour A behind does not show; text at least 1.0 mm from the window chamfer and from the bed-edge chamfer | purge tower and flush on 4 layers only; one 3MF with the plaque as a part of the FACE object so the registration is exact |
| B. Separate inlay tile | Tile 26.0 x 8 x 1.6, text raised 0.6 on the tile top (flat print: easiest), tile colour B, text colour A by a colour change at layer 6; pocket in the front plate 26.4 x 8.4 (0.2 per side) open to the bed, 0.8 floor behind: bridge only 8 mm | stroke at least 0.8, cap at least 3.5 | the pocket is open to the bed so the layers 1 to 4 have a hole; retention: 2 crush ribs 0.15 plus a dab of glue; tile flush within 0.1 |
| C. Debossed text | Strokes absent in layers 1 to 3, then bridged over 1.0 to 1.5 mm | stroke at least 1.0, depth 0.4 for a 1.6 wall (leaves 1.2; 0.6 only on the 3.0 rear wall), cap at least 5, counters at least 1.2 | visible only in raking light; weakest contrast, no colour |
| D. Embossed frame or text | Impossible on the bed face (raised features hang below the bed); fine on a vertical wall or top face: relief 0.6 (3 layers), stroke at least 0.8, frame line at least 0.8 wide x 0.6 high | cap at least 3.5 | use only on side walls or on the roof slope; on a 45 deg slope text is stair-stepped: cap at least 6 |

Outline offsets for vector text: debossed grow the outline 0.05, raised shrink 0.05; fonts: bold rounded sans, no serifs, no hairline strokes; keep curves at 0.1 mm tolerance when exporting.

### 7.4 Keeping two colours aligned

1. Separate parts (roof, body): align by geometry. Generate the roof inner profile from the body outer profile by one function (offset +0.2), so the fit is correct by construction; register with the lip and stops above. Make the visible joint a reveal: 0.6 x 45 deg bevel on both edges (shadow line 1.0 to 1.2 mm wide), so a 0.2 to 0.3 mm lateral error reads as design, not misfit. Expect each colour to shrink 0.3 to 0.5 % differently (0.2 mm over 53 mm): the 0.2 mm clearance absorbs it.
2. Print the separate parts as separate jobs (one filament each): no purge waste, no AMS errors. Dry both filaments (PETG 65 degC, 6 h).
3. Same-object colour (plaque inlay): all colour boundaries on vertical walls or on layer boundaries (multiples of 0.2); no colour region under 0.8 wide or 4 layers thick; export body and plaque as one 3MF object with two parts (never re-centre either); no colour boundary within 1.0 mm of a feature edge; verify in the slicer colour-change layer view; shared boundary overlap 0 (let the slicer interlock).
4. Check the plaque colour by printing the 3.5 / 4 / 5 mm cap rows on coupon C-plaque before the final house.

## 8. Coupons for this round

| # | Coupon | Measure |
|---|---|---|
| C2-v4 | A-frame arm with groove (undercut 0.4, lead 25, ret 55) and roof tab with rib (t 1.26, w 6, L 12, 0.4), each 20 cycles | insertion and pull-out with a kitchen scale (target 12 N for 3 arms; 2.0 and 1.4 N per tab); white lines; hook face quality at 55 and 60 deg |
| C-pocket | Display pocket ring with 3 crush ribs 0.35 high | feel and retention of the real display (target 2 to 4 N) |
| C-groove | Bed face with 0.4 flat-bottom groove 1.0, V 0.8 x 0.4, 1.2 x 0.6; side wall with the same | what is visible at arm length on textured PEI |
| C-plaque | Inlay A and tile B rows, cap 3.0 / 3.5 / 4 / 5, stroke 0.6 / 0.8 / 1.0 | legibility, colour bleed |
| C-roof | 20 mm slice of the roof cap printed rim down at 0 deg and at the 9.5 deg taper tilt | underside quality, rim foot adhesion, ridge crown bridge |
| C-heat | As in DFM_SPEC C8 with the new vents | ceiling and sensor corner temperature |

Check list before the next print: one watertight shell per part from the raw STL (not after repair); no visible overhang above 45 deg; no part standing on features under 100 mm2 (roof rim at least 270 mm2); bridges at most 8 mm and none on hook faces; snap strain by the table above; grooves per section 6; seam direction per part (section 2, K8).
