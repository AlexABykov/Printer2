# Config Base Plan — first implementation slice
**Status:** Draft, awaiting BY's approval. Implementation starts only after approval.  
**Date:** 2026‑10‑23 (revised 2026‑10‑24: Hardware layout, chip files, MC boot; 2026‑10‑24: CPU boot out of scope)  
**Implements:** [`Config Specification.md`](Config%20Specification.md) §9 (configuration), plus the minimum of §§3–5 and §10 needed to prove it end to end.  
**Request:** [`Config Request.md`](Config%20Request.md) (Status: Discussion)

> **Build status (2026‑10‑24).** Nothing in this plan is built. `Printer/` is empty, and the root `config.h` still holds the `main()` stub. In the shell used for this plan, `g++` and `cmake` are **not on the PATH**. The handoff names CLion's bundled MinGW 15.2 as the host toolchain, so builds in this plan run through CLion or with that toolchain's `bin/` added to the PATH. No `arm-none-eabi` toolchain was found, so the MC header in this plan is compile-checked on the host only (Phase 6).

## 1. Goal

Turn the §9 sketches into real headers, select them per platform through CMake, and prove the PC path end to end. `prt1.boot()` runs the PC boot and then binds `streamLog` / `streamMessage` / `streamError` per `config_pc.h`. Colour follows the §5 precedence rules, and nothing crashes before `prt1.boot()`. The MC header is written and checked on the host, but no MC hardware code runs in this slice.

This follows handoff house rule 1 (**PC first**): the configuration and binding model is proven on the host before any register, boot, USART, DMA or GPIO code exists.

## 2. Scope

**In scope**
- `config.h`, `config_pc.h`, `config_mc.h` exactly as named in spec §9, with compile-time self-checks.
- The "one include name" mechanism (`config_platform.h`, D-2) and a CMake platform switch.
- The minimal stream core the binding needs: `BaseStream`, `NullStream`, `Stream` (lazy colour, §5), `ConsoleStream`, and PC `FileStream` (open/close/write only).
- `Hardware/PrinterInit.h` and `Hardware/pc_PrinterInit.cpp`: the PC boot (VT mode) and the PC binding.
- A minimal `Printer` holding only `boot()`, and the global `prt1` (§13.25).
- A host test target with a `CaptureStream` (spec §12).
- A host compile-check of `config_mc.h`. It includes no CMSIS header, so it needs no stub.

**Out of scope** (later plans)
- The rest of `Printer`: `pStream`, `operator<<`, `endLine`, `hold`, `StreamGuard`, number formatting, `printf` (§§4, 6). The demo and tests drive streams through `sendByteRaw` / `resetColor` directly.
- Everything chip-specific (spec §10): `mc_PrinterInit.cpp`, `mc_chip.h` / `pc_chip.h`, `STM32F103xE_regs.h`, `mc_STM32F103xE.h` / `pc_STM32F103xE.h`, PC register emulation, the MC boot sequence, BRR, `UsartStream<N>`, DMA, `updateUsartX()`.
- File markers and the splitter (§13.2, §13.16, §13.17). In this slice, PC `FileStream` writes plain bytes.

## 3. Proposed layout

Files marked *(MC Hardware plan)* are listed to fix the layout. This plan does not create them.

