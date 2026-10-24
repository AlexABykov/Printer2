# Config Base Plan — first implementation slice
**Status:** Draft, awaiting BY's approval. Implementation starts only after approval.  
**Date:** 2026‑10‑23 (revised 2026‑10‑24: Hardware layout, chip files; CPU boot out of scope; keywords, `.hpp`, bare metal, constructor boot, `bankSize`, minimum 4; no C++ library in common/MC code; separate PC and MC test suites)  
**Implements:** [`Config Specification.md`](Config%20Specification.md) §9 (configuration), plus the minimum of §§3–5, §10 and §11 needed to prove it end to end.  
**Request:** [`Config Request.md`](Config%20Request.md) (Status: Discussion)

> **Build status (2026‑10‑24).** Nothing in this plan is built. `Printer/` is empty, and the root `config.h` still holds the `main()` stub. In the shell used for this plan, `g++` and `cmake` are **not on the PATH**. The handoff names CLion's bundled MinGW 15.2 as the host toolchain, so builds in this plan run through CLion or with that toolchain's `bin/` added to the PATH. No `arm-none-eabi` toolchain was found, so the MC header in this plan is compile-checked on the host only (Phase 6).

## 1. Goal

Make this program build and run on the PC, as BY described it (spec §9, §13.28):

```cpp
#include "Printer/Printer.hpp"

Printer prt1 = {};

int main() {
    prt1 << message << "Hello Printer 2!" << endLine;
}
```

Behind that line, the slice proves the configuration model:
- the config headers are selected per platform;
- the `Printer` constructor boots the PC platform and binds `info` / `message` / `error` per `config_pc.hpp`;
- colour follows the §5 precedence rules;
- nothing crashes before boot.

The MC config header is written and checked on the host, but no MC hardware code runs in this slice.

This follows handoff house rule 1 (**PC first**): the configuration and binding model is proven on the host before any register, boot, USART, DMA or GPIO code exists.

## 2. Scope

**In scope**
- `config.hpp`, `config_pc.hpp`, `config_mc.hpp` as specified in spec §9, with compile-time self-checks.
- The `config_platform.hpp` selection mechanism (D-2), a CMake platform switch, and `CMakePresets.json` with `pc` and `mc` presets (spec §11).
- The minimal stream core the binding needs: `BaseStream`, `NullStream`, `Stream` (lazy colour, §5), `ConsoleStream`, and PC `FileStream` (open/close/write only).
- `Hardware/PrinterInit.hpp` and `Hardware/pc_PrinterInit.cpp`: the PC boot (VT mode) and the PC binding.
- A minimal `Printer`, just enough for the program above: the constructor calls an idempotent `boot()`, plus a `nullptr`-safe `pStream`, target switching, `const char*` insertion and `endLine`.
- A host test target with a `CaptureStream` (spec §12).
- A host compile-check of `config_mc.hpp`. It includes no vendor header, so it needs no stub.

**Out of scope** (later plans)
- The rest of `Printer`: numbers, `char`, `hold`, `StreamGuard`, `flushDma`, `printf` (§§4, 6).
- Everything chip-specific (spec §10):
  - `mc_PrinterInit.cpp`;
  - the chip files `mc_chip.hpp` / `pc_chip.hpp`, `STM32F103xE_regs.hpp`, `mc_STM32F103xE.hpp` / `pc_STM32F103xE.hpp`;
  - PC register emulation, the MC boot sequence, the clock check and the Printer's assert (§13.27), BRR, `UsartStream<N>`, DMA, `updateUsartX()`;
  - startup code (§13.31).
- File markers and the splitter (§13.2, §13.16, §13.17). In this slice, PC `FileStream` writes plain bytes.

## 3. Proposed layout

Files marked *(MC Hardware plan)* are listed to fix the layout. This plan does not create them.

