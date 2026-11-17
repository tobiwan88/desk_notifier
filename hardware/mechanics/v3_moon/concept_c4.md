# C4: "Moon on a plinth", aesthetic pass

Script `moon4.py` (log `run4.log`), renders `render4.py` -> `render4_{front,side,34,back34,internals,side_internals,under,top}.png`,
STLs `stl/moon4_{face,plinth,back}_{assembled,print}.stl`. C3c files are unchanged. PETG, three parts, clips inside (all C3c snap numbers unchanged).
Outer size 53.40 W x 63.85 D x 56.15 H (C3c 47.95 x 63.85 x 55.35; +1.2 H is the feet rails).

## What changed and why
Reviewer verdict on C3c: a thin disc on a flat L-base, keyhole neck, power-bank slab, noisy slots. C4 answers with one front, one sweep, one grille.
1. **One front plate, no neck.** The disc and the plinth front are one flat face (prints face down). Its outline is the plinth section plus the disc,
   joined by a 2D closing R11: the plinth's shoulder (R6 x 4 ellipse) flows through an S-curve into the disc. Narrowest width about 37 mm (measured from the front render), never a stem
   (reads like a Braun mantel clock / radio). Window 33.4 mm, no bezel, unchanged.
2. **Saddle.** A quarter-circle cove R30 (loft of convex rounded sections, 35 wide, flared R3 at the foot) rises from the plinth top to the back of the
   plate and meets it tangentially at Z 42. In profile the L becomes one curve. It belongs to the PLINTH part and rests against the plate, so a push on the
   disc is carried in compression. The plate's own section modulus is no longer the limit.
3. **Pillow plinth.** Symmetric plan about the display axis (antenna side sets half-width 26.7), elliptical top edge 6 x 4, bottom R3, rear corners R11,
   top falls 0.9 mm to the rear (taper). Left flank is hollow (air for the sensors, less mass). Two feet rails 1.2 high: a visible shadow gap, plinth floats.
4. **Vents hidden.** ESP exhaust = 8 slots 1.8 wide in the saddle (behind the plate, visible only from behind as one fan grille); intake and sensor air =
   floor slots between the rails (front 3, rear 6). No slots on the top. Seam reveal (0.6 chamfer) is the only line besides the window ring.

## Checks (run4.log)
- Housing x internals 0.000000 mm3 (11 items); clearance boxes 0 (display +0.25: 0.05 mm3 grid snap, gap 0.249, as C3c). Part x part overlap 0.
- One shell per part, watertight, 0 bad edges (assembled and print STLs re-imported).
- Layer check (0.2 mm, 45 deg): mid-air islands FACE 0, PLINTH 0, BACK 0. Unsupported beyond 45 deg: FACE 1.3 mm2, BACK 0.5 mm2, PLINTH 311 mm2 all
  hidden bridges: rear roof 7.2 mm span, antenna bay end 6, slot ends 1.4. Plinth first layer 1212 mm2 (C3c 159): it now stands 1.3:1, no brim needed.
- Plate section modulus (weakest z 14.3): S_front 347, S_rear 228, S_side 2019 mm3 (limit 80); 15 N at 41 mm = 1.8 MPa. Tab pocket 23.5 kept.
- Snap numbers unchanged: FACE arm 1.19 % strain, pull-out 18.5 N (3), insertion 21 N (3); BACK arm 0.79 %.
- ESP ceiling clearance 2.00 mm (flat top over Y 26..44.5); USB receptacle 1.12 (C3c 1.87), J2 2.56. Wall min 1.38 (floor 1.4), soft walls >= 1.68.
- Vent open area (nominal): ESP exhaust 230 mm2 (C3c 184); total 392 (C3c 311).
- Mass 35.5 g solid-modelled (C3c 25.5). CoM with 12 g (21.4, 24.5, 10.7); tip angles front 64, rear 67, left 61, right 59 deg; with 5 g display + 7 g
  board min 53 deg (right).

## Weak points
- 5.5 mm wider than C3c and the left 6 mm beside the board is dead width (hollow): the price of a symmetric form.
- The lower plate is a big flat 53 x 25 mm field; it can read as a radio front. Texture or a 0.3 mm recess are options, not tried.
- Sensor corner has no direct top exit: floor slots, hollow left flank, then exhaust in the saddle 40 mm away. Test the BME688 response.
- Rear ceiling over the USB receptacle is 1.12 mm (was 1.87); saddle slots are 16 mm deep chimneys (dust).
- Solid saddle adds mass (about 10 g solid-modelled); slicer infill reduces it.
- Insertion force 21 N (3 arms) and the 51 deg arm diagonal are as in C3c.