```text
Printer2/
├── CMakeLists.txt              # library + demo + tests, PRINTER_PLATFORM switch
├── main.cpp                    # demo only; owns main()
├── Config/                     # project-owned configuration (D-1)
│   ├── config.h                # common, spec §9
│   ├── config_pc.h
│   ├── config_mc.h             # choices only, no CMSIS
│   ├── pc/config_platform.h    # one line: #include "config_pc.h"   (D-2)
│   └── mc/config_platform.h    # one line: #include "config_mc.h"
├── Printer/                    # library, common code
│   ├── Printer.h / .cpp        # minimal: Printer::boot(), prt1
│   ├── BaseStream.h
│   ├── NullStream.h / .cpp     # nullSink
│   ├── Stream.h / .cpp         # lazy colour; streamLog, streamMessage, streamError
│   ├── FileStream.h            # file0..file8 declarations
│   ├── pc_FileStream.cpp
│   └── pc_ConsoleStream.h / .cpp   # console0
├── Hardware/                   # public include dir of the printer target
│   ├── PrinterInit.h           # declares printerPlatformInit(), printerInit()
│   ├── pc_PrinterInit.cpp      # PC boot (VT mode) + binding
│   ├── mc_PrinterInit.cpp      # (MC Hardware plan) real boot + binding
│   ├── mc_chip.h               # (MC Hardware plan) single chip selection, MC
│   ├── pc_chip.h               # (MC Hardware plan) single chip selection, PC
│   ├── STM32F103xE_regs.h      # (MC Hardware plan) register layout, both platforms
│   ├── mc_STM32F103xE.h        # (MC Hardware plan) real addresses, silicon facts
│   └── pc_STM32F103xE.h        # (MC Hardware plan) static volatile registers
└── Tests/
    ├── CaptureStream.h
    ├── test_main.cpp           # assert-based runner (D-3)
    ├── test_config_common.cpp
    ├── test_binding_pc.cpp
    ├── test_color.cpp
    └── test_config_mc_host.cpp
```

## 4. Phases

Each phase ends in a buildable, green state and is one commit, unless BY prefers otherwise.

### Phase 0 — Repository preparation
1. Move `main()` from `config.h` into `main.cpp` (resolves handoff P-12 in favour of "keep `config.h` as compile-time config").
2. `git mv config.h Config/config.h` (now empty apart from `#pragma once`), and point the include path at `Config/`.
3. Add `.gitignore` for `.idea/`, `cmake-build-*/`, `build/`, `*.exe`. (`main.exe` is currently untracked in the root.)
4. Build with MinGW 15.2: `cmake -S . -B build && cmake --build build`.

**Done when:** the empty demo builds and runs with exit code 0, and `git status` shows no build artefacts.

### Phase 1 — `Config/config.h` (common)
1. Write the header exactly as in spec §9: feature flags, `Color` + 16 presets, `Parity` / `StopBits` / `FlowControl`, `UartSettings`, `uartDefault`, `printerMaxDmaBuffer`, `defaultFileName`.
2. Declare the constants `inline constexpr` (C++17), not plain `constexpr`. Plain `constexpr` at namespace scope gives each translation unit its own copy. That is harmless for integers, but `inline` gives `defaultFileName` one address everywhere. This is a deviation from the sketch's spelling only, not its meaning.
3. Add a `constexpr bool isValid(const UartSettings&)` helper: `bufferSize` is even and `<= printerMaxDmaBuffer`, `dataBits` is 8 or 9, and `baudrate > 0`. Platform headers `static_assert` it for every settings constant, so a bad value fails where it is written, not later inside `UsartStream<N>`. The §7 asserts in `UsartStream<N>` stay as the second line of defence.

**Done when:** `test_config_common.cpp` includes `config.h` **alone** (no platform header) and compiles. It also `static_assert`s `isValid(uartDefault)`, `darkGray.sgr == 90` and `darkGreen.sgr == 32`, and that `isValid` rejects an odd `bufferSize`.

### Phase 2 — Platform selection and CMake
1. `PRINTER_PLATFORM` cache variable, `PC` (default) or `MC`. Configuring with any other value is an error.
2. Selection mechanism (D-2): CMake adds `Config/pc/` or `Config/mc/` to the include path. Common code includes only `"config_platform.h"`, which forwards to the platform header. No macros are needed, which matches spec §2's preference for avoiding `#if`.
3. Targets:
   - `printer`: static library with `Printer/` + `Hardware/` sources for the platform (`pc_*` or `mc_*`). Its public include dirs are `Printer/`, `Hardware/`, `Config/` and `Config/<platform>/`. `Hardware/` is public because `mc_chip.h` / `pc_chip.h` are public selection points (spec §10).
   - `printer_2`: the demo; links `printer`.
   - `printer_tests`: host test executable, registered with `ctest`. Built only when `PRINTER_PLATFORM=PC`.
   - `config_mc_check`: compile-only object library for Phase 6; never linked.