```text
Printer2/                        # on the include path: "Printer/Printer.hpp", "Hardware/..."
├── CMakeLists.txt               # library + demo + tests, PRINTER_PLATFORM switch
├── CMakePresets.json            # pc, mc
├── main.cpp                     # demo: the program in §1
├── Config/                      # project-owned configuration (D-1)
│   ├── config.hpp               # common, spec §9
│   ├── config_pc.hpp
│   ├── config_mc.hpp            # choices only, no vendor headers
│   ├── pc/config_platform.hpp   # one line: #include "config_pc.hpp"   (D-2)
│   └── mc/config_platform.hpp   # one line: #include "config_mc.hpp"
├── Printer/                     # library, common code
│   ├── Printer.hpp / .cpp       # umbrella header; minimal Printer, endLine
│   ├── BaseStream.hpp
│   ├── NullStream.hpp / .cpp    # nullSink
│   ├── Stream.hpp / .cpp        # lazy colour; info, message, error
│   ├── FileStream.hpp           # file0..file8 declarations
│   ├── pc_FileStream.cpp
│   └── pc_ConsoleStream.hpp / .cpp   # console0
├── Hardware/
│   ├── PrinterInit.hpp          # declares printerPlatformInit(), printerInit()
│   ├── pc_PrinterInit.cpp       # PC boot (VT mode) + binding
│   ├── mc_PrinterInit.cpp       # (MC Hardware plan) Printer's MC boot + binding
│   ├── mc_chip.hpp              # (MC Hardware plan) single chip selection, MC
│   ├── pc_chip.hpp              # (MC Hardware plan) single chip selection, PC
│   ├── STM32F103xE_regs.hpp     # (MC Hardware plan) register layout, both platforms
│   ├── mc_STM32F103xE.hpp       # (MC Hardware plan) real addresses, silicon facts
│   └── pc_STM32F103xE.hpp       # (MC Hardware plan) static volatile registers
└── Tests/
    ├── pc/                      # PC suite (spec §12): free to use the PC; tests Common code too
    │   ├── CaptureStream.hpp
    │   ├── test_main.cpp        # PC suite runner (D-3); defines the tests' prt1
    │   ├── test_config_common.cpp
    │   ├── test_binding_pc.cpp
    │   ├── test_color.cpp
    │   ├── test_printer.cpp
    │   └── test_config_mc_host.cpp
    └── mc/                      # (MC Hardware plan) MC suite, independent of the PC suite
```

Every header starts with `#pragma once` and the `__cplusplus` guard (spec §2).

## 4. Phases

Each phase ends in a buildable, green state and is one commit, unless BY prefers otherwise.

### Phase 0 — Repository preparation
1. Move `main()` from the root `config.h` into `main.cpp` (resolves handoff P-12 in favour of "keep the config header as compile-time config").
2. `git mv config.h Config/config.hpp` (now holding only `#pragma once` and the `__cplusplus` guard), and point the include path at `Config/`.
3. Add `.gitignore` for `.idea/`, `cmake-build-*/`, `build/`, `*.exe`. (`main.exe` is currently untracked in the root.)
4. Build with MinGW 15.2: `cmake -S . -B build && cmake --build build`.

**Done when:** the empty demo builds and runs with exit code 0, and `git status` shows no build artefacts.

### Phase 1 — `Config/config.hpp` (common)
1. Write the header as specified in spec §9: feature flags, `Color` + 16 presets, `Parity` / `StopBits` / `FlowControl`, `UartSettings` (with `bankSize`, spec §7), `uartDefault`, `printerMinBankSize` (4), `printerMaxBankSize`, `defaultFileName`.
2. Declare the constants `inline constexpr` (C++17), not plain `constexpr`. Plain `constexpr` at namespace scope gives each translation unit its own copy. That is harmless for integers, but `inline` gives `defaultFileName` one address everywhere. This is a deviation from the sketch's spelling only, not its meaning.
3. Add a `constexpr bool isValid(const UartSettings&)` helper: `bankSize` is 0 or within `printerMinBankSize`…`printerMaxBankSize`, `dataBits` is 8 or 9, and `baudrate > 0`. There is no even-size check (spec §13.32, §13.33). Platform headers `static_assert` it for every settings constant, so a bad value fails where it is written, not later inside `UsartStream<N>`. The §7 asserts in `UsartStream<N>` stay as the second line of defence. The header includes only `<stdint.h>`, never `<cstdint>` (spec §1.1).

