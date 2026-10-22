# Housing (Fusion, 3D printed, 2-part snap-fit)

Design plan: `PLAN.md`. Source: `desk_notifier_housing.f3d` (direct B-rep built by script, not live-parametric). Input: `../notifier-v1/notifier-v1.step` (no J1/U5 solids; J1 came from a kicad-cli regenerated STEP).

Prints (PLA, 0.2 mm layers, Bambu Lab): `plate_shell_cover.3mf` / `.stl` (shell, cover, display dummy, 10 mm apart), or `shell.stl`, `cover.stl`, `display_dummy.stl`, `coupon_m1.stl` (fit test, print first).
- Shell: front face down, ~223 mm2 overhang (one optional 6 mm roof).
- Cover: stands on the outer face of its 38 mm wall; support the 296 mm2 shelf at the top of the rear wall.
- Dummy: glass down, support the PCB underside.

Unverified assumptions (no display STEP): header 2.54 mm, pin row 3.0 mm above the PCB bottom edge, glass centre 26.5 mm above it, PCB + glass 5.74 mm; SHT40 height 1.0 mm. Check against the real module with the dummy before printing the full case. Untested by a real print.
