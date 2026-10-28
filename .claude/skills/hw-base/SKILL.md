---
name: hw-base
description: Read first for any hardware/PCB task (KiCad schematic, layout, parts, ordering). Also use when starting a new board profile (board.yaml, critical.yaml, datasheet cards), writing or changing a hardware/calc check script, or writing a hardware skill. Task skills: kicad-schematic, kicad-layout, hw-parts.
---

# Hardware base

Board-neutral. Facts about one board live in its profile, never in a skill.

## Folder contract
- `hardware/<board>/` = one board profile:
  - `board.yaml`: files, fab, stackup (layer roles), net classes, layout settings, BOM alternates, regulator inputs.
  - `critical.yaml`: critical blocks, one key per `calc/critical.py` check (see Vocabulary).
  - `parts/<MPN>.yaml`: one datasheet card per IC or critical passive.
  - `BOARD.md`: purpose, decisions and why, commands (gate, calc list, fab tools), board-specific pitfalls.
  - the KiCad project, generated: edit the generators named in `board.yaml`, not the KiCad files.
- `hardware/calc/`: board-neutral check scripts; each takes the board folder.
- `hardware/tools/`: pipeline and fab helpers (stock lookup, renders, fab package); `BOARD.md` lists the commands.
- `hardware/agents/`: prompts for narrow helper agents.
- Start every task by reading `BOARD.md` and `board.yaml` of the board you work on.

## Rule: numbers come from scripts
- Any check with a number (distance, impedance, voltage, current, timing, length, stock) runs a script. Never estimate in the reply.
- Look in `hardware/calc/` first. If nothing fits, write a script and keep it.
- A script:
  - takes `hardware/<board>`; reads inputs from the profile; hard-codes nothing about a board
  - prints one row per check: `OK`, `WARN`, `FAIL` or `MISSING` and the numbers used; ends with `<name>: OK|FAILED`
  - exits non-zero on `FAIL`; `MISSING` = an input is `null` in the profile (fill it from the source, never guess)
  - reports `WARN` when an input has no source (card value without `section`, profile input without `source`)
  - names the source of each formula and limit in a comment (datasheet section, IPC-2141, IPC-2152, app note); design margins live in the profile
  - has a `--selftest` with one known answer when the formula is not trivial
  - uses the standard library, numpy/scipy and the `.kicad_pcb` reader `calc/pcb.py`; no new framework
- Add the script to the gate's calc list (`BOARD.md` names the gate and its list) so it runs every time.
- Existing: `critical.py` (layout constraints), `buck.py` (regulator design), `impedance.py` (diff pair Z), `pcb.py` (reader).
- Candidates when a board needs them: length/skew groups (DDR byte lanes, RGB, MIPI), DDR timing budget, I2C pull-ups and addresses, track current (IPC-2152), regulator dissipation, LDO dropout, crystal load caps, diff-pair layer/width.

## Vocabulary
| Term | Meaning |
|---|---|
| board profile | `hardware/<board>/` as above |
| generator | script that writes the KiCad schematic or board; the only place to edit them |
| net intent | the generator's net list; the net-intent check compares it with KiCad's netlist |
| datasheet card | `parts/<MPN>.yaml`: values with `section`; `null` value + `section: null` = not read yet; `null` value + `section: not stated` = datasheet silent |
| fab part number | the fab's stock number (JLCPCB: LCSC, key `lcsc`); empty = not assembled |
| basic part | fab library part without a setup fee (JLCPCB: Basic or preferred Extended) |
| alternate | substitute fab part number in `board.yaml` `alternates`, with a `note` of what else changes |
| critical block | parts and nets with a layout constraint in `critical.yaml` |
| `switcher_loop` | regulator input cap, switch pins and GND: smallest loop; inductor at the switch node |
| `feedback` | divider at the FB pin, short FB trace |
| `decoupling` | each cap at the supply pin it serves |
| `series_termination` | series R at the driving pin |
| `diff_pair` | pair on one layer: impedance, skew, via count |
| `thermal_distance` | heat source to sensor distance |
| return path | continuous reference plane under every signal (review; not a `critical.yaml` key) |
| keepout | no-copper area (antenna, shell, thermal island) as a DRC rule area (not a `critical.yaml` key) |
| gate | the board's check command; must print OK before commit and order |
| waiver | accepted ERC/DRC item with a reviewer's reason; never for the gate's never-waive list |
| known deviation | `known:` text on a constraint: accepted this revision, shown as WARN, with the fix |

## Gate
- OK means every check ran. A gate OK with `SKIP` lines (no KiCad installed) has not run ERC/DRC: rerun with KiCad before commit or order.
- `WARN` passes; `FAIL` and order-time `MISSING` stop.

## Review roles
- Narrow helper agents (small model): one image or one datasheet, one question, JSON answer (`hardware/agents/`). They read KiCad plots poorly: spell out the exact rule in the question.
- Whole-board review: a fresh-context strong model with the skills, before the human review.
- Humans: KiCad review, fab placement preview, payment, order.

## Writing hardware skills (KISS)
- One skill, one job. The description says when to use it; the body says how.
- Under ~150 lines. Detail goes to `references/` and is read only when needed.
- One rule per line, imperative, the reason in a few words.
- No part references, values, nets, paths or URLs of a board in a skill.
- Every rule names its check (script, DRC rule, gate) or says `(review)`.
- Use the vocabulary above; add a term here before using it elsewhere.