**Done when:** `test_config_common.cpp` includes `config.hpp` **alone** (no platform header) and compiles with `-nostdinc++` (spec §1.1). It also `static_assert`s `isValid(uartDefault)`, `darkGray.sgr == 90` and `darkGreen.sgr == 32`, `printerMaxBankSize <= 65535`, and that `isValid` accepts `bankSize` 0 and 4 but rejects 1, 3 and `printerMaxBankSize + 1`.

### Phase 2 — Platform selection and CMake
1. `PRINTER_PLATFORM` cache variable, `PC` (default) or `MC`. Configuring with any other value is an error.
2. `CMakePresets.json` with a `pc` preset (MinGW, `PRINTER_PLATFORM=PC`) and an `mc` preset (`PRINTER_PLATFORM=MC` with an ARM toolchain file). They are two configurations, not two targets, because each needs its own compiler (spec §11).
3. Selection mechanism (D-2): CMake adds `Config/pc/` or `Config/mc/` to the include path. Common code includes only `"config_platform.hpp"`, which forwards to the platform header. No macros are needed, which matches spec §2's preference for avoiding `#if`.
4. Targets:
   - `printer`: static library with `Printer/` + `Hardware/` sources for the platform (`pc_*` or `mc_*`). Its public include dirs are the repository root (for `"Printer/Printer.hpp"` and `"Hardware/..."`), `Config/` and `Config/<platform>/`.
   - `printer_2`: the demo; links `printer`.
   - `printer_tests`: the **PC suite** (`Tests/pc/`), registered with `ctest`. Built only when `PRINTER_PLATFORM=PC`. The MC suite (`Tests/mc/`) is independent and belongs to the MC Hardware plan (spec §12).
   - `config_mc_check`: compile-only object library for Phase 6, part of the PC suite; never linked. It checks `config_mc.hpp`, which is a Common-area file (`AGENTS.md` §1).
   - **Library rules (spec §1.1):** common sources (`Config/`, `Printer/` outside `pc_*`) and `mc_*` sources compile with `-nostdinc++`, so a C++ library header is a build error. The MC configuration adds `-fno-exceptions -fno-rtti -fno-threadsafe-statics`. `pc_*` sources compile without `-nostdinc++`. CMake sets this per source file group.
5. The `mc` preset stops with a clear message if no ARM toolchain is found. The MC library build belongs to the MC Hardware plan.

**Done when:** the `pc` preset configures and builds, `ctest` runs (with an empty test set at this point), and the `printer` target compiles with no vendor header on any include path (handoff v0 DoD item). A deliberately added `#include <cstdint>` in a common file fails the build (manual check, then reverted).

### Phase 3 — `Config/config_pc.hpp`
1. As in spec §9: `platformHasColor = true`, the `PcSink` enum, `infoTarget` / `messageTarget` / `errorTarget`, and `infoColor` / `messageColor` / `errorColor`. Function declarations live in `Hardware/PrinterInit.hpp`, not here, so the config stays data-only.
2. `static_assert` that each role target is a valid `PcSink` value (guards against a future enum edit).

**Done when:** `config_platform.hpp` resolves to it in the PC build and Phase 1's test still passes.

### Phase 4 — Minimal stream core
Only what the binding and colour rules need.

1. **`BaseStream`** (§3): pure virtual `sendByteRaw(uint8_t)` and `flush()`; `allowsAnsi()` returns `false` by default; `resetColor()` is a no-op by default. `printf` is **not** added in this slice. No virtual destructor (globals are never deleted through a base pointer), so the destructor stays trivial.
2. **`NullStream nullSink`**: drops bytes; `constexpr` constructor.
3. **`Stream`**, with the globals `info`, `message` and `error`: `BaseStream* pInner` defaults to `&nullSink` through a `constexpr` constructor, so the three globals are constant-initialized (§4). Each holds a `Color color` and an `idle` flag. `sendByteRaw` implements §5:
   - `if constexpr (cfg::platformHasColor && cfg::useColorDefault)`, then `if (idle && pInner->allowsAnsi())`: emit `ESC[<sgr>m` and clear `idle`. Then forward the byte.
   - `resetColor()`: if a colour is active, emit `ESC[0m`. Mark the stream idle either way.
   - `flush()` forwards to `pInner` and does not touch colour state (§5, §13.9).
