# esp_eyes — agent notes

Animated eyes on two GC9A01 round displays driven by an ESP32-C61. Full spec: `eyes-c61-implementation-plan.md` (slice backlog in §7, frozen contracts in §3).

## Tooling
- **ESP-IDF v6.1** lives in `~/esp/esp-idf`, tools in `~/.espressif`. Activate with `source scripts/idf-env.sh` (never source `export.sh` directly: the IDF Python env is 3.13 but the default `python3` is 3.14; the script selects 3.13 via `uv python find 3.13`). Then `idf.py` and `riscv32-esp-elf-g++` are on PATH.
- **uv** manages Python (all versions available) and the project env (`pyproject.toml`, `uv.lock`; dev deps: esptool, esp-idf-size, pyserial, ruff). Run Python tools with `uv run ...`.
- **ffmpeg** is installed (clipprep video input). **Docker is not installed**; Apple `container` exists but avoid it unless really needed. eyes-studio runs as a plain Python app (`uv run`).
- Target: `esp32c61`, 8 MB flash. Host compiler: Apple clang 21 (C++23, `std::expected` OK).
- Note: this machine's `ls` is aliased to something that rejects paths; use `command ls`.

## Commands
- `scripts/check.sh` — the single gate: core-include check, host doctest (ASan/UBSan, C++23, `-Werror`), pytest for `clipprep/` and `studio/`, then `idf.py build`.
- `scripts/check.sh --quick` — skips the IDF build.
- Firmware: `cd firmware && source ../scripts/idf-env.sh && idf.py build` (flash/monitor need the board on USB: `idf.py -p PORT flash monitor`).
- Host tests only: `cmake -S firmware/test/host -B build/host && cmake --build build/host && build/host/host_tests`.

## Conventions
- Layout: `firmware/components/<name>/{core,idf}`. `core/` is pure C++ with no `esp_*`/`freertos/` includes (check.sh enforces it) and is tested on the host; include as `"gfx/core/gfx.hpp"` (components dir is on the include path).
- C++: no exceptions/RTTI/iostream/`std::function`, `std::expected` for errors, `constexpr` tables, `static_assert` as tests, no heap in render paths. Tests: `firmware/test/host/test_*.cpp` (doctest vendored).
- Pixels are RGB565 big-endian (`gfx::Rgb565be`). Circle mask: r²=14470 (calibrated to 48 880 px for 20-line strips).
- Pins/clock come from Kconfig via `firmware/main/config.hpp` (defaults: plan §1.6; SPI 20 MHz).
- Commit per slice: `<ID>: <title>`; local git only, no CI.

## Status
S0.2 (skeleton + gate), S1.1, S1.2 done. Hardware-dependent slices (S0.1, S0.3, S2.x HIL) need the owner at the board. PSRAM config is not yet in `sdkconfig.defaults` (added in S0.3).