4. `PRINTER_PLATFORM=MC` without an ARM toolchain file stops with a clear message. The MC library build belongs to the MC Hardware plan.

**Done when:** a PC configure + build succeeds, `ctest` runs (with an empty test set at this point), and the `printer` target compiles with no STM32 header on any include path (handoff v0 DoD item).

### Phase 3 — `Config/config_pc.h`
1. As in spec §9: `platformHasColor = true`, `PcSink` enum, the three role targets, and the three colours. Function declarations live in `Hardware/PrinterInit.h`, not here, so the config stays data-only.
2. `static_assert` that each role target is a valid `PcSink` value (guards against a future enum edit).

**Done when:** `config_platform.h` resolves to it in the PC build and Phase 1's test still passes.

### Phase 4 — Minimal stream core
Only what the binding and colour rules need.

1. **`BaseStream`** (§3): pure virtual `sendByteRaw(uint8_t)` and `flush()`; `allowsAnsi()` returns `false` by default; `resetColor()` is a no-op by default. `printf` is **not** added in this slice. No virtual destructor (globals are never deleted through a base pointer), so the destructor stays trivial.
2. **`NullStream nullSink`**: drops bytes; `constexpr` constructor.
3. **`Stream`**: `BaseStream* pInner` defaulting to `&nullSink` through a `constexpr` constructor, so the three globals are constant-initialized (§4). Holds `Color color` and an `idle` flag. `sendByteRaw` implements §5:
   - `if constexpr (cfg::platformHasColor && cfg::useColorDefault)`, then `if (idle && pInner->allowsAnsi())`: emit `ESC[<sgr>m` and clear `idle`. Then forward the byte.
   - `resetColor()`: if a colour is active, emit `ESC[0m`. Mark the stream idle either way.
   - `flush()` forwards to `pInner` and does not touch colour state (§5, §13.9).
4. **`ConsoleStream console0`**: writes to `std::cout`. `allowsAnsi()` returns a flag that `printerPlatformInit()` sets (false until then).
5. **PC `FileStream file0..file8`**: `open(name, OpenMode = append)`, `close()`, `sendByteRaw`, `flush`; `allowsAnsi()` is always false. A write while closed is dropped (provisional, pending §13.17). There are no markers yet (§13.2), and `name` is a public field (§13.19).
   - **Constant-initialization risk.** A `std::ofstream` member gives `FileStream` a non-`constexpr` constructor. `file0..file8` would then be dynamically initialized, and a static constructor in another translation unit that writes to `file1` (directly or through `streamError`) would touch an unconstructed object. That breaks the §4 guarantee. The proposal is to keep the `ofstream` in aligned storage, constructed with placement-new in `open()` and destroyed in `close()`, so `FileStream` keeps a `constexpr` constructor. See D-4.
6. **`Tests/CaptureStream`**: records bytes. `allowsAnsi()` is settable per test (§12).

**Done when:** the core compiles in the PC build, and `test_color.cpp` passes:
- C1: `allowsAnsi = true`, colour `darkGray`, "ab" gives `"\x1b[90mab"`; then `resetColor()` gives `"\x1b[0m"`.
- C2: after `resetColor()`, the next byte re-emits `"\x1b[90m"` (the §5 idle rule, covering the next line on the same stream).
- C3: `resetColor()` on an idle stream emits nothing.
- C4: `allowsAnsi = false`: no escape bytes at all.
- C5: `flush()` between two bytes does not re-emit the colour.
- C6: a `Stream` whose `pInner` is a `FileStream` emits no escape bytes (files never colour, §5).

### Phase 5 — PC boot and binding: `prt1.boot()`
1. `Hardware/PrinterInit.h` declares `void printerPlatformInit();` and `void printerInit();` (spec §9, "Boot and binding").
2. `Hardware/pc_PrinterInit.cpp` defines both:
   - `printerPlatformInit()`: `GetStdHandle(STD_OUTPUT_HANDLE)` → `GetConsoleMode` → `SetConsoleMode(mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)`. On success it sets `console0`'s ANSI flag; on any failure it leaves the flag false. Guarded by `#ifdef _WIN32`; on a non-Windows host the flag is true only when `isatty(STDOUT_FILENO)` is true. No register emulation and no boot mock yet: nothing uses registers in this slice.
   - `printerInit()`: resolve each `PcSink` to an object through a `switch` (`console0`, `file0..file8`) and bind the three `pInner`s. Set `streamLog.color` / `streamMessage.color` / `streamError.color` from `cfg::*Color`. There is nothing to bind for `fileN` on PC.