4. **`ConsoleStream console0`**: writes to `std::cout`. `allowsAnsi()` returns a flag that `printerPlatformInit()` sets (false until then).
5. **PC `FileStream file0..file8`**: `open(name, OpenMode = append)`, `close()`, `sendByteRaw`, `flush`; `allowsAnsi()` is always false. A write while closed is dropped (provisional, pending §13.17). There are no markers yet (§13.2), and `name` is a public field (§13.19).
   - **Constant-initialization risk.** A `std::ofstream` member gives `FileStream` a non-`constexpr` constructor. `file0..file8` would then be dynamically initialized, and a static constructor in another translation unit that writes to `file1` (directly or through `error`) would touch an unconstructed object. That breaks the §4 guarantee. A `std::ofstream` member would also put `<fstream>` into the common `FileStream.hpp`, which the library rules forbid (spec §1.1). So the nine `std::ofstream` objects live in a **function-local** static array inside `pc_FileStream.cpp`, constructed on the first `open()`. A namespace-scope array would be initialized at run time, which brings the initialization-order problem back. `FileStream` holds only its index and an `isOpen` flag, and it doesn't touch the array while closed. That keeps its constructor `constexpr` and its header free of the standard library. The function-local static is allowed here because §1.1 rule 5 applies to common and MC code, not to `pc_*` files. See D-4.
6. **`Tests/pc/CaptureStream`**: records bytes. `allowsAnsi()` is settable per test (§12). It belongs to the PC suite; the MC suite will have its own.

**Done when:** the core compiles in the PC build, and `test_color.cpp` passes:
- C1: `allowsAnsi = true`, colour `darkGray`, "ab" gives `"\x1b[90mab"`; then `resetColor()` gives `"\x1b[0m"`.
- C2: after `resetColor()`, the next byte re-emits `"\x1b[90m"` (the §5 idle rule, covering the next line on the same stream).
- C3: `resetColor()` on an idle stream emits nothing.
- C4: `allowsAnsi = false`: no escape bytes at all.
- C5: `flush()` between two bytes does not re-emit the colour.
- C6: a `Stream` whose `pInner` is a `FileStream` emits no escape bytes (files never colour, §5).

### Phase 5 — PC boot, binding and the minimal `Printer`
1. `Hardware/PrinterInit.hpp` declares `void printerPlatformInit();` and `void printerInit();` (spec §9, "Boot and binding").
2. `Hardware/pc_PrinterInit.cpp` defines both:
   - `printerPlatformInit()`: `GetStdHandle(STD_OUTPUT_HANDLE)` → `GetConsoleMode` → `SetConsoleMode(mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)`. On success it sets `console0`'s ANSI flag; on any failure it leaves the flag false. Guarded by `#ifdef _WIN32`; on a non-Windows host the flag is true only when `isatty(STDOUT_FILENO)` is true. There is no register emulation, boot mock or clock check yet, because nothing uses registers in this slice.
   - `printerInit()`: resolve each `PcSink` to an object through a `switch` (`console0`, `file0..file8`) and bind the three `pInner`s. Set `info.color` / `message.color` / `error.color` from `cfg::*Color`. There is nothing to bind for `fileN` on PC.
