# Desk notifier housing (Fusion, 3D printed, 2-part snap-fit)

## Context
Case for notifier-v1 (50x36x1.6 mm, ESP32-S3-WROOM, USB-C, BME688 + SHT40) and the round GC9A01 1.28" display module (38 x 45.5 mm PCB, 11.5 mm incl. header).
Source STEP: `hardware/notifier-v1/notifier-v1.step`.
Case stands on a desk, screen in front, board behind. Needs sensor openings. Fusion is driven over its local MCP.
Owner answers: tilt back ~15 deg, PLA, display plugs **directly** into the 7-pin socket, USB-C out the back wall, no extras in v1.

## Key finding: the direct plug works, and it dictates the layout
J2 (1x07 right-angle socket, board x=140..150, mouth faces +X out the right edge, pin row y 113.3..126). A display pin header therefore enters horizontally, so the display plane is **perpendicular** to the board plane:
- Board lies flat in the base (components up, all on F.Cu, bottom face is flat), right edge (J2) toward the front.
- Display rises from the front edge of the board, glass facing front, pin side facing the board. Display pin row runs across its width = board Y axis.
- USB-C (J1, left edge, mouth faces -X) ends up at the **back** wall. Matches the request.
- Whole assembly is tilted back ~15 deg by a wedge-shaped base (or the bottom face is cut at 15 deg). Case depth is about 50 mm board + ~12 mm display + walls, height about 62 mm (display) + base.
- The plug is the only mechanical link between the display and board: the front shell must hold the display, the base must hold the board, and the two must register through the plug (use 0.2-0.3 mm of float, not a rigid fit). Board needs support at the far end (M2 holes at x=103/147, y=103/133 as standoffs).
- The J2 tails stick out the bottom by about 1.7 mm: recess in the base around x 138..142, y 109..127.

## Parts
1. **Front/lower shell**: front face with round window (active area 32.4 mm, bezel opening ~34 mm), display pocket (38 + 0.3 mm square-ish for PCB, 1 mm retaining lip over the glass edge), display is pushed in from behind. Sides and wedge base with the board standoffs.
2. **Top cover**: closes from above and the back, snaps onto the shell. USB-C cutout in the back wall, sensor and vent openings, nothing metal over the antenna.

## Design rules (from the research agents; PLA adjusts them)
- Walls 1.6-2.0 mm (multiples of 0.4 nozzle), 0.2 mm layers.
- Clearances as Fusion user parameters: pocket 0.25 per side, M2 holes 2.4 mm, USB-C cutout 9.6 x 3.8 mm (plug opening ~0.5 mm over the 8.4 x 3.2 receptacle).
- Snaps: cantilever, print flat (X/Y), length 10-12 mm, thickness 1.2 mm, hook 0.6-0.8 mm, lead-in 30-45 deg. PLA strain limit about 1-1.5 % with eps = 1.5 t d / L^2: choose d accordingly. Two snaps per side plus locating tongue/groove. Add a pry notch.
- PLA caveat: HDT ~55-60 C. Fine for a USB-powered desk unit, but avoid direct sun or a heater. PETG is the drop-in swap if the case warps.
- Front face printed flat on the bed; top cover closed side down; no supports in the display bore.
- Antenna overhang (board y 94..100, x 116..134): plastic ok, keep 6 mm free of metal and no screws near it; board GND pour stops at y=101.8.
- Sensors: BME688 (107.9, 103.4) sits on a tongue open to the board's top edge; SHT40 (103.6, 111.2). Put vents on that side wall/top in a chimney path, 1.2 x 6 mm slots, away from the ESP32 side. Heat sources: ESP32, L1/U3 are 35+ mm away. Backlight heat of the display is the main bias: slots high at the back and low on the sensor side.
- LED D1 (130.6, 124.3) and no buttons on the board: no holes in v1 (as chosen).

## Fusion workflow
1. Register the MCP (needs your OK, config change): `claude mcp add --transport http fusion http://127.0.0.1:27182/mcp`. Fusion must be running with the MCP enabled (Preferences > General > API). Server answered HTTP 405 on GET, so it is up. First step: `tools/list` to learn the real tool set. Fallback if the MCP is weak: Fusion Python API script or the community servers (faust-machines/fusion360-mcp-server).
2. Import the STEP as a reference component; check units/origin (KiCad Y is down, check against hole positions).
3. User parameters (wall, clearances, tilt, snap dims); components `shell`, `cover`, `pcb_ref`, `display_ref`.
4. Build keep-out bodies from the PCB (per-side bounding boxes + 0.5 mm margin), model the display module from the drawing (no STEP exists).
5. Shell, cover, snaps, cutouts; interference check; section views; export 3MF/STL per part; save the `.f3d` source and exports under `hardware/mechanics/` (in the desk_notifier repo or the hw-sch repo, owner to say).
6. One feature per step, save after each, verify numbers by measuring in Fusion (the Autodesk blog warns against complex models).

## Milestones
- M0: verify inputs (below). M1: coupon print (display pocket with 3 clearances + 1 snap + J2 plug alignment). M2: full v1, dry-fit. M3: v2 from the measured fit.
- Done when: display plugs in without force, snaps survive 5 cycles, USB-C plug fits, Wi-Fi RSSI within ~3 dB of bare board, sensors read sensibly vs a reference.

## To verify before modelling
- Display module pin side faces the board and its header height / plug depth (the drawing shows 11.5 mm total, pins 5.76 above PCB); which way the display extends (up) relative to J2's pin row.
- USB-C mouth height above the board (~1.6, estimated) and SHT40 height (estimate); the STEP may differ from the KiCad library values.
- Note: this board is ESP32-S3 (CLAUDE.md describes the C61 firmware board); the housing follows the notifier-v1 STEP.

## Verification
Fusion interference analysis (shell/cover vs PCB, display, keep-outs = 0), section views at snap, USB-C, vents, plug; print coupon first, then the full case with the real board and display.
