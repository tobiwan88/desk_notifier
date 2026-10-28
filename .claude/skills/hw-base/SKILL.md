---
name: hw-base
description: Read first for any hardware/PCB task (KiCad schematic, layout, parts, ordering). Defines the board profile, the rule that every number comes from a script, the shared vocabulary, and who reviews what. The task skills are kicad-schematic, kicad-layout and hw-parts.
---

# Hardware base

Board-neutral. Facts about one board live in its profile, never in a skill.

## Folder contract
- `hardware/<board>/` = one board profile:
  - `board.yaml`: files, fab house, stackup, net classes, BOM alternates, regulator inputs.
  - `critical.yaml`: critical blocks by constraint type (see Vocabulary).
  - `parts/<MPN>.yaml`: one datasheet card per IC or critical passive.
  - `BOARD.md`: purpose, decisions and why, board-specific pitfalls, commands.
  - the KiCad project (generated: edit the generators named in `board.yaml`, not the KiCad files).
- `hardware/calc/`: board-neutral check scripts. Each takes the board folder.
- `hardware/agents/`: prompts for narrow helper agents.
- Start every task by reading `BOARD.md` and `board.yaml` of the board you work on.

## Rule: numbers come from scripts
- Any check with a number (distance, impedance, voltage, current, timing, length, stock) runs a script. Never estimate in the reply.
- Look in `hardware/calc/` first. If nothing fits, write a script and keep it.
- A script:
  - takes `hardware/<board>` as its argument; reads inputs from the profile; hard-codes nothing about a board
  - prints one row per check: `OK`, `WARN`, `FAIL` or `MISSING` + the numbers used; ends with `<name>: OK|FAILED`
  - exits non-zero on FAIL; `MISSING` = input not in the profile yet (fill it in, do not guess)
  - names the source of each formula or limit in a comment (datasheet + section, IPC-2141, IPC-2152, app note)
  - has a `--selftest` with one known answer when the formula is not trivial
  - uses the standard library, numpy/scipy and the `.kicad_pcb` reader in `calc/pcb.py`; no new framework
- Add the script to the gate's calc list (`BOARD.md` names the gate) so it runs every time, not once.
- Existing: `critical.py` (layout constraints), `buck.py` (regulator design), `impedance.py` (diff pair Z), `pcb.py` (reader).
- Candidates when a board needs them: length/skew groups (DDR byte lanes, RGB, MIPI), DDR timing budget, I2C pull-up and address map, track current (IPC-2152), thermal dissipation, LDO dropout, crystal load caps.

## Vocabulary (use these words in skills, profiles and reviews)
| Term | Meaning |
|---|---|
| board profile | `hardware/<board>/` as above |
| datasheet card | `parts/<MPN>.yaml`: values with section references, `null` when not read |
| critical block | parts and nets with a layout constraint, declared in `critical.yaml` |
| `switcher_loop` | input cap, switch pins and GND of a regulator: smallest loop; inductor at the switch node |
| `feedback` | divider at the FB pin, short FB trace away from the switch node |
| `decoupling` | each cap at the supply pin it serves |
| `series_termination` | series R at the driving pin |
| `diff_pair` | impedance, skew and via count of a pair |
| `thermal_distance` | heat source to sensor distance |
| `return_path` | continuous reference plane under every signal (review) |
| `keepout` | antenna, mechanical or thermal no-copper area (DRC rule area) |
| gate | the board's check command that must print OK before commit and order |
| known deviation | `known:` text on a constraint: accepted for this revision, shown as WARN, with the fix |

## Review roles
- Narrow helper agents (small model): one image or one datasheet, one question, JSON answer, prompts in `hardware/agents/`. They read KiCad plots poorly: spell out the exact rule in the question.
- Whole-board review: a fresh-context strong model with the skills, before the human review.
- Humans: KiCad review, fab placement preview, payment, order.

## Writing hardware skills (KISS)
- One skill, one job. The description says when to use it; the body says how.
- Under ~150 lines. Detail goes to `references/` and is read only when needed.
- One rule per line, imperative, the reason in a few words.
- No part references, values, nets or URLs of a board in a skill.
- Every rule names its check (script, DRC rule, gate) or says `(review)`.
- Use the vocabulary above; define a new term here before using it.