3. `Printer/Printer.hpp` / `.cpp`, the umbrella header (spec §13.28). It includes the keywords, the config and `Hardware/PrinterInit.hpp`, and declares `extern Printer prt1;`. The minimal `class Printer` has:
   - `BaseStream* pStream = nullptr;`
   - A constructor that calls `boot()`.
   - `void boot()`: idempotent through a static `booted` flag; on the first call it runs `printerPlatformInit()` and then `printerInit()`. The free functions stay public (§13.25).
   - `operator<<(Stream&)` / `operator<<(FileStream&)`: call `resetColor()` on the outgoing target (mid-line switch, §5), then re-point `pStream`.
   - `operator<<(const char*)`: sends each byte to `pStream`; does nothing while `pStream == nullptr` (§4).
   - `endLine`: a keyword object whose `operator<<` sends `'\n'` and then calls `pStream->resetColor()` (D-5).
4. Demo `main.cpp`: exactly the program in §1. In an interactive console, the line appears in dark green.

**Done when** `test_binding_pc.cpp` and `test_printer.cpp` pass. `test_main.cpp` defines the tests' `Printer prt1 = {};`.
- B1 (before boot): `test_main.cpp` defines a probe object **before** `prt1`. Definition order is construction order within one translation unit, so the probe's constructor runs first. It records whether `info.pInner`, `message.pInner` and `error.pInner` equal `&nullSink`, and it writes a byte to each stream directly. A test then asserts what was recorded. The probe does not print through `prt1`, because that would use `prt1` before its lifetime begins (spec §4).
- B2: after construction, `info.pInner == &console0`, `message.pInner == &console0`, and `error.pInner == &file1` (the §9 example values). Colours equal the `cfg::*Color` constants.
- B3: under `ctest`, stdout is a pipe, so `console0.allowsAnsi() == false`. This exercises the VT fallback path on every test run.
- B4 (idempotent): set `info.pInner = &capture`, call `prt1.boot()`, and `info.pInner` is still `&capture`. Constructing a second `Printer` doesn't rebind either.
- P1: with `message.pInner = &capture` (`allowsAnsi = true`), `prt1 << message << "Hello Printer 2!" << endLine` gives `"\x1b[32mHello Printer 2!\n\x1b[0m"`.
- P2 (mid-line switch): `prt1 << info << "a" << error << "b" << endLine` gives `"\x1b[90ma\x1b[0m"` on `info`'s capture and `"\x1b[91mb\n\x1b[0m"` on `error`'s.
- P3 (`nullptr` guard): a local `Printer p2;` has `pStream == nullptr`, and `p2 << "x"` emits nothing and doesn't crash.
- Each test restores the bindings it changed.
- Manual check, recorded in the handoff: running `printer_2` in a Windows 10 console shows "Hello Printer 2!" in dark green, and no raw `ESC[` text.

### Phase 6 — `Config/config_mc.hpp` and host compile-check
1. Write the header as in spec §9. It holds choices only:
   - `hseFrequency_Hz`, which is informational. The PLL and divider lines are commented out, because they belong to the optional CPU boot (spec §13.23).
   - `sysclkFrequency_Hz`, the clock in effect (HSI 8 MHz), and the derived `cpuFrequency_MHz` / `apb1Frequency_Hz` / `apb2Frequency_Hz`.
   - The remap choices.
   - `usart2Settings` / `usart3Settings` (`usart3Settings.bankSize == 0`, per the request).
   - `McSink` with the role targets.

   Add `static_assert(isValid(usartNSettings))`.
2. `test_config_mc_host.cpp` includes `config_mc.hpp` **directly** (not via `config_platform.hpp`). It is built as a separate **compile-only** CMake target (`config_mc_check`, an object library that nothing links), not as part of `printer_tests`. The `cfg` constants are `inline constexpr`, so linking MC and PC definitions into one executable would break the one-definition rule.

**Done when** the check compiles and these `static_assert`s hold:
- `cfg::sysclkFrequency_Hz == 8'000'000`, `cfg::cpuFrequency_MHz == 8`, `cfg::apb1Frequency_Hz == 8'000'000` and `cfg::apb2Frequency_Hz == 8'000'000`: the F103 reset clock (HSI, all dividers at 1), with no CPU boot.
- `cfg::platformHasColor == false`, and `cfg::usart3Settings.bankSize == 0`.
- `isValid(cfg::usart2Settings)` and `isValid(cfg::usart3Settings)`.