3. `Printer/Printer.h / .cpp`: a minimal `class Printer` with only `void boot()`, which calls `printerPlatformInit()` and then `printerInit()`, plus `Printer prt1;`. The Printer core plan adds everything else to this class. Whether the two free functions stay public is §13.25. Here they stay public, so the tests can call them.
4. Demo `main.cpp`: call `prt1.boot()`. Write one line to each role through a small local helper that loops `sendByteRaw`, then call `resetColor()`. In an interactive console the lines appear in colour.

**Done when** `test_binding_pc.cpp` passes:
- B1: before `prt1.boot()`, all three `pInner == &nullSink`, and writing a byte to each does not crash. The test must run before any test that boots, so it is first in `test_main.cpp`.
- B2: after `prt1.boot()`, `streamLog.pInner == &console0`, `streamMessage.pInner == &console0`, and `streamError.pInner == &file1` (the §9 example values). Colours equal the `cfg::*Color` constants.
- B3: under `ctest`, stdout is a pipe, so `console0.allowsAnsi() == false`. This exercises the VT fallback path on every test run.
- Manual check, recorded in the handoff: running `printer_2` in a Windows 10 console shows dark-gray and dark-green lines, and no raw `ESC[` text.

### Phase 6 — `Config/config_mc.h` and host compile-check
1. Write the header as in spec §9: choices only. That covers `hseFrequency_Hz` (informational; the PLL and divider lines are commented out because they belong to the optional CPU boot, spec §13.23), the clock in effect `sysclkFrequency_Hz` (HSI 8 MHz) and the derived `cpuFrequency_MHz` / `apb1Frequency_Hz` / `apb2Frequency_Hz`. It also covers the remap choices, `usart2Settings` / `usart3Settings` (`usart3Settings.bufferSize == 0`, per the request), and `McSink` with the role targets. Add `static_assert(isValid(usartNSettings))`.
2. `test_config_mc_host.cpp` includes `config_mc.h` **directly** (not via `config_platform.h`). It is built as a separate **compile-only** CMake target (`config_mc_check`, an object library that nothing links), not as part of `printer_tests`. The `cfg` constants are `inline constexpr`, so linking MC and PC definitions into one executable would break the one-definition rule.

**Done when** the check compiles and these `static_assert`s hold:
- `cfg::sysclkFrequency_Hz == 8'000'000`, `cfg::cpuFrequency_MHz == 8`, `cfg::apb1Frequency_Hz == 8'000'000` and `cfg::apb2Frequency_Hz == 8'000'000`: the F103 reset clock (HSI, all dividers at 1), with no CPU boot.
- `cfg::platformHasColor == false`, and `cfg::usart3Settings.bufferSize == 0`.
- `isValid(cfg::usart2Settings)` and `isValid(cfg::usart3Settings)`.

**Limit:** this proves that `config_mc.h` is self-contained and that the derived frequencies match the reset clock. Checking them against the silicon limits (APB1 ≤ 36 MHz, SYSCLK ≤ 72 MHz) is the chip header's job (spec §10), so it arrives with the MC Hardware plan.

## 5. Decisions needed from BY

New decisions this plan introduces. Each has a provisional default so work can start; none is decided.

| ID | Question | Provisional default |
|---|---|---|
| D-1 | Where do the config headers live: `Config/` (project-owned, outside the library) or `Printer/Config/` (shipped with the library)? | `Config/`. A host project supplies its own config, and the `printer` target takes the directory from a CMake variable `PRINTER_CONFIG_DIR` (default `${CMAKE_SOURCE_DIR}/Config`) |
| D-2 | How does common code find the platform header? | Include-path selection of `config_platform.h` (kept, 2026‑10‑24) |
| D-3 | Test style (handoff P-10) | Assert-based runner with no external dependency, registered with `ctest` |
| D-4 | PC `FileStream`'s `ofstream` storage | Aligned storage + placement-new in `open()`, to keep constant initialization (§4) |

