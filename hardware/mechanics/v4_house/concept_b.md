# A-frame v4b: coloured lift-off roof, name plaque, critic fixes

Same soft reduced A-frame (rounded pentagon, taper to the back, R3 base / R5 eave / R9.2 ridge), now three parts: FACE (front gable plate, grey), BODY (grey), ROOF (second colour). All numbers are in run_b.log, script aframe_b.py, STLs in stl/aframe_b_*.stl.

## Changes against v4
1. **Roof.** The roof is a tent shell (two slopes, rounded ridge, 15 mm vertical skirt, rear gable with exhaust). It has no front wall: the FACE plate now carries the whole gable outline up to the ridge, and the roof starts 0.2 mm behind it. The roof edge (0.6 x 45 deg bevel on both sides, 0.2 gap) therefore draws one continuous line: along the eave on both sides and up around the gable. The v4 door boss through a hole in the roof is gone (a round hole in the roof is a 43 mm bridge in print and the roof cannot lift past it: measured interference 11 mm3).
   - **Print: roof on its eave rim, ridge up, tilted by atan(K_TILT) = 9.2 deg about X** so the sloped rim lies flat. Not front-down (slender arch, 46 mm tall thin walls), not two slope halves (a ridge seam, two parts to align). Slopes lean 44 deg from vertical, inner ridge is a flat bridge 7.2 mm wide, first layer 330 mm2 (rule 270), no islands, no visible overhang; hidden bridges are the 1.8 mm vent slots and that 7.2 mm crown.
   - **Click.** 4 vertical fingers on the body (2 per side, Y 14..25 and 32..43), in the XZ plane, bending in X in the layer plane. t 1.26, free length 14, wedge-shaped plan (60 deg edge, so every layer grows from the one below; no mid-air start), bump 0.5 with 25 deg lead and 30 deg retention face, groove in the roof skirt 0.1 clearance. Strain 0.45 % (limit 2.5 %), lateral force 2.3 N per finger, lift-off 2.5 N per finger = 10 N for four (friction angle 17 deg, PETG; 14 N PLA), press-on 8.4 N. Roof lift sweep (0.3 .. 50 mm, straight up): interference 0.000 mm3.
   - Roof off = the whole top of the body is open (board, USB, cradle, J2 visible: render_b_roofoff.png).
2. **Plaque.** 30 x 8 mm rounded frame below the door (Z 4.5..12.5), a V groove 1.25 mm wide closing to 0.3 mm over 0.6 mm depth in 0.1 mm steps (43 deg flanks, so no bridge, layer check: 0.6 mm2 over 45 deg, 6 regions). No text. The area inside is flat for a colour inlay (layers 1 to 4, stroke >= 0.8, cap >= 3.2 per the DFM) or a sticker. The facet check still lists the 0.1 mm step ledges as "visible overhang" (88 mm2); the layer check does not.
3. **Fixes.**
   - Stance: foot x -3.6..44.6 (R3 base). Side tip with 12 g at the board centre 51.8 deg (target 50). With the real split (5 g display at Z 33.7, 7 g board) 48.0 deg; with the optional 6.8 g steel plate in the underside recess (29 x 30 x 1.0, 40 deg ends, prints as a vertical-wall recess) 51.3 deg.
   - Display clip: 3 crush ribs (0.8 base x 0.35 high, 0.10 interference, 90 / 210 / 330 deg, along Y, lead-in taper at the rear end) instead of 4 beads; honest limit: friction only (about 2 to 4 N), a printable snap does not fit the front-down pocket depth.
   - FACE arms: undercut 0.4, lead 25, retention 55 deg: strain 0.99 %, insertion 14.8 N for 3 arms (was 21), pull-out is 13 N (DFM formula) to 51 N (W tan(a+phi)); FACE is meant to stay on.
   - Vents: rear intake 69 mm2 (6 slots, Z 7.6..14), rear exhaust 113 mm2 (roof gable, Z 31..40), ridge 54 mm2 (3 slots under the crown), left side 65 mm2 at the sensor corner (3 slots 1.8 x 12, pointed 45 deg front end so no visible bridge). Total 301 mm2.
   - USB: 12.8 x 7.8 pocket, rear face has a 0.8 x 45 deg funnel (14.4 x 9.4 opening), plug gap 0.36.
   - Board-jaw beads thickened to 1.2 base, 0.4 flat apex (DFM A5). Roof/body colour alignment: 0.2 gap, 0.6 x 45 deg reveal both sides; roof and body are separate print jobs.
   - SKIRT raised from 12 to 15 mm so the 14 mm fingers stay below the roof's inner slope.

## Checks (run_b.log)
Housing x internals 0.000 except the display crush ribs 0.226 mm3 (by design); face x body, face x cap, body x cap 0.000; roof sweep 0.000. Ceilings: ESP 31.99, USB 40.22, J2 48.10 mm; antenna 0.67 (needs 0.5). STL reload (float32 weld): face, body, cap, assembled and print, all watertight, one shell, consistent winding. Islands 0 / 0 / 0. Masses face 6.0, body 19.4, roof 17.3 g (solid PETG). Outer 53.4 x 63.85 x 65.9.

## Weak points
Display hold is friction. 10 N lift-off is "a few N" only by hand standards; coupon C (finger + groove) must confirm, bump 0.4 lowers it. Roof in tilted print has a rim foot 330 mm2 (spec 270) but the skirt chamfer leaves 1.5 mm foot width. Real tip angle needs the steel plate to pass 50 deg with the display up high. The FACE gable plate above the door is a 1.6 mm free-standing cantilever (about 11 mm tall above the boss ring).