**Limit:** this proves that `config_mc.hpp` is self-contained and that the derived frequencies match the reset clock. Checking them against the silicon limits (APB1 ≤ 36 MHz, SYSCLK ≤ 72 MHz) is the chip header's job, and checking them against the running clock is the clock check's job (spec §10, §13.27). Both arrive with the MC Hardware plan.

## 5. Decisions needed from BY

New decisions this plan introduces. Each has a provisional default so work can start; none is decided except where marked.

| ID | Question | Provisional default |
|---|---|---|
| D-1 | Where do the config headers live: `Config/` (project-owned, outside the library) or `Printer/Config/` (shipped with the library)? | `Config/`. A host project supplies its own config, and the `printer` target takes the directory from a CMake variable `PRINTER_CONFIG_DIR` (default `${CMAKE_SOURCE_DIR}/Config`) |
| D-2 | How does common code find the platform header? | Include-path selection of `config_platform.hpp` (kept by BY, 2026‑10‑24) |
| D-3 | PC suite framework (handoff P-10) | Assert-based runner with no external dependency, registered with `ctest`. The PC suite may use any framework (spec §12), so Catch2 or GoogleTest is also an option. The MC suite chooses its own in the MC Hardware plan |
| D-4 | PC `FileStream`'s `ofstream` storage | A function-local static array of nine `std::ofstream` in `pc_FileStream.cpp`, constructed on first `open()`; `FileStream` holds an index and an `isOpen` flag. This keeps constant initialization (spec §4) and keeps `<fstream>` out of the common header (spec §1.1). Revised 2026‑10‑24: the earlier "aligned storage + placement-new" would have needed `<new>` |
| D-5 | The bytes `endLine` emits, and their order | `'\n'` first, then `resetColor()`. Resetting first would leave the stream idle, so the newline would re-emit the colour (§5). The line ending is `"\n"` on both platforms for now; whether MC serial terminals need `"\r\n"` is for the Printer core plan |

## 6. Spec open items adopted provisionally by this plan

The plan follows the current spec text for these. If BY decides otherwise, the cost of changing is noted.

| Spec item | Adopted here | Cost to change later |
|---|---|---|
| §13.13 colour model | 16-colour SGR `Color` | Low: `Color` and one emit function |
| §13.14 unit suffixes | `cpuFrequency_MHz`, `apb1Frequency_Hz`, `hseFrequency_Hz` | Trivial rename |
| §13.17 write while closed | Dropped | Low: one branch in `FileStream::sendByteRaw` |
| §13.19 `fileN.name` | Public field | Low: becomes a setter |
| §13.20 role → target form | `PcSink` / `McSink` enums resolved in `printerInit()` | Low: one `switch` per platform |
| §13.25 the two boot functions | `printerPlatformInit()` / `printerInit()` stay public | Low: make them private or rename |

These spec open items are **not touched** by this plan and do not block it: §13.1, §13.2, §13.11, §13.12, §13.15, §13.16, §13.18, §13.22 and §13.31.

The spec's 2026‑10‑24 decisions §13.5 (keywords), §13.28 (constructor boot, umbrella header, presets), §13.29 (`.hpp`) and §13.30 (bare metal) are applied here. Decisions §13.6, §13.21, §13.23, §13.24, §13.26 and §13.27 shape the layout, but their implementation belongs to the MC Hardware plan.

## 7. Risks and checks

- **Library rules (spec §1.1).** `-nostdinc++` on common sources catches a C++ library header, but it doesn't catch a call to `malloc` or `new` through a C header. On the PC those link silently; on the MC, the link without `_sbrk` catches them. Until the MC build exists, code review is the check: no `new`, `malloc` or `std::` outside `pc_*` files. `Tests/pc/` is PC code and may use the standard library. The one exception is `test_config_common.cpp`, which compiles with `-nostdinc++` to prove that `config.hpp` is clean.
- **Trivial destructors** (spec §1.1 rule 4). `BaseStream` has no virtual destructor (Phase 4), and the same goes for `Stream`, `NullStream`, `FileStream` and `Printer`. Each class gets `static_assert(__has_trivial_destructor(T))`, using the compiler built-in because `<type_traits>` is not allowed in common code. Checked with g++ 15.2.0 under `-nostdinc++` on 2026‑10‑24: `__has_trivial_destructor` works, and `__is_trivially_destructible` does not exist in that compiler.