## 6. Spec open items adopted provisionally by this plan

The plan follows the current spec text for these. If BY decides otherwise, the cost of changing is noted.

| Spec item | Adopted here | Cost to change later |
|---|---|---|
| §13.13 colour model | 16-colour SGR `Color` | Low: `Color` and one emit function |
| §13.14 unit suffixes | `cpuFrequency_MHz`, `apb1Frequency_Hz`, `hseFrequency_Hz` | Trivial rename |
| §13.17 write while closed | Dropped | Low: one branch in `FileStream::sendByteRaw` |
| §13.19 `fileN.name` | Public field | Low: becomes a setter |
| §13.20 role → target form | `PcSink` / `McSink` enums resolved in `printerInit()` | Low: one `switch` per platform |
| §13.25 boot entry point | `prt1.boot()`; `printerPlatformInit()` / `printerInit()` stay public | Low: make them private or rename |

These spec open items are **not touched** by this plan and do not block it: §13.1, §13.2, §13.11, §13.12, §13.15, §13.16, §13.18, §13.22 and §13.27. The spec's decisions §13.6, §13.21, §13.23, §13.24 and §13.26 (2026‑10‑24) shape the layout above, but their implementation belongs to the MC Hardware plan.

## 7. Risks and checks

- **Constant initialization of the globals** (§4). This depends on every stream type having a `constexpr` constructor and on `nullSink` being constant-initialized. GCC has no `-Wglobal-constructors` and C++17 has no `constinit`, so the check is test B1 plus a code-review rule: no stream type may have a member with a non-`constexpr` constructor (D-4). The same rule applies to `Printer prt1`.
- **`if constexpr` in common code** requires every common TU that uses `Stream` to see `config_platform.h`. Including it from `Stream.h` keeps that automatic.
- **`std::cout` vs Windows VT mode.** VT processing applies to the console handle, so `std::cout` output picks it up. Phase 5's manual check confirms it, since `ctest` cannot (stdout is a pipe there).
- **Toolchain on the PATH.** Builds run via CLion or with MinGW 15.2's `bin/` added. Each handoff entry should name the compiler and the result (handoff house rule 5).

## 8. Next plans (after this one)

1. **Printer core v0:** the rest of `Printer` / `prt1`: `pStream`, target switching with `resetColor()` on switch (§5), `endLine`, string, char and integer insertions. This is the handoff's v0 workstream.
2. **MC Hardware plan** (spec §10, §13.6, §13.21, §13.23, §13.24):
   - Chip files: `mc_chip.h` / `pc_chip.h`, `STM32F103xE_regs.h`, `mc_STM32F103xE.h` (addresses, DMA mapping, pin tables, flash latency rule, silicon-limit `static_assert`s) and `pc_STM32F103xE.h` (static `volatile` registers).
   - The Printer's MC boot in `mc_PrinterInit.cpp`, in the spec's order: GPIO remap if needed → GPIO for the used interfaces → USART → DMA if needed. **No CPU boot:** the Printer runs on the clock in effect (spec §13.23).
   - BRR from `apb1Frequency_Hz` / `apb2Frequency_Hz`, host-checked at the reset clock: 8 MHz/115200 → 69 (about 115 942 baud, +0.64 %) and 8 MHz/230400 → 35 (about 228 571 baud, −0.79 %). If a CPU boot runs at 72 MHz: 36 MHz/115200 → 313, 36 MHz/230400 → 156, 72 MHz/115200 → 625. How these values track a CPU boot is §13.27, which needs a decision before this step.
   - The binding to `mcUsart2` / `mcUsart3`.
   - `UsartStream<N>`, polled first, then DMA. It is tested on the PC against the static registers first; a timed model with simulated interrupts follows when a driver path waits on a flag.
   - The first ARM build.
3. **File markers plan,** once §13.2, §13.16 and §13.17 are decided.
