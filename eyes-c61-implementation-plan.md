# HTTP-Controlled Animated Eyes on ESP32-C61 — Implementation Plan

**Version:** 2.0 (cat_eyes pivot) · **Date:** 2026-10-10 · **Status:** in execution — M0–M7 mostly implemented, see 'Implementation status'
**Execution model:** **Sonnet 5.5 coordinator** dispatches small vertical slices to **Haiku 5.5** workers with minimal context; **Sonnet 5.5** reviews; **Opus 5.5** freezes contracts and handles escalations.
**Language:** firmware in **modern C++ (C++23 subset, compiled as gnu++26)**; PC/Docker tooling in Python 3.
**Builds:** local only (no hosted CI). One script, `scripts/check.sh`, is the gate for every slice.

### Changes v1.1 → v1.2 (owner answers)

| Answer | Change |
|---|---|
| Two displays | **One eye per display**, both GC9A01 on one SPI bus (shared SCLK/MOSI/DC/RST, separate CS). New pin map §1.6. Engine renders two output channels. |
| esp32-eyes-like, but general | Eyes = **layered, data-driven `EyeStyle`** (sclera, iris, pupil, highlight, lids). Moods are presets on top. Built-in style presets `realistic` (default) and `robo`. API accepts style overrides and low-level lid control. |
| Simpler; prep in a Docker web app | **No web UI on the device.** New **`eyes-studio`** Docker container (prepare clips, preview, upload, control). Device API trimmed to **7 routes**. |
| Script prepares the easiest format | **Clip v1 = RAW565 only**, already cut into circle-trimmed bands by the PC side. The device just copies bytes into DMA buffers: no decoder. |
| No 80 MHz | SPI fixed at **≤ 40 MHz**; 80 MHz spike and IO_MUX option removed. |
| Freeze during upload OK | No busy screen; render simply pauses during flash writes. |
| Wi-Fi via build config or UART | Kconfig credentials + **UART console** (`wifi`, `token`, `hostname`, …). No SoftAP. |
| Home LAN security | Baseline only (token, limits, atomic uploads, rollback). |
| USB-powered, dim when asleep | `asleep` mood dims backlight; schedules live in Docker/home automation, not on the device. |
| Local builds | GitHub Actions dropped; `scripts/check.sh` locally. |
| Several devices | Default hostname `eyes-<MAC3>.local`, mDNS service `_eyes._tcp` for discovery by eyes-studio. |

### Changes v1.2 → v1.3 (display module datasheet from owner)

| Finding (F22) | Change |
|---|---|
| 7-pin module: `VCC GND SCL SDA DC CS RST`, **no backlight pin**; 2 white LEDs always on via on-board XC6206 3.3 V LDO + 2 Ω | Backlight GPIO removed (6 signals now). `brightness` and `asleep` work by scaling/blanking eye colours. Optional hardware mod for real dimming listed in §1.5. |
| **1 kΩ series resistors** on SCL, SDA, DC, CS, RST | SPI default **20 MHz** (safe); S2.3 sweeps 20 / 26.7 / 40 MHz. For 40 MHz bridge the SCL/SDA resistors (§1.5). Performance tables for 20 / 26.7 / 40 MHz. |
| On-board LDO, VCC input up to 6 V | Power both modules from the **5 V pin**: full backlight brightness, no load on the DevKit’s 3.3 V regulator. Logic stays 3.3 V. |
| RST has its own 10 kΩ pull-up + 100 nF on the module | Shared RST line still used (one reset pulse for both); could be dropped later to free a GPIO. |

### Changes v1.3 → v2.0 (cat_eyes pivot + hardware results)

| Change | Effect |
|---|---|
| Procedural `EyeStyle` / mood renderer (§3.3, §5.2: moods, lids, `StylePatch`) | **Replaced** by the ready-made C component `cat_eyes` (`firmware/components/cat_eyes`). The old sections stay in the document, marked superseded. |
| cat_eyes styles | `realistic` (default), `flat`, `line`, `anime`, `cute`, `halloween`, `christmas`. |
| cat_eyes iris colours | `amber`, `green`, `blue`, `copper`, `violet`, `odd`. |
| cat_eyes animations (12) | `wink`, `slow_blink`, `roll`, `side_eye`, `hunt`, `startle`, `nictitate`, `sleepy`, `cross_eyed`, `happy`, `glance`, `special`. Built-in random idle with saccades and blinks. |
| Device eye API is now very small | `POST /api/eye` accepts any of the keys `style`, `iris`, `play` (animation name, `blink` or `random`), `auto` (random idle on/off), `look` (`[gx, gy, hold_ms]`), `save`. |
| New `GET /api/animations` | Catalog plus current state; no auth. Status fields are embedded in the existing status output. |
| On-device web page | `GET /` serves a small page to pick style, iris and animation and to toggle random idle. Replaces decision D14 ("no on-device UI"). `eyes-studio` remains for clip preparation. |
| Clips kept as optional second source | Per display: RAW565 `.clip`, `PUT/GET/DELETE /api/clips`, `POST /api/play`, `POST /api/stop`. |
| Rendering model | cat_eyes renders full 240-px rows with dirty-row ranges (no circle trim). `halloween` and `christmas` redraw the full screen every frame. |
| Console | USB-Serial/JTAG, not UART0. |
| SPI interrupt flag | The esp_lcd SPI done-callback is not IRAM-safe, so `ESP_INTR_FLAG_IRAM` must **not** be set on the SPI bus. |
| SPI clock | GPIO-matrix SPI at 40 MHz is being evaluated visually (S2.3). |
| LittleFS read speed | Measured 3.2 MB/s. A6 is satisfied; the D9 fallback is not needed. |
| OTA | OTA with rollback verified on the board. |
| RAM | Internal RAM is the scarce resource. Wi-Fi/lwIP buffers moved to PSRAM, `SPIRAM_MALLOC_ALWAYSINTERNAL=256`. |
| PSRAM usage | Clip index, upload/OTA buffers, and Wi-Fi/lwIP. |

### v2.1 additions (random pool, easy clips, web page)

| Item | Status |
|---|---|
| SPI clock 40 MHz again (wiring fixed; the earlier glitches were cabling) | done |
| Random pool: user chooses which built-in animations and which clips random mode may pick, plus the pause range (`GET/POST /api/pool`, NVS key `pool`, `Scene` scheduler, `play:"random"` uses the pool) | done |
| `eyesctl add` (GIF/PNG/video → prepared with the device budget → upload, `--pool`), `eyesctl pool …`, studio proxy routes | done |
| Web page: Random pool checkboxes + interval, clip list (play/loop/delete/stop), `.clip` upload | done |
| **Web page: add an animation from a GIF without any tool.** The browser decodes the GIF (`ImageDecoder`), fits it to 240×240, encodes a `.clip` with a JavaScript port of clipprep (`web/ui/clip_encoder.js`, byte-identical to the Python writer, tested in Node), applies the same size budget, previews the result in a round canvas, uploads it, and can add it to the pool. Fallback text for browsers without `ImageDecoder`: use `eyesctl add`. | built and served by the board; the encoder is byte-identical to Python (pytest); browser path still to be tried by the owner |
| Later: clip rename, per-item weights in the pool, "play now" on pool items, date-based default style, upload of MP4 via `<video>` frame capture | planned |

### Implementation status

**Done**
- S0.2 skeleton + gate
- S1.1 / S1.2 gfx
- S2.1 / S2.2 display + strip pool
- S3.x old eye renderer (superseded by cat_eyes)
- S4.1 parser
- S4.3 net/console
- S4.4 web API
- S4.5 eyesctl
- S5.x clips (clipprep, clip core, storage, ClipPlayer, routes)
- S5.0 fs benchmark (3.2 MB/s)
- S6.1 / S6.2 studio
- S7.1 OTA + rollback

**Open**
- S2.3 clock sweep (20 / 26.7 / 40 MHz, visual)
- S7.2 power-cut test (owner)
- S7.3 24 h soak
- RAM headroom target: minimum free internal RAM >= 40 KB
- cat_eyes frame-time measurement per style (target < 33 ms)
- Optional date-based default style (halloween in October, christmas in December)

---

## Table of contents