- **Constant initialization of the streams** (§4). This depends on every stream type having a `constexpr` constructor and on `nullSink` being constant-initialized. GCC has no `-Wglobal-constructors` and C++17 has no `constinit`, so the check is test B1 plus a code-review rule: no stream type may have a member with a non-`constexpr` constructor (D-4).
- **`prt1` is dynamically initialized** by design: its constructor boots. The `nullptr` guard (P3) covers a `Printer` that hasn't been bound, not a `prt1` used before its own constructor. That case is undefined behaviour and stays a documented rule (spec §4).
- **Construction order across translation units.** `prt1`'s constructor binds streams that live in other translation units. This is safe only because those streams are constant-initialized (they exist before any constructor runs), which is the same rule as the first risk.
- **`if constexpr` in common code** requires every common TU that uses `Stream` to see `config_platform.hpp`. Including it from `Stream.hpp` keeps that automatic.
- **`std::cout` vs Windows VT mode.** VT processing applies to the console handle, so `std::cout` output picks it up. `std::cout` is used from `prt1`'s constructor, which libstdc++ supports because it initializes the standard streams before user globals. Phase 5's manual check confirms both, since `ctest` cannot (stdout is a pipe there).
- **Toolchain on the PATH.** Builds run via CLion or with MinGW 15.2's `bin/` added. Each handoff entry should name the compiler and the result (handoff house rule 5).

## 8. Next plans (after this one)

1. **Printer core v0:** the rest of `Printer`: `char`, integer and floating-point insertions, `hold`, `StreamGuard`, `flushDma`, `printf` (§§4–6), and the line-ending question from D-5. This is the handoff's v0 workstream.
2. **MC Hardware plan** (spec §10, §13.6, §13.21, §13.23, §13.24, §13.27, §13.31):
   - Startup code and the optional `cpuBoot()` hook, once §13.31 is decided, plus the small CPU-boot example (72 MHz, flash latency first).
   - Chip files: `mc_chip.hpp` / `pc_chip.hpp`, `STM32F103xE_regs.hpp` (from RM0008), `mc_STM32F103xE.hpp` (addresses, DMA mapping, pin tables, flash latency rule, silicon-limit `static_assert`s) and `pc_STM32F103xE.hpp` (static `volatile` registers).
   - The Printer's MC boot in `mc_PrinterInit.cpp`, in the spec's order: clock check → GPIO remap if needed → GPIO for the used interfaces → USART → DMA if needed. There is no CPU boot inside the Printer (spec §13.23).
   - The running clock check before each line or bank swap, and the Printer's assert (MC: halt; PC: `std::abort()`).
   - BRR from `apb1Frequency_Hz` / `apb2Frequency_Hz`, host-checked at the reset clock: 8 MHz/115200 → 69 (about 115 942 baud, +0.64 %) and 8 MHz/230400 → 35 (about 228 571 baud, −0.79 %). With the CPU-boot example at 72 MHz: 36 MHz/115200 → 313, 36 MHz/230400 → 156, 72 MHz/115200 → 625.
   - The binding to `mcUsart2` / `mcUsart3`.
   - `UsartStream<N>`, polled first, then DMA. It is tested on the PC against the static registers first; a timed model with simulated interrupts follows when a driver path waits on a flag.
   - The bank-size edge-case tests promised in spec §13.34 (`bankSize` 1–4, head / body / tail states). They decide whether `printerMinBankSize` can drop from 4 to 1.
   - The first ARM build (`mc` preset).
   - The MC test suite (`Tests/mc/`), with its own framework and its own way of building and running, independent of the PC suite (spec §12).
3. **File markers plan,** once §13.2, §13.16 and §13.17 are decided.