0. [How to execute this plan with agents](#0-how-to-execute-this-plan-with-agents)
1. [Verified facts, decisions, assumptions, open points](#1-verified-facts-decisions-assumptions-open-points)
2. [Repository layout](#2-repository-layout)
3. [Modern C++ rules and component specs (frozen contracts)](#3-modern-c-rules-and-component-specs-frozen-contracts)
4. [RTOS / concurrency design](#4-rtos--concurrency-design)
5. [Data formats](#5-data-formats)
6. [Performance budget](#6-performance-budget)
7. [Milestones and slice backlog](#7-milestones-and-slice-backlog)
8. [Testing strategy](#8-testing-strategy)
9. [Risks and mitigations](#9-risks-and-mitigations)
10. [Security and robustness](#10-security-and-robustness)
11. [Effort estimate and first 1–2 hours checklist](#11-effort-estimate-and-first-12-hours-checklist)
12. [Flagged deviations from the given architecture](#12-flagged-deviations-from-the-given-architecture)
13. [Self-review log](#13-self-review-log)

---

## 0. How to execute this plan with agents

### 0.1 Roles

| Role | Model | Does | Never does |
|---|---|---|---|
| **Coordinator** | Sonnet 5.5 | Picks next slice (§7), builds context pack, dispatches worker, runs `scripts/check.sh` + slice `verify`, routes review, keeps `docs/agents/STATUS.md`, asks owner for hardware checks | Writes production code (except merge fixes ≤ 10 lines) |
| **Worker** | Haiku 5.5 | Implements one slice card: code + tests, runs `verify`, returns diff + short report | Edits frozen headers, touches files outside `writes:`, adds dependencies |
| **Reviewer** | Sonnet 5.5 | Checks diff vs card, frozen contract, §0.6; approves or returns ≤ 5 findings | Rewrites the slice |
| **Architect / escalation** | Opus 5.5 | S0.4 (contracts, `cpp-style.md`, red-slice cards), escalations, contract change requests | Routine implementation |
| **Owner** | human | Wiring, flashing, observing displays, measurements | — |

### 0.2 Why this works for Haiku

* **Frozen headers first** (S0.4, Opus); workers implement behind them.
* **`core/` = pure C++, no ESP-IDF includes**, built and tested on the PC. Many checks are `static_assert`s: if it compiles, it is right.
* **Thin `idf/` adapters** for SPI, Wi-Fi, httpd, LittleFS, OTA, console.
* **Machine-checkable gate per slice** (`verify:`) + optional owner check.
* **Context pack per slice** (`reads:`): ≤ 600 lines in, ≤ 250 lines changed.

### 0.3 Slice card template (`docs/agents/slices/<ID>.md`)

```markdown
# <ID> <title>
tier: green|amber|red
worker: haiku | sonnet
depends_on: [<IDs>]
reads:  [exact paths: frozen headers, docs/cpp-style.md, spec sections, fixtures]
writes: [exact paths]
goal: <one sentence>
contract: <functions/classes from which frozen header>
steps: [numbered, concrete]
tests_to_write: [file + named cases, incl. static_asserts]
verify: <shell command(s), must exit 0>
hil: <owner observation/measurement, or 'none'>
done_when: <objective criteria>
out_of_scope: <explicit>
```

### 0.4 Coordinator loop

```
for slice in ready_slices(order=§7):
    pack   = card + files(slice.reads)
    result = dispatch(worker_model(slice), pack)
    if run(slice.verify) != 0 or run("scripts/check.sh --quick") != 0: retry_or_escalate(slice)
    green → review batch (≤ 3 slices, same milestone); amber/red → review now
    findings → same worker (counts as attempt)
    if slice.hil: ask owner, record in STATUS.md
    commit "<ID>: <title>"   (local git)
```

### 0.5 Escalation rules

| Trigger | green | amber | red |
|---|---|---|---|
| `verify` fails after N worker attempts | N=3 → Sonnet worker | N=2 → Sonnet | N=1 → Sonnet |
| Sonnet worker also fails | → Opus | → Opus | → Opus |
| Reviewer rejects twice | → Sonnet worker | → Opus review | → Opus |
| “Contract insufficient” | → Opus CCR | same | same |
| Hardware result contradicts expectation | → Opus | same | same |
| Timing/race/DMA issue not reproducible on PC | → Opus | same | same |
| Template/compile error not fixed in 1 attempt | → Sonnet | same | same |

CCR (contract change request): only Opus edits frozen headers; coordinator re-runs `verify` for all approved slices that include the changed header.

### 0.6 Review checklist (Sonnet)

1. Only `writes:` paths touched; no frozen header edits without CCR.
2. Contract fully implemented; no new public symbols.
3. C++ rules §3.0: no exceptions/RTTI/`iostream`/`std::function`; no heap in `next()/fill()`; no virtual calls in ISR; errors via `std::expected`; `[[nodiscard]]`/`noexcept`; tables `constexpr`.
4. ISR callbacks: free functions, `IRAM_ATTR`, only `...FromISR` APIs.
5. Integer/fixed-point math per pixel.
6. Tests cover card edge cases; compile-time facts are `static_assert`s.
7. Config from `config.hpp`, not literals.
8. `ESP_LOGx` with tag; no `printf` in firmware.
9. Flag > 400 lines per slice or a firmware template instantiated > 4 times.

### 0.7 Tiers

**green** = `core/` or Python, PC-tested · **amber** = ESP-IDF APIs, no concurrency subtleties · **red** = DMA/ISR/concurrency/OTA (Opus-written card, 1-attempt escalation).

---

## 1. Verified facts, decisions, assumptions, open points

### 1.1 Verified facts (with sources)

| # | Fact | Source |
|---|---|---|
| F1 | ESP-IDF v6.0 moved ESP32-C61 from preview to full support. | [Espressif](https://www.espressif.com/en/node/10421), [CNX Software](https://www.cnx-software.com/2026/03/24/esp-idf-v6-0-framework-adds-support-for-esp32-c5-and-esp32-c61-preview-for-esp32-h21-and-esp32-h4/) |
| F2 | ESP-IDF **v6.1** released; install via `eim install -i v6.1` or `git clone -b v6.1 --recursive`. | [GitHub release v6.1](https://github.com/espressif/esp-idf/releases/tag/v6.1) |
| F3 | Safe bootloader OTA supported on C61 since v6.0. | [CNX Software](https://www.cnx-software.com/2026/03/24/esp-idf-v6-0-framework-adds-support-for-esp32-c5-and-esp32-c61-preview-for-esp32-h21-and-esp32-h4/) |
| F4 | Built-in cJSON removed from IDF 6.x → managed component. | [v6.0 protocols migration](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32c61/migration-guides/release-6.x/6.0/protocols.html) |
| F5 | At **≤ 40 MHz**, GPIO-matrix routing behaves the same as IO_MUX → any free GPIO can carry SPI. | [C61 SPI master v6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32c61/api-reference/peripherals/spi_master.html#gpio-matrix-and-io-mux) |
| F6 | DMA buffers: `MALLOC_CAP_DMA` internal RAM, 32-bit aligned address and length; `spi_bus_dma_memory_alloc()` handles alignment. | same page |
| F7 | C61 can DMA from PSRAM directly, sharing MSPI bandwidth (data loss possible if exceeded). | same page; [SPI LCD v6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32c61/api-reference/peripherals/lcd/spi_lcd.html) |
| F8 | `max_transfer_sz` defaults to 4092 B with DMA → set explicitly. | SPI master |
| F9 | Interrupt transaction ≈ 20 + 8n/F[MHz] µs; `CONFIG_SPI_MASTER_ISR_IN_IRAM` + `ESP_INTR_FLAG_IRAM` keep transfers running during flash ops. | SPI master “Transfer Speed Considerations” |
| F10 | esp_lcd SPI IO: `pclk_hz`, `trans_queue_depth`, `on_color_trans_done`, `user_ctx`. | [SPI LCD v6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32c61/api-reference/peripherals/lcd/spi_lcd.html) |
| **F21** | **An SPI master host has three CS lines → up to three devices on one bus**; the driver is thread-safe only if each device is used by one task. | [C61 SPI master v6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32c61/api-reference/peripherals/spi_master.html) (`spi_bus_add_device`, driver features) |
| F11 | `espressif/esp_lcd_gc9a01` v2.0.4, all targets. | [Registry](https://components.espressif.com/components/espressif/esp_lcd_gc9a01) |
| F12 | `joltwallet/littlefs` v1.22.3, all targets; old ESP32 benchmark read 440 KB in ~5.9 s. | [Registry](https://components.espressif.com/components/joltwallet/littlefs) |
| F13 | `espressif/mdns` v1.14.0, all targets. | [Registry](https://components.espressif.com/components/espressif/mdns) |
| F14 | DevKitC-1: GPIO7/8/9 strapping (GPIO8 = RGB LED), GPIO10/11 UART0, GPIO12/13 USB, GPIO14 PSRAM CS. | [DevKitC-1 v2.0](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c61/esp32-c61-devkitc-1/user_guide.html) |
| F15 | Pins on both board revisions: GPIO0–13, 22–24. | [v1.0](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c61/esp32-c61-devkitc-1/user_guide_v1.0.html), [v2.0](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c61/esp32-c61-devkitc-1/user_guide.html) |
| F16 | GPIO9 low at reset → download mode. | [esptool C61](https://docs.espressif.com/projects/esptool/en/latest/esp32c61/advanced-topics/boot-mode-selection.html) |
| F17 | Board supply ≥ 0.5 A at 5 V; J5 for module current measurement. | DevKitC-1 v2.0 |
| F18 | C61 320 KB SRAM; WROOM-1 8 MB flash, 2 MB PSRAM. | [Zephyr board page](https://docs.zephyrproject.org/latest/boards/espressif/esp32c61_devkitc/doc/index.html) |
| F19 | IDF compiles C++ as `-std=gnu++26` by default; exceptions and RTTI off by default. | [C++ Support C61 v6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32c61/api-guides/cplusplus.html) |
| F20 | `<iostream>` ≈ +200 KB; vtables are in flash, unusable while cache is off → no virtual calls in IRAM ISRs. | same page |
| **F22** | Display module: GC9A01, 1.28″, 240 × 240, normally black, 4-line SPI, 7-pin 2.54 mm header `VCC GND SCL SDA DC CS RST`; on-board XC6206P332MR LDO (VCC → 3.3 V); backlight = 2 white LEDs in parallel from the LDO output through 2 Ω, **not controllable**; **1 kΩ series resistors** on SCL/SDA/DC/CS/RST; RST has 10 kΩ pull-up + 100 nF. | Module datasheet and schematic (owner-provided, 2026-10-10) |

### 1.2 Decisions

| # | Decision |
|---|---|
| D1 | ESP-IDF v6.1 (latest v6.1.x at start; tag in `idf_version.txt`). |
| D2 | Deps: `esp_lcd_gc9a01 ^2.0.4`, `littlefs ^1.22.3`, `mdns ^1.14.0`, `cjson` (name verified in S0.2). |
| D3 | **Two GC9A01 on SPI2**, shared SCLK/MOSI/DC/RST, separate CS (F5, F21). **Default 20 MHz** because of the modules’ 1 kΩ series resistors (F22); S2.3 sweeps 20 / 26.7 / 40 MHz and sets the Kconfig default to the fastest stable value. |
| D4 | 3 strip buffers × 240 × 20 lines (28.8 KB), **shared by both displays**. |
| D5 | Circle trimming: device trims procedural eye strips; **clip rects are pre-trimmed by the PC side**. |
| D6 | RGB565 big-endian everywhere (wire order). |
| D7 | Own streaming OTA handler + rollback. |
| D8 | Wi-Fi credentials: Kconfig default, overridable over the UART console (stored in NVS). |
| D9 | Clips on LittleFS, gated by throughput spike S5.0; fallback raw partition + `esp_partition_mmap`. |
| D10 | Tests: CMake + doctest + `static_assert`; pytest; all run by `scripts/check.sh` locally. |
| D11 | C++23 subset (compiled gnu++26), no exceptions/RTTI, `-O2`. |
| D12 | Per-display source selection with `std::variant` + concept. |
| D13 | **Clip v1 = RAW565 only**; encoding field kept for later RLE if flash space runs short. |
| D14 | **No on-device web UI**; UI and clip preparation live in the `eyes-studio` Docker container, which talks to devices server-side (token never in a browser, no CORS). |

### 1.3 Assumptions (measured by slices)

| # | Assumption | Measured in |
|---|---|---|
| A1 | Free internal heap with Wi-Fi + httpd + mDNS ≥ 90 KB before our buffers. | S0.3 |
| A3 | Window setup waits for the previous queued color transfer (same or other display). | S2.3 |
| A4 | Each module draws ≤ 60 mA from 5 V (backlight + logic); 1 kΩ series resistors + ~10 pF input load (estimate) give ~10 ns edges → 20 MHz safe, 26.7 MHz likely, 40 MHz only with SCL/SDA resistors bridged. | S0.1 (current), S2.3 (clock sweep) |
| A6 | LittleFS sequential reads ≥ 3 MB/s with 16 KB reads (needed for full-screen RAW clips on both eyes at 15 fps). | S5.0 |
| A7 | Span renderer ≤ 15 cycles/pixel average. | S3.5 |
| A8 | Wi-Fi/lwIP buffers can go to PSRAM on C61. | S0.3 |
| A9 | Two `esp_lcd` panel IOs can share one DC GPIO (only one transaction is on the bus at a time). | S2.1 |

### 1.4 Install, build, gate

```bash
eim install -i v6.1
idf.py set-target esp32c61
idf.py add-dependency "espressif/esp_lcd_gc9a01^2.0.4"
idf.py add-dependency "joltwallet/littlefs^1.22.3"
idf.py add-dependency "espressif/mdns^1.14.0"
idf.py add-dependency "espressif/cjson"
idf.py build flash monitor                    # USB-to-UART port; console runs here too

scripts/check.sh          # = host tests (doctest+ASan/UBSan) + pytest (studio, clipprep) + idf.py build
scripts/check.sh --quick  # host tests + pytest only (for core/Python slices)
```

### 1.5 Open points (non-blocking; defaults apply if unanswered)

1. **Faster SPI (optional solder mod):** bridge the 1 kΩ resistors on **SCL and SDA only** on both modules (solder blob or 0 Ω; optionally 33 Ω on the ESP side). Expected to allow 40 MHz → blinks at 2× speed headroom and full-screen clips up to ~24 fps on both eyes. Without the mod the plan works at 20–26.7 MHz (§6.1). Default: no mod; decide after S2.3.
2. **Real backlight dimming (optional hardware mod):** lift the 2 Ω resistor (R6) from GND and switch the LED cathode with a small N-MOSFET (e.g. AO3400) driven by a PWM GPIO (GPIO1 is reserved for it as `EYES_LCD_PIN_BL`, default −1). Default: no mod; brightness = colour scaling.
3. **Flash capacity vs RAW-only clips** (§6.6): RAW keeps the ESP32 simplest but fits only ~1 long full-screen clip. Default: accept; add RLE (encoding 3) later only if you run out of space.

### 1.6 GPIO layout for two displays (Kconfig defaults; works on board v1.0 and v2.0)

Avoided: GPIO7/8/9 (strapping, RGB LED), 10/11 (UART0 = console), 12/13 (USB), 14 (PSRAM CS). Pin-JTAG on GPIO3–6 is not used (debug via USB).

| Signal | GPIO | Header pin | Goes to | Kconfig |
|---|---|---|---|---|
| SCLK | 6 | J1-5 | both displays `SCL` | `EYES_LCD_PIN_SCLK` |
| MOSI | 5 | J1-4 | both displays `SDA` | `EYES_LCD_PIN_MOSI` |
| DC | 3 | J1-13 | both displays `DC` | `EYES_LCD_PIN_DC` |
| RST | 2 | J1-12 | both displays `RST` | `EYES_LCD_PIN_RST` |
| CS left | 4 | J1-3 | left display `CS` | `EYES_LCD_PIN_CS_L` |
| CS right | 7 (owner's wiring; plan had 0) | – | right display `CS` | `EYES_LCD_PIN_CS_R` — see `docs/wiring.md` |
| **5V** | — | **J1-14** | both `VCC` (module LDO makes 3.3 V, F22) | — |
| GND | — | J1-15 | both `GND` | — |
| reserved | 1 | J1-8 | only for the optional backlight MOSFET mod | `EYES_LCD_PIN_BL` (default −1) |
| spare | 22, 23, 24 | J3-6, J3-5, J3-4 | debug frame toggle, button | `EYES_DEBUG_PIN` |

Module header order is `1 VCC · 2 GND · 3 SCL · 4 SDA · 5 DC · 6 CS · 7 RST` (F22).

```
DevKitC-1 J1                  Left module (7-pin)     Right module (7-pin)
 5V   (14) ──────────────┬──  1 VCC             ┌──  1 VCC
 GND  (15) ──────────────┼──  2 GND             ├──  2 GND
 GPIO6 (5)  SCLK ────────┼──  3 SCL             ├──  3 SCL
 GPIO5 (4)  MOSI ────────┼──  4 SDA             ├──  4 SDA
 GPIO3 (13) DC ──────────┼──  5 DC              ├──  5 DC
 GPIO2 (12) RST ─────────┴──  7 RST             └──  7 RST
 GPIO4 (3)  CS_L ───────────  6 CS
 GPIO7      CS_R ─────────────────────────────────── 6 CS   (see docs/wiring.md)
```

Wiring notes: keep shared SCL/SDA wires short (< 10 cm per branch), daisy-chain rather than long star. The modules’ 1 kΩ series resistors (F22) already damp ringing, but also slow the edges — hence the 20 MHz default and the optional SCL/SDA bridge mod (§1.5). GPIO0/1 are the 32 kHz crystal pins; the DevKitC-1 has no such crystal, so they are free.

**Power:** feed both modules from **5 V** (USB rail): the backlight then gets the full 3.3 V from each module’s own LDO, and the DevKit’s 3.3 V regulator only supplies the ESP32 module. Budget: ESP module ~360 mA TX peaks (3.3 V rail) + 2 × ≤ 60 mA displays (5 V rail, A4, measured in S0.1) → use a USB supply ≥ 1 A (board minimum 0.5 A, F17). Add ≥ 47 µF + 100 nF near the displays’ VCC if wires are long. Logic levels are fine: the GC9A01 runs at 3.3 V from the module LDO, same as the ESP32.

**Brightness:** the backlight is always on (F22). `brightness` (0–100) scales the eye colours in the renderer; `asleep` closes the lids (black on a normally-black panel). Clips are shown at full brightness. With the optional MOSFET mod (§1.5) `brightness` drives real PWM instead.

**Compile-time checks (`main/config.hpp`)** reject reserved pins, duplicate pins (all signals distinct), SPI clock > 40 MHz, and DMA lengths not divisible by 4.

---

## 2. Repository layout

```
eyes/
├── firmware/
│   ├── CMakeLists.txt  idf_version.txt  sdkconfig.defaults  partitions.csv
│   ├── main/            Kconfig.projbuild  idf_component.yml  config.hpp  app_main.cpp
│   ├── components/
│   │   ├── gfx/         core/ rect, strip iterator, circle chords, Rgb565, Fixed, LUTs
│   │   ├── display/     idf/  SPI bus, 2 × GC9A01, StripPool, backlight
│   │   ├── eyes/        core/ EyeStyle, moods, layout, anim, span renderer, diff, EyesModel, EyeView
│   │   ├── clip/        core/ format structs + validation   idf/ ClipPlayer (file reads)
│   │   ├── engine/      core/ FrameSource concept, Commands, JSON parsing, Scene   idf/ render task
│   │   ├── storage/     idf/  LittleFS, clip list/open/remove, atomic Upload
│   │   ├── net/         idf/  Wi-Fi STA, mDNS, UART console
│   │   └── web/         idf/  httpd, auth, 7 routes, OTA
│   └── test/
│       ├── host/        CMake + doctest for all */core
│       ├── golden/      sample .clip files + expected renders (CRC)
│       └── device/      small IDF test apps (pool, patterns, heap, fs_bench)
├── studio/              # eyes-studio Docker web app
│   ├── Dockerfile  docker-compose.yml  pyproject.toml
│   ├── eyes_studio/     app.py (FastAPI), devices.py (mDNS + manual list), static/ (one HTML page + JS)
│   └── tests/
├── clipprep/            # Python package used by studio and CLI: GIF/PNG/video → .clip + preview
│   ├── clipprep/        load.py  frames.py  bands.py  writer.py  reader.py  preview.py  budget.py  cli.py
│   └── tests/
├── tools/               eyesctl.py (CLI + soak), mask_calc.py
├── scripts/check.sh     # the single local gate
└── docs/                PLAN.md  cpp-style.md  hw-notes.md  spec/{clip-format.md, api.md, eye.schema.json}
                         agents/{STATUS.md, CCR/, slices/}
```

Dependency rule: `*/core/` includes only the standard library, other `core` headers and cJSON; `check.sh` fails on `#include "esp_` or `freertos/` under `core/`.

---

## 3. Modern C++ rules and component specs (frozen contracts)

### 3.0 Modern C++ rules (→ `docs/cpp-style.md`)

**Baseline:** C++23 subset compiled as gnu++26 (F19); `-fno-exceptions -fno-rtti`; `noexcept` everywhere; `[[nodiscard]]` on results; C++26 library features only behind `__cpp_lib_*` checks.

**Use, with the embedded payoff:**

| Feature | Where | Payoff |
|---|---|---|
| `consteval` | `rgb565(0xFF8800)`, `"#ff8800"_rgb`, pin validation | invalid colors/pins are compile errors, zero runtime cost |
| `constexpr` tables | easing LUT, AA coverage LUT, circle chord table, mood presets, default styles | data in flash, no init code, no RAM |
| `static_assert` as tests | chord sums, struct sizes, table sizes, `FrameSource<T>` checks, `Command` trivially copyable | instant gates for workers |
| Concepts | `FrameSource` | readable errors; contract checked at compile time |
| `std::variant` + `std::visit` | per-display active source (`EyeView` or `ClipPlayer`), `Command` | runtime switching without heap or vtables |
| Templates + `if constexpr` | `StripPool<N, Lines>`, `Fixed<Frac>`, renderer layer flags | sizes and branches resolved at compile time |
| `std::expected<T, Err>` | all fallible functions | no exceptions, results can’t be ignored |
| `std::span`, `std::string_view`, `std::array`, `std::optional` | buffers, names, tables, command fields | no allocation, bounds-carrying |
| `std::byteswap`, `std::bit_cast`, `std::to_underlying` | pixel order, header parsing, enum indexing | no UB-prone casts |
| Designated initializers | config, style presets, IDF structs | readable, order-checked |
| `[[likely]]`, `[[gnu::always_inline]]` | span-fill inner loops only | faster hot path |
| RAII wrappers | `Semaphore`, `Queue<T,N>`, `File` | no leaks on error paths |
| `StaticVec<T,N>`, `StaticString<N>` | fixed-capacity containers | no heap |

**Avoid:** exceptions, RTTI; `<iostream>` (~200 KB, F20), `std::format`, `<regex>`; virtual calls or `std::function` in ISR/IRAM code (F20); heap containers in render paths; `std::thread` for our tasks (need explicit priority/stack); coroutines; global objects whose constructors call IDF (use `constinit` or explicit `init()`); floats in per-pixel code (floats fine per frame).

**Kconfig → C++ (`main/config.hpp`, sketch):**

```cpp
namespace cfg {
inline constexpr std::array<int, 7> kLcdPins = { CONFIG_EYES_LCD_PIN_SCLK, CONFIG_EYES_LCD_PIN_MOSI,
    CONFIG_EYES_LCD_PIN_DC, CONFIG_EYES_LCD_PIN_RST, CONFIG_EYES_LCD_PIN_CS_L,
    CONFIG_EYES_LCD_PIN_CS_R, CONFIG_EYES_LCD_PIN_BL };     // BL = −1 unless the MOSFET mod is fitted
inline constexpr bool kHasBacklightPwm = CONFIG_EYES_LCD_PIN_BL >= 0;
inline constexpr int kSpiHz = CONFIG_EYES_LCD_SPI_HZ;
inline constexpr std::size_t kStripLines = CONFIG_EYES_STRIP_LINES, kStripCount = CONFIG_EYES_STRIP_COUNT;

consteval bool reserved(int p) { return p >= 7 && p <= 14; }          // F14, F16 (7..14 all reserved here)
consteval bool all_distinct(const auto& a) {
    for (std::size_t i = 0; i < a.size(); ++i) for (std::size_t j = i + 1; j < a.size(); ++j)
        if (a[i] >= 0 && a[i] == a[j]) return false;
    return true; }
static_assert(std::ranges::none_of(kLcdPins, reserved), "LCD pin on strapping/UART/USB/PSRAM pin");
static_assert(all_distinct(kLcdPins), "two LCD signals on the same GPIO");
static_assert(kSpiHz <= 40'000'000, "max 40 MHz on GPIO-matrix pins (F5)");
static_assert((240 * kStripLines * 2) % 4 == 0, "DMA length must be a multiple of 4 (F6)");
}
```

**ISR pattern:**

```cpp
extern "C" IRAM_ATTR bool on_color_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* ctx) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(ctx), &woken);   // same semaphore for both panels
    return woken == pdTRUE;
}
```

### 3.1 `gfx` (core)

```cpp
namespace gfx {
struct Rect { std::int16_t x{}, y{}, w{}, h{};
    constexpr bool empty() const noexcept { return w <= 0 || h <= 0; }
    constexpr std::int32_t area() const noexcept { return empty() ? 0 : std::int32_t{w} * h; } };
constexpr Rect unite(Rect, Rect) noexcept;  constexpr Rect intersect(Rect, Rect) noexcept;
constexpr Rect clip_screen(Rect) noexcept;

inline constexpr int kScreen = 240;
inline constexpr std::array<std::pair<std::int16_t,std::int16_t>, kScreen> kChord = make_chords(); // integer isqrt
constexpr std::int32_t trimmed_pixels(int strip_lines) noexcept;   // static_assert(trimmed_pixels(20) == 48'880)

class Strips { public:                       // splits r into strips of ≤ lines rows, optional circle trim
    constexpr Strips(Rect r, int lines, bool trim) noexcept;
    struct It { constexpr Rect operator*() const; constexpr It& operator++();
                constexpr bool operator==(std::default_sentinel_t) const; };
    constexpr It begin() const; constexpr std::default_sentinel_t end() const; };

using Rgb565be = std::uint16_t;
consteval Rgb565be rgb565(std::uint32_t rgb888) noexcept;
constexpr Rgb565be rgb565_rt(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept;
namespace literals { consteval Rgb565be operator""_rgb(const char*, std::size_t); }
constexpr Rgb565be blend(Rgb565be a, Rgb565be b, std::uint8_t alpha) noexcept;   // for AA edges
template <int Frac> struct Fixed { std::int32_t raw; /* constexpr ops, lerp */ };
inline constexpr std::array<std::uint8_t, 257> kEaseInOut = make_ease_lut();
inline constexpr std::array<std::uint8_t, 16>  kEdgeAlpha = make_edge_lut();     // sub-pixel coverage → alpha
}
```

### 3.2 `engine` core — sources, commands, scene

```cpp
namespace engine {
enum class Err : std::uint8_t { Ok, Unauthorized, BadJson, TooLarge, NotFound, BadName, BadClip,
                                NoSpace, Busy, Conflict, LengthRequired, Io, Ota, DisplayStall };
enum class Eye : std::uint8_t { Left = 0, Right = 1 };
inline constexpr std::size_t kEyes = 2;

struct FramePlan { StaticVec<gfx::Rect, 8> dirty; bool full_redraw = false; bool trim = true;
                   std::uint32_t delay_ms = 33; bool finished = false; };

template <class S>
concept FrameSource = requires(S s, std::uint32_t now, gfx::Rect r, std::span<gfx::Rgb565be> buf) {
    { s.next(now) }    noexcept -> std::same_as<FramePlan>;
    { s.fill(r, buf) } noexcept -> std::same_as<std::expected<void, Err>>;   // buf.size() == r.w * r.h
    { s.stop() }       noexcept;
    { S::kName }       -> std::convertible_to<std::string_view>;
};
```
Rules: `next()` once per frame before `fill()`; `fill()` calls in increasing y then x and never spanning two dirty rects; same content between two `next()` calls. `trim=false` means rects are sent as-is (clips are pre-trimmed by clipprep).

```cpp
enum class EyeSel : std::uint8_t { Both, Left, Right };

struct LidCmd   { std::optional<std::uint8_t> top, bottom; std::optional<std::int8_t> tilt; };   // 0..255, −127..127
struct EyeCmd {
    std::optional<eyes::Mood> mood;
    std::optional<std::array<std::int16_t, 2>> look_q8;          // −256..256
    std::optional<EyeSel> blink;                                  // Both = blink, Left/Right = wink
    std::optional<bool> auto_blink, auto_look;
    std::optional<std::uint8_t> pupil;                            // 64..255
    std::optional<LidCmd> lids;                                   // explicit lid override (general control)
    std::optional<eyes::StylePatch> style; bool save_style = false;
    std::optional<std::uint8_t> brightness;                       // 0..100
    std::optional<std::uint16_t> transition_ms; bool interrupt = false;
};
struct PlayCmd { std::array<StaticString<31>, kEyes> clip; bool loop = false; bool then_eyes = true; };  // empty = keep eyes on that display
struct StopCmd {};
using Command = std::variant<EyeCmd, PlayCmd, StopCmd>;
static_assert(std::is_trivially_copyable_v<Command>);            // goes through a FreeRTOS queue

struct ParseError { Err code; StaticString<63> message; };
[[nodiscard]] std::expected<EyeCmd, ParseError>  parse_eye (std::string_view json) noexcept;
[[nodiscard]] std::expected<PlayCmd, ParseError> parse_play(std::string_view json) noexcept;
[[nodiscard]] constexpr bool clip_name_valid(std::string_view) noexcept;   // ^[a-z0-9_-]{1,31}$

// Per-display channel + scene logic (pure, PC-testable with fakes)
template <FrameSource... Ss>
class Scene { public:
    void apply(const Command&, std::uint32_t now) noexcept;
    FramePlan next(Eye, std::uint32_t now) noexcept;
    std::expected<void, Err> fill(Eye, gfx::Rect, std::span<gfx::Rgb565be>) noexcept;
    std::string_view current(Eye) const noexcept; };
}
```
Scene behavior: each display has an active source (`EyeView` or `ClipPlayer`). `PlayCmd` starts a clip on the named display(s); other display keeps the eyes. When a clip finishes and `then_eyes`, that display returns to its eye with `full_redraw`. `EyeCmd` while a clip runs on a display is applied to the shared eye model (visible after the clip) unless `interrupt` stops clips first. New targets mid-transition retarget from the current interpolated state.

### 3.3 `eyes` (core) — general, layered eye model

> **Superseded in v2.0** by the cat_eyes component — see 'Changes v1.3 → v2.0'.

**Layer stack per eye (drawn per row, back to front):** background → sclera → iris (with optional ring) → pupil (round or slit) → highlight → upper/lower lids (bg or skin colour, curved edge, tilt).

```cpp
namespace eyes {
enum class Mood : std::uint8_t { Neutral, Happy, Angry, Sleepy, Scared, Curious, Asleep, Count };
enum class PupilShape : std::uint8_t { Round, Slit };

struct EyeStyle {                                   // everything drawable is data
    gfx::Rgb565be bg;
    struct { bool on; std::int16_t w, h, radius; gfx::Rgb565be color; } sclera;   // radius = w/2 → circle
    struct { bool on; std::int16_t r, ring_w; gfx::Rgb565be color, ring; } iris;
    struct { bool on; std::int16_t r; PupilShape shape; std::int16_t slit_w; gfx::Rgb565be color; } pupil;
    struct { bool on; std::int16_t r, dx, dy; gfx::Rgb565be color; } highlight;
    struct { gfx::Rgb565be color; std::int16_t curve; } lid;                     // curve = sag of lid edge (px)
    struct { std::int16_t max_x, max_y; } look;                                  // iris travel (px)
};
struct StylePatch { /* every field std::optional, applied over the current style */ };

struct MoodShape { std::uint8_t lid_top, lid_bot; std::int8_t tilt; std::uint8_t pupil, scale; };

inline constexpr EyeStyle kRealistic = { /* §3.3.1 */ };
inline constexpr EyeStyle kRobo      = { /* §3.3.1 */ };
inline constexpr std::array<MoodShape, std::to_underlying(Mood::Count)> kMoods = { /* §3.3.2 */ };

struct State { Mood mood; std::int16_t look_x, look_y; std::uint16_t blink_l, blink_r;   // 0 open … 256 closed
               std::uint8_t pupil; std::optional<engine::LidCmd> lid_override; };
struct EyeGeom { /* resolved per-eye geometry: centers, radii, lid edge rows/tilt, mirrored for right eye */ };

[[nodiscard]] constexpr EyeGeom layout(const EyeStyle&, const State&, engine::Eye) noexcept;
[[nodiscard]] constexpr StaticVec<gfx::Rect, 3> diff(const EyeGeom& a, const EyeGeom& b) noexcept;  // iris box, lid bands
void render(const EyeStyle&, const EyeGeom&, gfx::Rect r, std::span<gfx::Rgb565be> out) noexcept;   // hot path

class EyesModel {                       // one shared model → both eyes stay in sync
public:
    void start(std::uint32_t seed, const EyeStyle&) noexcept;
    void apply(const engine::EyeCmd&, std::uint32_t now) noexcept;
    void tick(std::uint32_t now) noexcept;                       // idle behaviour, easing (once per frame)
    const EyeGeom& geom(engine::Eye) const noexcept; const EyeStyle& style() const noexcept; };

class EyeView {                         // FrameSource for one display, reads the shared model
public: static constexpr std::string_view kName = "eyes";
    EyeView(EyesModel&, engine::Eye) noexcept;
    engine::FramePlan next(std::uint32_t now) noexcept;          // diff(prev geom, current geom)
    std::expected<void, engine::Err> fill(gfx::Rect, std::span<gfx::Rgb565be>) noexcept;
    void stop() noexcept; };
}
```

**Renderer = span renderer (fast and simple):** for each row, compute the horizontal x-span of each layer with one integer `isqrt` (or rounded-rect formula) per layer; fill span interiors with 32-bit writes of the layer colour; only the 1–2 edge pixels per span get `blend()` with an alpha from `kEdgeAlpha` (fractional part of the edge). Lids: per row, the lid edge y is a parabola in x (`curve`, `tilt`) evaluated only inside the lid band; rows fully covered by a lid are one span. Right eye = mirrored geometry (x → 239 − x) so look/tilt stay symmetric.

**3.3.1 Style presets (starting values, tuned with the owner in S3.2):**

| Field | `realistic` (default) | `robo` |
|---|---|---|
| bg | `#000000` | `#000000` |
| sclera | on, 208 × 208, radius 104, `#f2f2f2` | on, 150 × 170, radius 36, `#20e0ff` |
| iris | on, r 52, ring 4 px, `#2a7fd4` / ring `#0b2f55` | off |
| pupil | on, r 24, round, `#000000` | off |
| highlight | on, r 10 at (−18, −20) from pupil, `#ffffff` | off |
| lid | `#000000`, curve 24 px | `#000000`, curve 0 (straight) |
| look travel | ±44 x, ±36 y (iris stays inside sclera) | ±30 x, ±24 y (whole shape moves) |

**3.3.2 Mood presets (applied on top of any style):**

| Mood | lid_top /255 | lid_bot /255 | tilt (+ = outer corner up) | pupil /255 | scale % |
|---|---|---|---|---|---|
| Neutral | 30 | 0 | 0 | 140 | 100 |
| Happy | 20 | 110 (cheek) | 0 | 150 | 100 |
| Angry | 95 | 0 | +40 | 110 | 96 |
| Sleepy | 140 | 30 | −10 | 130 | 100 |
| Scared | 0 | 0 | −20 | 80 | 108 |
| Curious | L 0 / R 60 | 0 | 0 | 160 | L 106 / R 94 |
| Asleep | 255 | 0 | 0 | — | 100 (lids closed → black panel; real dimming only with the backlight mod) |

Blink: lid_top → 255 → back over 150 ms; wink = one eye only. Idle: auto-blink every 2.5–6 s, auto-look every 1.5–4 s (xorshift32), both overridable.

### 3.4 `display` (idf)

```cpp
namespace display {
struct StripBuf { std::span<gfx::Rgb565be> px; std::uint8_t idx; };
template <std::size_t N, std::size_t Lines> class StripPool;    // FIFO ring + counting semaphore

[[nodiscard]] std::expected<void, engine::Err> init() noexcept;  // bus, 2 panel IOs (shared DC), 1 reset pulse, panels blanked
void set_backlight(std::uint8_t pct) noexcept;                   // no-op unless cfg::kHasBacklightPwm (mod)
[[nodiscard]] std::expected<StripBuf, engine::Err> acquire(std::uint32_t timeout_ms) noexcept;
[[nodiscard]] std::expected<void, engine::Err> push(engine::Eye, StripBuf&&, gfx::Rect) noexcept;
[[nodiscard]] std::expected<void, engine::Err> wait_idle(std::uint32_t timeout_ms) noexcept;
[[nodiscard]] std::expected<void, engine::Err> fill(engine::Eye, gfx::Rgb565be) noexcept;
}
```
Bus: `max_transfer_sz = 240 * Lines * 2` (F8), `SPI_DMA_CH_AUTO`, `ESP_INTR_FLAG_IRAM`, `CONFIG_SPI_MASTER_ISR_IN_IRAM=y` (F9). Two `esp_lcd_new_panel_io_spi` (CS_L, CS_R, same DC pin, same semaphore as `user_ctx`); GC9A01 panel #1 gets `reset_gpio_num = RST`, panel #2 gets −1 (both reset by the same pulse). Because the backlight is always on (F22), `init()` fills both panels black *before* `DISPON` (0x29), so no garbage is visible at boot. Only the render task calls `acquire/push`.

### 3.5 `clip` (core + idf)

```cpp
namespace clip {
#pragma pack(push, 1)
struct Header { char magic[4]; std::uint16_t version, header_size, width, height;
                std::uint8_t pixel_format, flags; std::uint16_t frame_count, key_color, palette_count;
                std::uint32_t index_offset, palette_offset, data_crc32; };
struct IndexEntry { std::uint32_t offset, length; std::uint16_t duration_ms, flags; };
struct RectHdr { std::uint16_t x, y, w, h; std::uint8_t encoding, reserved; std::uint32_t payload_len; };
#pragma pack(pop)
static_assert(sizeof(Header) == 32 && sizeof(IndexEntry) == 12 && sizeof(RectHdr) == 14);

[[nodiscard]] std::expected<Header, engine::Err> parse_header(std::span<const std::byte>) noexcept;
[[nodiscard]] std::expected<RectHdr, engine::Err> parse_rect(std::span<const std::byte>) noexcept;
[[nodiscard]] std::expected<void, engine::Err> validate(/* file reader callback */) noexcept;   // at upload commit
[[nodiscard]] constexpr std::uint32_t crc32(std::uint32_t, std::span<const std::byte>) noexcept;

class ClipPlayer {                    // idf/; one instance per display
public: static constexpr std::string_view kName = "clip";
    std::expected<void, engine::Err> start(std::string_view name, bool loop) noexcept;
    engine::FramePlan next(std::uint32_t now) noexcept;          // trim = false
    std::expected<void, engine::Err> fill(gfx::Rect, std::span<gfx::Rgb565be>) noexcept;  // fread straight into buf
    void stop() noexcept; };
}
```
ClipPlayer: `start()` reads the header and the whole index into PSRAM. `next()` reads the frame’s rect headers (≤ 8 rects). `fill(strip)` = one `fread` of `strip.w × strip.h × 2` bytes **directly into the DMA strip buffer** (rect rows are contiguous; a strip of a rect is a contiguous byte range). No decoding at all. Any error → `finished = true` → eyes.

### 3.6 `engine` idf — render task

```cpp
namespace engine {
[[nodiscard]] std::expected<void, Err> start_render_task() noexcept;
[[nodiscard]] std::expected<void, Err> post(const Command&, std::uint32_t timeout_ms) noexcept;
struct Stats { std::uint16_t fps_x10; std::uint32_t frame_us_avg, frame_us_max; std::array<std::string_view, kEyes> source; };
Stats stats() noexcept;
}
```
Loop per frame: drain queue → `scene.apply` → `model.tick(now)` → **for eye in {Left, Right}:** `plan = scene.next(eye)` → for each rect → `for (Rect s : Strips(r, kStripLines, plan.trim))` → `acquire` → `scene.fill(eye, s, buf)` → `push(eye, …)` → **`wait_idle()` before switching to the other display** (keeps buffer return strictly FIFO; costs ≤ 1 strip time: 3.8 ms at 20 MHz, 1.9 ms at 40 MHz) → TWDT reset → stats → `vTaskDelayUntil`.

### 3.7 `storage`, `net`, `web`

```cpp
namespace storage {
std::expected<void, engine::Err> init() noexcept;              // mount /fs, /fs/clips, delete stale .tmp-*
struct ClipInfo { StaticString<31> name; std::uint32_t size; std::uint16_t frames, fps_x10; };
std::expected<StaticVec<ClipInfo, 64>, engine::Err> list() noexcept;
std::expected<File, engine::Err> open(std::string_view) noexcept;
class Upload { public:                                          // RAII: unlinks tmp unless committed
    static std::expected<Upload, engine::Err> begin(std::string_view name, std::uint32_t total) noexcept;
    std::expected<void, engine::Err> write(std::span<const std::byte>) noexcept;
    std::expected<void, engine::Err> commit() noexcept; };     // validate + CRC + atomic rename (replaces)
std::expected<void, engine::Err> remove(std::string_view) noexcept;   // Conflict if playing
std::uint32_t free_bytes() noexcept;
}
namespace net {
std::expected<void, engine::Err> init() noexcept;               // NVS creds, else Kconfig creds; reconnect backoff
std::expected<void, engine::Err> start_mdns() noexcept;         // hostname eyes-<MAC3> (or NVS), service _eyes._tcp
std::expected<void, engine::Err> start_console() noexcept;      // UART0 REPL, §5.6
bool connected() noexcept; std::int8_t rssi() noexcept;
}
namespace web { std::expected<void, engine::Err> start() noexcept; }   // §5.3, OTA §10
```

### 3.8 `eyes-studio` (Docker) and `clipprep` (Python)

**clipprep** (library + CLI; the only place where clips are made):

```python
def prepare(src: Path, *, eye_region: Literal["full"] | tuple[int,int,int,int] = "full",
            fps: float | None = None, max_frames: int | None = None,
            budget: Budget = Budget(max_bytes=1_500_000, max_bps=2_000_000),
            fit: Literal["cover","contain"] = "cover", bg: str = "#000000") -> PreparedClip
    # 1 load GIF / PNG sequence / video (ffmpeg → PNG frames)
    # 2 resize/crop to 240×240 (or the region), apply circle mask, convert to RGB565 BE
    # 3 diff consecutive frames → changed pixels → circle-trimmed horizontal bands (≤ 8 rects/frame, non-overlapping)
    # 4 RAW565 payloads; first frame KEYFRAME (full trimmed circle)
    # 5 enforce budget: lower fps / frame count / region until size and bytes-per-second fit; report decisions
def write_clip(p: PreparedClip, out: Path) -> None
def read_clip(path: Path) -> list[np.ndarray]          # reference reader for tests/preview
def preview_gif(p: PreparedClip, out: Path) -> None    # exactly what the device will show (circle mask, RGB565 quantisation)
```

**eyes-studio** (FastAPI, one static HTML page, no build step):

| Endpoint (studio) | Does |
|---|---|
| `GET /` | single page: device picker, eye controls (mood, look pad, blink/wink, style preset/colours, brightness), clip library |
| `GET /api/devices` | devices from mDNS `_eyes._tcp` + `EYES_DEVICES` env list; status of each |
| `POST /api/prepare` | upload GIF/PNG-zip/video + options → runs clipprep → returns preview GIF, size, fps, budget notes |
| `POST /api/devices/{id}/clips/{name}` | streams the prepared .clip to the device (`PUT /api/clips/<name>`) |
| `POST /api/devices/{id}/eye`, `/play`, `/stop`, `DELETE …/clips/{name}` | server-side proxy to the device (token from env/secret) |

Docker: `python:3.12-slim` + `ffmpeg`; `network_mode: host` (needed for mDNS discovery; without it, set `EYES_DEVICES=eyes-1a2b3c.local,192.168.1.50`); device token via env file. Device budget (`fs_free`, `clip_max_bps`) read from `/api/status`.

---

## 4. RTOS / concurrency design

### 4.1 Tasks (single core)

| Task | Prio | Stack | Notes |
|---|---|---|---|
| Wi-Fi / esp_timer / lwIP (IDF) | 23 / 22 / 18 | IDF | |
| **render** (scene + both displays) | **8** | 6 KB | blocks on strip semaphore / `vTaskDelayUntil` |
| httpd | 5 | 8 KB | parse, post command, stream uploads/OTA |
| console (UART REPL) | 2 | 4 KB | rare use |
| mDNS | default | default | |

### 4.2 Synchronization

| Object | Type | Path | Depth |
|---|---|---|---|
| command queue | `Queue<Command, 8>` | httpd/console → render | full → 503 |
| strip semaphore | counting | ISR (both panels) → render | N strips |
| stats | struct + `portMUX` copy | render → httpd | — |
| playing clip names | 2 × name + atomic flag | scene → storage | — |

### 4.3 Buffer ownership

Pool ring [B0][B1][B2]; one owner at a time (pool → render → DMA → pool via ISR). Within one display, transfers complete in queue order. **Before switching from the left to the right display, render calls `wait_idle()`**, so returns stay FIFO across displays without per-panel bookkeeping. `push` takes `StripBuf&&` (moved-from buffer can’t be reused). `wait_idle()` also before source switches.

### 4.4 Frame pacing

30 fps target (`MIN_FRAME_MS = 33`), 10 fps idle, clips at their `duration_ms` (≥ 33 ms effective; if both displays play clips the slower one sets the pace). `vTaskDelayUntil`; on overrun reset reference. `CONFIG_FREERTOS_HZ=1000`.

### 4.5 Watchdogs and recovery

TWDT on render (5 s); `acquire` timeout 100 ms, 3 in a row → display re-init; httpd recv/send timeouts 10 s; panic → reboot → eyes; OTA rollback §10. Flash writes (upload/OTA) pause rendering — accepted.

---

## 5. Data formats

### 5.1 `.clip` v1 (integers little-endian, pixels RGB565 big-endian)

**Header (32 bytes):**

| Off | Size | Field | Value |
|---|---|---|---|
| 0 | 4 | magic | `"EYCL"` |
| 4 | 2 | version | 1 |
| 6 | 2 | header_size | 32 |
| 8 | 2 | width | 240 |
| 10 | 2 | height | 240 |
| 12 | 1 | pixel_format | 1 = RGB565_BE |
| 13 | 1 | flags | bit0 LOOP default, bit1 HAS_KEY (reserved for overlay), bit3 CIRCLE_TRIMMED (must be 1 in v1) |
| 14 | 2 | frame_count | 1..4096 |
| 16 | 2 | key_color | reserved for overlay |
| 18 | 2 | palette_count | 0 in v1 |
| 20 | 4 | index_offset | absolute |
| 24 | 4 | palette_offset | 0 in v1 |
| 28 | 4 | data_crc32 | CRC-32 (zlib) over bytes [32, EOF) |

**Index:** `frame_count × 12 B` = {offset u32, length u32, duration_ms u16, flags u16 (bit0 KEYFRAME)}.
**Frame:** {rect_count u16 (0..8; 0 = hold), reserved u16} + rects.
**Rect:** {x, y, w, h u16; encoding u8 (= 0 RAW565 in v1); reserved u8; payload_len u32 (= w·h·2); payload}. Rects sorted by y then x, non-overlapping, inside the 120 px circle (pre-trimmed bands).
**Reserved for later:** encodings 1–255 (e.g. 3 = RLE565 if flash space runs out), palette, transparent key (overlay).
**Device validation at commit:** magic, version 1, sizes, pixel_format 1, CIRCLE_TRIMMED set, frames/rects inside file and screen, encoding 0, payload_len = w·h·2, CRC.

### 5.2 Eye command schema (`docs/spec/eye.schema.json`, abbreviated)

> **Superseded in v2.0** by the cat_eyes component — see 'Changes v1.3 → v2.0'.

```json
{
  "type": "object", "additionalProperties": false,
  "properties": {
    "mood":  { "enum": ["neutral","happy","angry","sleepy","scared","curious","asleep"] },
    "look":  { "type": "array", "items": {"type":"number","minimum":-1,"maximum":1}, "minItems": 2, "maxItems": 2 },
    "blink": { "oneOf": [ {"type":"boolean"}, {"enum":["both","left","right"]} ] },
    "auto_blink": {"type":"boolean"}, "auto_look": {"type":"boolean"},
    "pupil": { "type":"number","minimum":0.25,"maximum":1.0 },
    "lids":  { "type":"object","additionalProperties":false,"properties":{
               "top":{"type":"number","minimum":0,"maximum":1}, "bottom":{"type":"number","minimum":0,"maximum":1},
               "tilt":{"type":"number","minimum":-1,"maximum":1} } },
    "style": { "type":"object","additionalProperties":false,"properties":{
               "preset":{"enum":["realistic","robo"]},
               "bg":{"$ref":"#/$defs/hex"},
               "sclera":{"type":"object","properties":{"on":{"type":"boolean"},"w":{"type":"integer","minimum":20,"maximum":230},
                         "h":{"type":"integer","minimum":20,"maximum":230},"radius":{"type":"integer","minimum":0,"maximum":115},"color":{"$ref":"#/$defs/hex"}}},
               "iris":{"type":"object","properties":{"on":{"type":"boolean"},"r":{"type":"integer","minimum":5,"maximum":100},
                         "ring_w":{"type":"integer","minimum":0,"maximum":20},"color":{"$ref":"#/$defs/hex"},"ring":{"$ref":"#/$defs/hex"}}},
               "pupil":{"type":"object","properties":{"on":{"type":"boolean"},"r":{"type":"integer","minimum":2,"maximum":90},
                         "shape":{"enum":["round","slit"]},"slit_w":{"type":"integer","minimum":2,"maximum":40},"color":{"$ref":"#/$defs/hex"}}},
               "highlight":{"type":"object","properties":{"on":{"type":"boolean"},"r":{"type":"integer","minimum":1,"maximum":40},
                         "dx":{"type":"integer"},"dy":{"type":"integer"},"color":{"$ref":"#/$defs/hex"}}},
               "lid":{"type":"object","properties":{"color":{"$ref":"#/$defs/hex"},"curve":{"type":"integer","minimum":0,"maximum":60}}} } },
    "save_style": {"type":"boolean"},
    "brightness": {"type":"integer","minimum":0,"maximum":100},
    "transition_ms": {"type":"integer","minimum":0,"maximum":5000},
    "interrupt": {"type":"boolean"}
  },
  "$defs": { "hex": { "type":"string","pattern":"^#[0-9a-fA-F]{6}$" } }
}
```
Body ≤ 1 KB. `preset` is applied first, then the other style fields. `save_style:true` stores the resulting style in NVS (loaded at boot). `brightness` scales all eye colours (renderer multiplies RGB565 channels via a 101-entry constexpr LUT); clips are unaffected. With the backlight mod it drives PWM instead.

### 5.3 Device HTTP API (7 routes)

> **Updated in v2.0**: POST /api/eye is now the cat_eyes schema (style, iris, play, auto, look, save); GET /api/animations added; GET / serves the embedded page.

Token (`Authorization: Bearer <token>`) on all routes except `GET /api/status`.

| Method | Path | Request | Success |
|---|---|---|---|
| POST | `/api/eye` | §5.2 | 202 `{"queued":true}` |
| POST | `/api/play` | `{"clip":"wink"}` (both) or `{"left":"a","right":"b"}`, `"loop":false`, `"then":"eyes"` | 202; 404 if a clip is missing |
| POST | `/api/stop` | `{}` | 202 (clips stop, eyes return) |
| GET | `/api/clips` | — | 200 `{"clips":[{"name","size","frames","fps"}],"free":n}` |
| PUT | `/api/clips/<name>` | raw `.clip`, `Content-Length` | 201 `{"name","size","frames"}`; replaces existing unless playing (409) |
| DELETE | `/api/clips/<name>` | — | 204; 409 if playing |
| GET | `/api/status` | — | 200 `{"version","idf","hostname","uptime_s","heap_internal","heap_min","psram_free","fps","frame_ms_avg","frame_ms_max","rssi","ip","source":{"left","right"},"fs_free","clip_max_bps","spi_hz"}` |
| POST | `/api/ota` | raw app `.bin`, `Content-Length` | 200 `{"rebooting":true}` |

(That is 7 paths + OTA; `GET /` returns a one-line JSON hint pointing to eyes-studio.)

### 5.4 Error body

`{ "error": { "code": "E_BAD_JSON", "message": "look must have 2 items" } }`

### 5.5 Error codes (`engine::Err`)

| Code | HTTP | Meaning |
|---|---|---|
| E_UNAUTHORIZED | 401 | missing/wrong token |
| E_BAD_JSON | 400 | parse/schema error |
| E_TOO_LARGE | 413 | body over limit |
| E_NOT_FOUND | 404 | clip/route missing |
| E_BAD_NAME | 400 | name fails `^[a-z0-9_-]{1,31}$` |
| E_BAD_CLIP | 422 | invalid structure/CRC |
| E_NO_SPACE | 507 | filesystem full |
| E_BUSY | 503 | queue full; upload/OTA running |
| E_CONFLICT | 409 | clip is playing |
| E_LENGTH_REQUIRED | 411 | no Content-Length |
| E_IO | 500 | flash/FS error |
| E_OTA | 500 | OTA failure |

### 5.6 UART console (UART0 via the USB-to-UART port, 115200)

| Command | Effect |
|---|---|
| `wifi <ssid> <password>` | store in NVS, reconnect |
| `wifi show` | SSID, IP, RSSI |
| `token show` / `token set <t>` / `token new` | manage API token (stored in NVS) |
| `hostname <name>` | store, restart mDNS |
| `status` | same data as `/api/status` |
| `eye <mood>` | quick test |
| `reboot` / `factory-reset` | erase NVS (keeps clips) |

---

## 6. Performance budget

### 6.1 SPI push time (one 240 × 240 frame = 115 200 B; trimmed = 97 760 B)

Time = bytes × 8 / f; plus ~0.9 ms window setups per display for a full frame (12 strips × ~75 µs, F9) and the display-switch wait (≤ one strip = 9 600 B).

| Case | Bytes | 20 MHz (default) | 26.7 MHz | 40 MHz (needs SCL/SDA mod) |
|---|---|---|---|---|
| One strip (switch wait) | 9 600 | 3.8 ms | 2.9 ms | 1.9 ms |
| One display, full trimmed | 97 760 | 39.1 ms | 29.3 ms | 19.6 ms |
| **Both displays, full trimmed** | 195 520 | **78.2 ms** | **58.6 ms** | **39.1 ms** |
| → max fps, full-screen on both | — | ~12 | ~16 | ~24 |

### 6.2 Typical dirty areas (both displays, `realistic` style; verified in S3.5)

| Event | Area (both eyes) | Bytes | 20 MHz | 26.7 MHz | 40 MHz |
|---|---|---|---|---|---|
| Idle | 0 | 0 | 0 | 0 | 0 |
| Look step (iris moves ≤ 10 px/frame) | 2 × 114 × 114 = 26 000 px | 52 KB | 20.8 ms | 15.6 ms | 10.4 ms |
| Blink step (lid edge ~45 px/frame, curve 24 px) | 2 × ~75 × 208 = 31 200 px | 62 KB | 25.0 ms | 18.7 ms | 12.5 ms |
| Wink step | 15 600 px | 31 KB | 12.5 ms | 9.4 ms | 6.2 ms |
| Mood/style change (one frame) | 97 760 px | 196 KB | 80 ms | 60 ms | 41 ms |
| Clip on one display, full screen | 48 880 px | 98 KB | 40 ms | 30 ms | 20.5 ms |

→ At the **20 MHz default**, look and blink frames take ≤ ~29 ms including the display-switch wait, so eyes still reach **~30 fps** (tight: if S3.5 measures > 33 ms, blinks run at 4 steps instead of 5 — invisible in practice). A mood change is one ~80 ms frame. Full-screen clips on both displays: ≤ 12 fps at 20 MHz, ≤ 16 fps at 26.7 MHz, ≤ 24 fps with the mod. clipprep uses the device’s reported `spi_hz` to pick a fitting fps.

### 6.3 Circle trimming (`tools/mask_calc.py`; becomes `static_assert`s)

| Method | Pixels per display | Fraction | Window setups |
|---|---|---|---|
| Full rectangle | 57 600 | 1.000 | 12 |
| **Trimmed 20-line strips** | **48 880** | **0.849** | **12** |
| Per-row windows | 45 464 | 0.789 | 240 (net loss: +17 ms commands vs −1.4 ms data) |

Clips: clipprep produces the trimmed bands offline, so the device doesn’t trim clip data.

### 6.4 CPU (160 MHz; 30 fps → 5.33 M cycles/frame)

Span renderer estimate: ~4 cycles/px interior + ~60 cycles per edge pixel + ~200 cycles/row/layer setup.

| Work | Pixels | Est. cycles | Time |
|---|---|---|---|
| Blink step, both eyes | 31 200 | ~0.25 M | 1.6 ms |
| Look step, both eyes | 26 000 | ~0.25 M | 1.6 ms |
| Full redraw, both eyes | 97 760 | ~0.6 M | 3.8 ms |
| Clip frame | any | `fread` only | flash-bound |

Rendering is far below SPI time → system is **SPI-bound**; ~10 % CPU for eyes.

### 6.5 Memory

**Internal SRAM (320 KB):** IDF+FreeRTOS ~60 (A), Wi-Fi+lwIP ~90 (A), httpd ~14, mDNS ~8 (A), LittleFS ~10, **strip buffers 28.8**, render stack 6, console 4, style+model ~1 → **~222 KB**; gate: min free ≥ 40 KB (S0.3/S3.5). Fallbacks: Wi-Fi in PSRAM → 16-line strips → 2 buffers.
**PSRAM (2 MB):** clip indexes (2 × ≤ 48 KB), 4 KB upload/OTA buffer, cJSON allocations.
**Flash (8 MB):**

```csv
# Name,    Type, SubType,  Offset,   Size
nvs,       data, nvs,      0x9000,   0x6000
otadata,   data, ota,      0xF000,   0x2000
phy_init,  data, phy,      0x11000,  0x1000
ota_0,     app,  ota_0,    0x20000,  0x200000
ota_1,     app,  ota_1,    0x220000, 0x200000
storage,   data, littlefs, 0x420000, 0x3D0000
coredump,  data, coredump, 0x7F0000, 0x10000
```
App slots 2 MB (expected image 0.8–1.2 MB without web UI), LittleFS **3.81 MB** (all for clips). If more clip space is needed later: app slots 1.5 MB each → LittleFS 4.8 MB (decide after S3.5 shows real image size).

### 6.6 Clip sizes (RAW565, trimmed bands; measured in S5.6)

| Clip | Frames | ~bytes/frame | ~clip | Fits in 3.6 MB |
|---|---|---|---|---|
| Wink effect (60 × 40 region) | 10 | 4.8 KB | 48 KB | ~75 |
| Heart pop (120 × 120 region) | 20 | 25 KB (diff) | 0.5 MB | ~7 |
| Full-screen GIF, mostly static background | 30 | 30 KB (diff) | 0.9 MB | ~4 |
| Full-screen video-like | 30 | 98 KB | 2.9 MB | 1 |

clipprep enforces the budget (max bytes per clip, `fs_free`) and reports what it changed (fps, frames, region). If flash capacity becomes the limit, add RLE565 as encoding 3 (the format already reserves it).

### 6.7 Flash read throughput needed

| Case | Bytes/s needed |
|---|---|
| Full-screen clip on one display at 15 fps | 1.47 MB/s |
| Full-screen clips on both at 15 fps | 2.93 MB/s |
| Typical effect clip (diff) at 30 fps | 0.1–0.8 MB/s |

S5.0 measures LittleFS (16 KB reads, 4 KB cache) and sets `clip_max_bps` (reported in `/api/status`, used by clipprep). If < 1.5 MB/s → fallback raw partition + `esp_partition_mmap` (D9).

---

## 7. Milestones and slice backlog

T = tier, W = worker, HIL = owner check. **33 core slices.**

### M0 — Bring-up and contracts

| ID | Title | T | W | Verify | HIL |
|---|---|---|---|---|---|
| S0.1 | Install IDF v6.1; flash `hello_world` + Wi-Fi `station`; chip-id, 2 MB PSRAM, board rev; wire both displays (§1.6, VCC from 5 V); measure each module’s current from 5 V | — | owner | — | notes in `docs/hw-notes.md` |
| S0.2 | Firmware skeleton, `Kconfig.projbuild`, `config.hpp` static_asserts, `sdkconfig.defaults` (PSRAM, partitions, -O2, FREERTOS_HZ 1000, SPI ISR in IRAM, rollback), **`scripts/check.sh`** | A | haiku | `scripts/check.sh`; a bad-pin config fails to compile | — |
| S0.3 | Heap probe (Wi-Fi + httpd + mDNS), with/without Wi-Fi in PSRAM | A | haiku | build | numbers recorded |
| S0.4 | **Freeze contracts:** all headers §3, `StaticVec/StaticString`, RAII wrappers, `cpp-style.md`, doctest skeleton, `docs/spec/*`, red cards (S2.2, S3.4, S7.1) | R | **opus** | `scripts/check.sh` | — |

### M1 — `gfx` core

| ID | Title | T | W | Verify |
|---|---|---|---|---|
| S1.1 | Rect ops, constexpr chord table, `trimmed_pixels`, `Strips`; static_asserts for §6.3 | G | haiku | check --quick |
| S1.2 | `rgb565`/`_rgb` (consteval + compile-fail test), `blend`, `Fixed`, ease + edge LUTs | G | haiku | check --quick |

### M2 — Two displays

| ID | Title | T | W | Verify | HIL |
|---|---|---|---|---|---|
| S2.1 | SPI bus + 2 panel IOs (shared DC/RST) + black fill before DISPON + `fill(eye, color)` (+ optional BL PWM if pin ≥ 0) | A | haiku | build | no garbage at boot; left red, right blue, then swap — colours correct (else BGR/invert Kconfig); confirms A9 |
| S2.2 | **StripPool + ISR + acquire/push/wait_idle** for 2 displays | R | haiku (1 strike) | device test: 2 000 strips alternating displays, all returned | log |
| S2.3 | Patterns + timing on both displays + A3 measurement + **clock sweep 20 / 26.7 / 40 MHz** (10 min each, checkerboard + gradient) → set Kconfig default to the fastest clean clock | A | haiku | build | timing within 10 % of §6.1 table; record highest clean clock (repeat after the optional SCL/SDA mod) |

### M3 — Eyes and render engine

| ID | Title | T | W | Verify | HIL |
|---|---|---|---|---|---|
| S3.1 | `EyeStyle` + `StylePatch` + presets + mood table (constexpr, static_asserts), `layout` (incl. right-eye mirroring, lid override), `diff` (iris box + lid bands) | G | haiku | check --quick | — |
| S3.2 | Span renderer (all layers, AA edges, slit pupil, curved/tilted lids); PPM dumps; golden CRCs for both presets × moods × {open, half-blink, look-left, look-up} | G | haiku | check --quick | owner approves PPMs |
| S3.3 | `EyesModel` (idle blink/look, wink, easing, retargeting) + `EyeView` FrameSource; simulated-clock tests | G | haiku | check --quick | — |
| S3.4 | **Render task + Scene with 2 channels + stats** | R | haiku (1 strike) | device run 10 min | both eyes animate in sync at ~30 fps |
| S3.5 | Measure cycles/px, dirty sizes, heap/stack, app image size; update §6 | A | haiku | device log | min free ≥ 40 KB |

### M4 — API, network, console

| ID | Title | T | W | Verify | HIL |
|---|---|---|---|---|---|
| S4.1 | `Command` types + `parse_eye` (incl. style/lids/blink sel/brightness) + `parse_play` (both/left/right) + name check; tests for each schema rule | G | haiku | check --quick | — |
| S4.2 | `Scene` logic (per-display switching, `then_eyes`, deferred eye cmds, interrupt, asleep dimming) with fakes | G | haiku | check --quick | — |
| S4.3 | `net`: STA (NVS → Kconfig), reconnect backoff, mDNS `eyes-<MAC3>` + `_eyes._tcp`; **UART console** §5.6 | A | haiku | build | set Wi-Fi via console; `ping eyes-xxxxxx.local` |
| S4.4 | `web`: server, auth, errors, `/api/status`, `/api/eye`, `/api/play`, `/api/stop`; style save to NVS | A | haiku | build + `eyesctl smoke` | eyes react < 100 ms |
| S4.5 | `tools/eyesctl.py` (status, eye, play, upload, delete, ota, soak) + pytest vs mock | G | haiku | check --quick | — |
| S4.6 | `app_main` wiring, init order, fallbacks | A | sonnet | build | boots to eyes; API works |

### M5 — Clips

| ID | Title | T | W | Verify | HIL |
|---|---|---|---|---|---|
| S5.0 | **Spike:** LittleFS read MB/s (4/16/32 KB reads; cache 512/4096) vs `esp_partition_read` vs mmap → `clip_max_bps`, decision D9 | A | haiku | device log | table; Opus decides if < 1.5 MB/s |
| S5.1 | clipprep: load GIF/PNG-seq/video(ffmpeg), crop/resize/region, circle mask, RGB565 BE | G | haiku | check --quick | — |
| S5.2 | clipprep: frame diff → trimmed bands (≤ 8 rects), writer + CRC, reader, preview GIF, budget enforcement, CLI | G | haiku | check --quick (round-trip bit-exact) | — |
| S5.3 | `clip` core: `parse_header/rect`, `validate`, `crc32` + tests on clipprep-generated goldens and corrupted files | G | haiku | check --quick | — |
| S5.4 | `storage` (mount, list, open, remove, `Upload` RAII + atomic rename, stale tmp cleanup) | A | haiku | device test incl. aborted upload | — |
| S5.5 | `ClipPlayer` (direct `fread` into strip buffers) + add to Scene variant | A | haiku | build + device test | clip plays on left/right/both at correct speed |
| S5.6 | Routes `PUT/DELETE/GET /api/clips` (streaming recv, limits, 409) + asset corpus (5 clips) + size table | A | haiku | `eyesctl upload` + play | upload 0.9 MB clip, play on both |

### M6 — eyes-studio (Docker)

| ID | Title | T | W | Verify | HIL |
|---|---|---|---|---|---|
| S6.1 | FastAPI app: device registry (mDNS + env list), proxy routes (eye/play/stop/clips), prepare endpoint using clipprep, pytest with a mock device | G | haiku | check --quick | — |
| S6.2 | Single HTML page (vanilla JS): device picker, mood buttons, look pad, blink/wink, style preset + colour pickers, brightness, drag-and-drop GIF → preview → send, clip list play/delete | G | haiku | check --quick (page served, API calls mocked) | usable from phone/desktop |
| S6.3 | Dockerfile + compose (`network_mode: host`, ffmpeg, env token, volume for prepared clips) | A | haiku | `docker compose build && docker compose up -d && curl :8080/api/devices` | finds the device |

### M7 — OTA and hardening

| ID | Title | T | W | Verify | HIL |
|---|---|---|---|---|---|
| S7.1 | **OTA** streaming + `/api/ota` + rollback self-test (both displays ok, 100 frames, network up, 30 s) | R | haiku (1 strike) | device test with a deliberately panicking image | rollback observed |
| S7.2 | Power-cut during upload ×10 (owner) + fixes | A | owner + haiku | — | FS mounts, old clips intact |
| S7.3 | 24 h soak (`eyesctl soak`) + fixes | A | sonnet | soak report | no reset, heap stable |

**Optional later:** overlay source + transparent key · RLE565 encoding if flash space runs out · LiveGif (on-device GIF decode) · per-eye backlight · SoftAP provisioning.

**Milestone acceptance**

| Milestone | Acceptance |
|---|---|
| M0 | firmware builds; host suite runs; heap numbers recorded; both displays wired |
| M2 | both displays show independent patterns; full trimmed both ≤ 42 ms; 1 h loop without stall |
| M3 | eyes in sync, 30 fps for blinks/looks, owner approves both presets × all moods |
| M4 | mood change ≤ 100 ms after request; Wi-Fi set via console; 1 000 requests during animation: no reset, ≥ 25 fps |
| M5 | Python reader and device output match (preview GIF vs photo); playback timing error < 5 % over 30 s |
| M6 | from the studio page: drop a GIF → preview → send → play on both eyes |
| M7 | OTA + rollback shown; 10/10 power cuts recovered; 24 h soak passes |

---

## 8. Testing strategy

* **Compile-time tests:** chord sums, LUT endpoints, struct sizes, table sizes, `FrameSource<EyeView>`, `FrameSource<ClipPlayer>`, `Command` trivially copyable, pin rules; a CMake `try_compile` check confirms a bad colour literal / reserved pin fails to compile.
* **Host unit tests (doctest):** all `core/`, `-std=c++23 -fno-exceptions -fno-rtti -Wall -Wextra -Werror -fsanitize=address,undefined`.
* **Renderer goldens:** CRC32 per preset × mood × pose; PPM files for owner review on change.
* **Clip path:** clipprep pytest round-trip (write → read bit-exact), budget tests; C++ `validate` tests on clipprep goldens + corrupted variants (truncated, overlapping rects, out-of-circle rects, bad CRC).
* **Studio:** pytest with a mock device (FastAPI TestClient + respx).
* **On-device:** `test/device/` apps (pool, patterns, heap, fs_bench, playback, upload abort, rollback).
* **Soak:** `eyesctl soak --hours N` — random commands every 0.2–2 s, upload+delete 300 KB every 10 min, status CSV every 10 s. Pass: no reboot, `heap_min` stable ±2 KB after 1 h, ≥ 25 fps outside uploads, p99 API < 300 ms.
* **fps/jitter:** status avg/max; `EYES_DEBUG_PIN` toggles at frame start → logic analyzer.
* **Power:** J5 current: idle, Wi-Fi TX, BL 100 %; brownout detector on; 1 A supply.

`scripts/check.sh` runs host tests, both pytest suites and `idf.py build`; `--quick` skips the IDF build.

---

## 9. Risks and mitigations (ranked)

| # | Risk | Trigger | Mitigation / fallback |
|---|---|---|---|
| R1 | Flash reads too slow for RAW clips | S5.0 < 1.5 MB/s | larger reads; clipprep budgets fps; fallback raw partition + mmap; RLE565 encoding |
| R2 | Flash capacity too small for RAW clips | clipprep budget warnings | smaller regions/fps; shrink app slots to 1.5 MB; RLE565 |
| R3 | Two displays on one bus: shared DC/RST or alternation glitches | S2.1/S2.3 corruption | `wait_idle` at switch (already planned); separate DC pin if needed (spare GPIO23) |
| R4 | Internal heap too small | min free < 40 KB | Wi-Fi in PSRAM, 16-line strips, 2 buffers |
| R5 | Modules’ 1 kΩ series resistors (F22) limit SPI clock | S2.3 artifacts at 26.7/40 MHz | stay at 20 MHz (eyes still ~30 fps, §6.2); or bridge SCL/SDA resistors (§1.5) for 40 MHz; short wires |
| R6 | Wi-Fi jitter | p99 frame > 50 ms | priorities §4.1, no logs in hot path, `WIFI_PS_NONE` |
| R7 | GC9A01 component quirks | wrong init/colours | custom init table (`vendor_config`) or small own driver |
| R8 | No backlight control (F22): “dark” is never fully dark; always-on LEDs add heat/current | owner finds it too bright at night | colour scaling + closed lids (default); optional MOSFET mod on the LED cathode (§1.5) |
| R9 | httpd upload timeouts | resets > 1 MB | chunked recv, 10 s timeouts; studio retries |
| R10 | mDNS not visible in Docker | studio finds no device | `network_mode: host` or `EYES_DEVICES` env list |
| R11 | Haiku writes C-style or over-templated C++ | repeated review findings | `cpp-style.md` in every pack, examples in S0.4 headers, checklist items 3/9 |
| R12 | Host vs device compiler differences | divergent builds | C++23 subset; `check.sh` builds both |

---

## 10. Security and robustness (home LAN baseline)

* Plain HTTP on a trusted LAN; token and data are visible on the network. The browser never sees the token (studio proxies server-side).
* Token: `Authorization: Bearer`; generated at first boot if Kconfig default is empty; stored in NVS; shown/changed via UART console; constant-time compare.
* Limits: JSON ≤ 1 KB, strict schema, names `^[a-z0-9_-]{1,31}$`, `Content-Length` required, clip ≤ `EYES_MAX_CLIP_BYTES` (default 2 MB) and ≤ free space − 64 KB, full validation + CRC at commit.
* Uploads: `.tmp-<name>` → validate → atomic rename (replaces); stale tmp files removed at boot; one upload/OTA at a time.
* OTA: rollback enabled; self-test marks valid; version in status.
* Recovery: TWDT; display re-init on stall; clip errors → eyes; panic → reboot → eyes; `factory-reset` via console.

---

## 11. Effort estimate and first 1–2 hours checklist

| Milestone | Slices | Agent wall-time* | Owner time |
|---|---|---|---|
| M0 | 4 | 3–5 h | 2–3 h |
| M1 | 2 | 1–2 h | — |
| M2 | 3 | 3–5 h | 2 h |
| M3 | 5 | 5–8 h | 2 h |
| M4 | 6 | 4–6 h | 1 h |
| M5 | 7 | 4–7 h | 2 h |
| M6 | 3 | 3–5 h | 1 h |
| M7 | 3 | 3–5 h | 2 h + 24 h unattended |
| **Total** | **33** | **~26–43 h** | **~12–15 h** |

\* incl. review and a typical retry; ~75 % Haiku-only, ~20 % Sonnet retry/integration, ~5 % Opus.
**Parallelism:** after S0.4, all `core/` and Python slices (M1, S3.1–S3.3, S4.1–S4.2, S4.5, S5.1–S5.3, S6.1–S6.2) run in parallel. Hardware chain: M2 → S3.4 → S4.3/S4.4/S4.6 → S5.0/S5.4–S5.6 → S6.3 → M7.

**First 1–2 hours:**
1. Check the latest v6.1.x tag → `idf_version.txt`; `eim install`.
2. USB-to-UART port; `esptool chip-id`; boot log shows 2 MB PSRAM; note board revision.
3. Flash `hello_world` and Wi-Fi `station`; note free heap after connect.
4. Wire both displays per §1.6 (VCC to **5V**, not 3V3); measure each module’s current from 5 V.
5. Flash IDF example `peripherals/lcd/spi_lcd_touch` (GC9A01) at **20 MHz**, once with CS = GPIO4, then GPIO0 → both panels work.
6. Coordinator: create repo (§2), start S0.2 (Haiku) and S0.4 (Opus) in parallel.

---

## 12. Flagged deviations from the given architecture

1. **Two outputs**: the engine drives two displays; each has its own active FrameSource; eye sources share one `EyesModel` so both eyes stay in sync.
2. **Three strip buffers**, shared by both displays, with `wait_idle()` at display switch.
3. **No per-row circle windows**; device trims procedural strips, clipprep pre-trims clip bands.
4. **Scene runs inside the render task.**
5. **FrameSource** = C++ concept + `std::variant`; `FramePlan` adds `full_redraw`, `trim`, `finished`; `fill()` order fixed.
6. **Own OTA push handler.**
7. **Clip v1 is RAW565 only** (simplest for the ESP32; costs flash space).
8. **No on-device web UI** — moved to the eyes-studio Docker container; device API reduced.
9. **Clip storage may move to an mmap’d raw partition** if S5.0 fails; format unchanged.

---

## 13. Self-review log

### 13.1 Simplified in v1.1 and v1.2

| # | Before | Now |
|---|---|---|
| 1 | 14 firmware components | 8 (`core/` + `idf/` split) |
| 2 | 5 pixel encodings + decoder (red slice) | RAW565 only, no decoder; clip slice is green |
| 3 | On-device web UI, static serving, UI LittleFS image | none; eyes-studio Docker app |
| 4 | Brightness, stop, style as many endpoints | folded into `/api/eye`; 7 routes |
| 5 | Device trims clip data | clipprep pre-trims |
| 6 | Rect merging on device | removed |
| 7 | GitHub Actions | `scripts/check.sh` locally |
| 8 | SoftAP provisioning, 80 MHz spike, security extras | removed (console + Kconfig; 40 MHz; LAN baseline) |
| 9 | Per-pixel distance renderer (~40 cyc/px) | span renderer (~4 cyc/px interior) |
| 10 | Per-panel completion bookkeeping for two displays | `wait_idle` at display switch (≤ one strip time per frame) |
| 11 | Function-pointer sources, `has_x` flags | concept + variant, `std::optional` |

### 13.2 Owner answers applied (v1.2)

| Q | Answer | Where |
|---|---|---|
| 1 | two displays | §1.6, §3.4, §3.6, §4.3, §6 |
| 2 | esp32-eyes-like, general | §3.3 (layered `EyeStyle`, presets, lid override), §5.2 |
| 3 | simple upload, Docker prep app | §3.8, §5.3, M6 |
| 4 | module datasheet | F22: no BL pin, 1 kΩ series resistors, on-board LDO → 20 MHz default, 5 V supply, colour-scaled brightness, optional mods §1.5 |
| 5 | no 80 MHz | D3, `config.hpp` |
| 6 | script prepares easiest form, max 240 × 240 | D13, §3.8, §5.1 |
| 7 | freeze OK | §4.5 |
| 8 | build config or UART | §5.6, S4.3 |
| 9 | home LAN | §10 |
| 10 | USB, dim when asleep | §3.3.2 |
| 11 | Sonnet coordinator; local builds | §0.1, D10 |
| 12 | multiple devices | §3.7 hostname/mDNS, studio device picker |

### 13.3 Remaining open points

See §1.5 (optional SCL/SDA mod for 40 MHz, optional backlight MOSFET mod, RAW flash capacity) — all have defaults and don’t block starting.
