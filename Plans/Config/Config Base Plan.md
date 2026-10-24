# Config Base Plan — first implementation slice
**Status:** Draft, awaiting BY's approval. Implementation starts only after approval.  
**Date:** 2026‑10‑23 (revised 2026‑10‑24: Hardware layout, chip files; CPU boot out of scope; keywords, `.hpp`, bare metal, constructor boot, `bankSize`, minimum 4; no C++ library in common/MC code; separate PC and MC test suites; config in Printer/Config, link-time platform selection)  
**Implements:** [`Config Specification.md`](Config%20Specification.md) §9 (configuration), plus the minimum of §§3–5, §10 and §11 needed to prove it end to end.  
**Request:** [`Config Request.md`](Config%20Request.md) (Status: Discussion)

> **Build status (2026‑10‑24).** Nothing in this plan is built. `Printer/` is empty, and the root `config.h` still holds the `main()` stub. In the shell used for this plan, `g++` and `cmake` are **not on the PATH**. The handoff names CLion's bundled MinGW 15.2 as the host toolchain, so builds in this plan run through CLion or with that toolchain's `bin/` added to the PATH. No `arm-none-eabi` toolchain was found, so the MC header in this plan is compile-checked on the host only (Phase 6).

## 1. Goal

Make this program build and run on the PC, as BY described it (spec §9, §13.28):

```cpp
#include "Printer/Printer.hpp"
#include "Tests/Tests.hpp"

Printer prt1 = {};

int main() {
    const int failed = Tests::runAll();   // all tests run from main (spec §12, §13.41)
    prt1 << message << "Hello Printer 2!" << endLine;
    return failed == 0 ? 0 : 1;           // on the MC, an endless loop instead (MC Hardware plan)
}
```

Printer2 is a technical demo, so the tests run from `main()` before the demo's own work: on the MC before the endless loop, on the PC before "whatever the demo does". There is no separate test executable (spec §13.41).

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
- Link-time platform selection: CMake builds the `pc_*` or the `mc_*` sources (spec §9, §11, §13.37). Also a CMake platform switch, and `CMakePresets.json` with `pc` and `mc` presets.
- The minimal stream core the binding needs: `BaseStream`, `NullStream`, `Stream` (lazy colour, §5), `ConsoleStream`, and PC `FileStream` (open/close/write only).
- `Hardware/PrinterInit.hpp` and `Hardware/pc_PrinterInit.cpp`: the PC boot (VT mode) and the PC binding.
- A minimal `Printer`, just enough for the program above: the constructor calls an idempotent `boot()`, plus a `nullptr`-safe `pStream`, target switching, `const char*` insertion and `endLine`.
- The PC test suite (§4a): our own simple suite in the style of the old project, run from `main()`, with one shared `TestResults` type and isolated tests (spec §12, §13.39, §13.41).
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
├── Printer/                     # library
│   ├── Config/                  # all config headers, spec §9, §13.37
│   │   ├── config.hpp           # common; the only config common code includes
│   │   ├── config_pc.hpp        # PC architecture; included only by pc_* files
│   │   └── config_mc.hpp        # MC architecture, choices only; included only by mc_* files
│   ├── Printer.hpp / .cpp       # umbrella header; minimal Printer, endLine
│   ├── BaseStream.hpp
│   ├── NullStream.hpp / .cpp    # nullSink
│   ├── Stream.hpp / .cpp        # info, message, error; pInner, idle state, binding
│   ├── pc_Stream.cpp            # colour emission (spec §5, §13.38)
│   ├── mc_Stream.cpp            # (MC Hardware plan) plain forwarding, no colour
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
    ├── Tests.hpp                # entry point only: struct Tests { static int runAll(); }
    ├── pc/                      # PC suite (spec §12): free to use the PC; tests Common code too
    │   ├── pc_Tests.cpp         # runner: suite switches, GlobalTestResults, summary (§4a)
    │   ├── TestResults.hpp      # the one result type shared by all PC suites
    │   ├── CaptureStream.hpp
    │   ├── test_config.cpp      # ConfigTests: the Phase 1 checks
    │   ├── test_config_address_probe.cpp  # reports &cfg::defaultFileName from a second source file
    │   ├── test_color.cpp       # ColorTests: C1-C6
    │   ├── test_binding.cpp     # BindingTests: B1-B4
    │   ├── test_printer.cpp     # PrinterTests: P1-P3
    │   └── test_config_mc_host.cpp   # compile-only check of config_mc.hpp; not linked
    └── mc/                      # (MC Hardware plan) brand-new MC suite, incl. mc_Tests.cpp
```

Every header starts with `#pragma once` (spec §2, §13.29).

## 4. Phases

Each phase ends in a buildable, green state. BY commits all changes personally (`AGENTS.md` §5); a phase is a natural commit boundary, but the split is BY's choice.

### Phase 0 — Repository preparation
1. Move `main()` from the root `config.h` into `main.cpp` (resolves handoff P-12 in favour of "keep the config header as compile-time config").
2. `git mv config.h Printer/Config/config.hpp` (now holding only `#pragma once`), and change `main.cpp` to `#include "Printer/Config/config.hpp"`. The repository root is already on `printer_2`'s include path, so `CMakeLists.txt` does not change.
3. Add `.gitignore` for `.idea/`, `cmake-build-*/`, `build/`, `*.exe`. (`main.exe` is currently untracked in the root.)
4. Build with MinGW 15.2: `cmake -S . -B build && cmake --build build`.

**Done when:** the empty demo builds and runs with exit code 0, and `git status` shows no build artefacts.

### Phase 1 — `Printer/Config/config.hpp` (common)
1. Write the header as specified in spec §9: feature flags, `Color` + 16 presets, `Parity` / `StopBits` / `FlowControl`, `UartSettings` (with `bankSize`, spec §7), `uartDefault`, `printerMinBankSize` (4), `printerMaxBankSize`, `defaultFileName`.
2. Declare the constants `inline constexpr` (C++17), as the spec sketches now do. Plain `constexpr` at namespace scope gives each translation unit its own copy; `inline` gives `defaultFileName` one address everywhere.
3. Add a `constexpr bool isValid(const UartSettings&)` helper: `bankSize` is 0 or within `printerMinBankSize`…`printerMaxBankSize`, `dataBits` is 8 or 9, and `baudrate > 0`. There is no even-size check (spec §13.32, §13.33). Platform headers `static_assert` it for every settings constant, so a bad value fails where it is written, not later inside `UsartStream<N>`. The §7 asserts in `UsartStream<N>` stay as the second line of defence. The header includes only `<cstdint>` (spec §1.1).

**Done when:** `Tests/pc/test_config.cpp` includes `config.hpp` **alone** (no platform header) and compiles by hand with `g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -c`, together with the address probe `Tests/pc/test_config_address_probe.cpp`. Its `static_assert`s cover `isValid(uartDefault)`, `darkGray.sgr == 90` and `darkGreen.sgr == 32`, `printerMaxBankSize <= 65535`, and that `isValid` accepts `bankSize` 0 and 4 but rejects 1, 3 and `printerMaxBankSize + 1`. `config.hpp`'s only system include is `<cstdint>` (manual check). `CMakeLists.txt` does not change in Phase 1 (the repository root is already on the include path); the include check and the test target come with Phase 2. The detailed plan is `Phase 1 plan.md`.

### Phase 2 — Platform selection and CMake
1. `PRINTER_PLATFORM` cache variable, `PC` (default) or `MC`. Configuring with any other value is an error.
2. `CMakePresets.json` with a `pc` preset (MinGW, `PRINTER_PLATFORM=PC`) and an `mc` preset (`PRINTER_PLATFORM=MC` with an ARM toolchain file). They are two configurations, not two targets, because each needs its own compiler (spec §11).
3. **Link-time platform selection** (spec §9, §11, §13.37). CMake builds the common sources plus either all `pc_*.cpp` or all `mc_*.cpp` files. Common `.cpp` files include only common headers (`Printer/Config/config.hpp` among them); `pc_*` / `mc_*` files include their own platform config directly. There is no forwarding header and no platform macro. Building both prefixes at once would fail to link with duplicate symbols, which is the intended guard.
4. Targets:
   - `printer`: static library with the common `Printer/` and `Hardware/` sources plus the platform's prefixed sources (`pc_*` or `mc_*`). Its only public include dir is the repository root, so every include is written from there: `"Printer/Printer.hpp"`, `"Printer/Config/config.hpp"`, `"Hardware/PrinterInit.hpp"`.
   - `printer_2`: the demo; links `printer`. It also compiles the test suite of its platform, because `main()` runs the tests (§1, §4a, spec §13.41): on the PC, `Tests/pc/*.cpp` (except the compile-only `test_config_mc_host.cpp`). The MC suite (`Tests/mc/`) is brand-new, independent, and belongs to the MC Hardware plan (spec §12). There is no separate test executable.
   - Optionally, `enable_testing()` and `add_test(NAME printer_2 COMMAND printer_2)`, so `ctest` and CLion can run the demo-plus-tests and read its exit code (§4a).
   - `config_mc_check`: compile-only object library for Phase 6, part of the PC suite; never linked. It checks `config_mc.hpp`, which is a Common-area file (`AGENTS.md` §1).
   - **Library rules (spec §1.1):** a `printer_check_includes` function is introduced here and applied to every Common and MC source, so a header outside the allow-list (`<cstdint>` and the compiler's C headers) fails at configure time. The MC configuration adds `-fno-exceptions -fno-rtti -fno-threadsafe-statics`. `pc_*` sources are not checked. The function below was tested on 2026‑10‑24 with CMake 4.3.1, in a scratch copy: a clean build passed, and adding `#include <array>` to `config.hpp` failed the next plain build with "`<array>` is not allowed in Common or MC code". `CMAKE_CONFIGURE_DEPENDS` is what makes CMake re-check a file as soon as it changes.

     ```cmake
     # Common and MC code may include only these system headers (spec §1.1, AGENTS.md §2).
     set(PRINTER_ALLOWED_SYSTEM_HEADERS cstdint stdint.h stddef.h stdarg.h limits.h float.h)

     function(printer_check_includes)
         foreach(file IN LISTS ARGN)
             set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${file}")
             file(STRINGS "${file}" lines REGEX "^[ \t]*#[ \t]*include[ \t]*<")
             foreach(line IN LISTS lines)
                 string(REGEX REPLACE "^[ \t]*#[ \t]*include[ \t]*<([^>]+)>.*" "\\1" header "${line}")
                 if(NOT header IN_LIST PRINTER_ALLOWED_SYSTEM_HEADERS)
                     message(FATAL_ERROR "${file}: <${header}> is not allowed in Common or MC code (AGENTS.md §2)")
                 endif()
             endforeach()
         endforeach()
     endfunction()
     ```

     It checks each file's own `#include <...>` lines, not what an allowed header includes in turn, and not project includes (`#include "..."`).
   - **PC suite skeleton (§4a):** `Tests/Tests.hpp`, `Tests/pc/TestResults.hpp` and the runner `Tests/pc/pc_Tests.cpp`. The two Phase 1 files, compiled by hand until now under their final names, become the first suite, `ConfigTests`:
     - `Tests/pc/test_config.cpp` keeps its `static_assert`s unchanged and gains `runConfigTests()`;
     - the single-address check becomes a run-time `check()`: `&cfg::defaultFileName` as seen in `test_config.cpp` must equal the address reported by the address probe, `Tests/pc/test_config_address_probe.cpp` (`defaultFileNameAddressFromProbe()`);
     - `main.cpp` gains `Tests::runAll()`, as in §1.
5. The `mc` preset stops with a clear message if no ARM toolchain is found. The MC library build belongs to the MC Hardware plan.

**Done when:** the `pc` preset configures and builds; `printer_2` runs `ConfigTests`, prints the summary with "✅ ALL TESTS PASSED", and exits 0; a deliberately failing `check()` makes it print "❌ SOME TESTS FAILED" with the test's name and exit 1 (manual check, then reverted). The `printer` target compiles with no vendor header on any include path (handoff v0 DoD item). A deliberately added `#include <array>` in a common file fails the build (manual check, then reverted).

### Phase 3 — `Printer/Config/config_pc.hpp`
1. As in spec §9: `platformHasColor = true`, the `PcSink` enum, `infoTarget` / `messageTarget` / `errorTarget`, and `infoColor` / `messageColor` / `errorColor`. Function declarations live in `Hardware/PrinterInit.hpp`, not here, so the config stays data-only.
2. `static_assert` that each role target is a valid `PcSink` value (guards against a future enum edit).

**Done when:** `pc_*` files that need it include it directly, no common file includes it, and Phase 1's test still passes.

### Phase 4 — Minimal stream core
Only what the binding and colour rules need.

1. **`BaseStream`** (§3): pure virtual `sendByteRaw(uint8_t)` and `flush()`; `allowsAnsi()` returns `false` by default; `resetColor()` is a no-op by default. `printf` is **not** added in this slice. No virtual destructor (globals are never deleted through a base pointer), so the destructor stays trivial.
2. **`NullStream nullSink`**: drops bytes; `constexpr` constructor.
3. **`Stream`**, with the globals `info`, `message` and `error`: `BaseStream* pInner` defaults to `&nullSink` through a `constexpr` constructor, so the three globals are constant-initialized (§4). Each holds a `Color color` and an `idle` flag. The class is split by platform:
   - `Stream.hpp` / `Stream.cpp` (common): the class, the globals, `pInner`, `color`, the `idle` flag, and `flush()`, which forwards to `pInner` and does not touch colour state (§5, §13.9).
   - `pc_Stream.cpp` defines `sendByteRaw` and `resetColor` (spec §5, §13.38):
     - `sendByteRaw`: if `cfg::useColorDefault && idle && pInner->allowsAnsi()`, emit `ESC[<sgr>m` and clear `idle`. Then forward the byte.
     - `resetColor()`: if a colour is active, emit `ESC[0m`. Mark the stream idle either way.
   - `mc_Stream.cpp` (MC Hardware plan) defines both as plain forwarding with no colour.
4. **`ConsoleStream console0`**: writes to `std::cout`. `allowsAnsi()` returns a flag that `printerPlatformInit()` sets (false until then).
5. **PC `FileStream file0..file8`**: `open(name, OpenMode = append)`, `close()`, `sendByteRaw`, `flush`; `allowsAnsi()` is always false. A write while closed is dropped (provisional, pending §13.17). There are no markers yet (§13.2), and `name` is a public field (§13.19).
   - **Constant-initialization risk.** A `std::ofstream` member gives `FileStream` a non-`constexpr` constructor. `file0..file8` would then be dynamically initialized, and a static constructor in another translation unit that writes to `file1` (directly or through `error`) would touch an unconstructed object. That breaks the §4 guarantee. A `std::ofstream` member would also put `<fstream>` into the common `FileStream.hpp`, which the library rules forbid (spec §1.1). So the nine `std::ofstream` objects live in a **function-local** static array inside `pc_FileStream.cpp`, constructed on the first `open()`. A namespace-scope array would be initialized at run time, which brings the initialization-order problem back. `FileStream` holds only its index and an `isOpen` flag, and it doesn't touch the array while closed. That keeps its constructor `constexpr` and its header free of the standard library. The function-local static is allowed here because §1.1 rule 5 applies to common and MC code, not to `pc_*` files. See D-4.
6. **`Tests/pc/CaptureStream`**: records bytes. `allowsAnsi()` is settable per test (§12). It belongs to the PC suite; the MC suite will have its own.

**Done when:** the core compiles in the PC build, and the `ColorTests` suite (`Tests/pc/test_color.cpp`, §4a) passes. Each test builds its own local `Stream` and `CaptureStream` (isolation, §4a):
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

**Done when** the `BindingTests` (`Tests/pc/test_binding.cpp`) and `PrinterTests` (`Tests/pc/test_printer.cpp`) suites pass, run from `main()` (§1, §4a). `main.cpp` defines `Printer prt1 = {};`, so `prt1` has booted before any test runs.

`BindingTests` looks at the globals, read-only unless stated:
- B1 (safe before boot): compile-time. `static_assert`s on `constexpr Stream probe{};` show that a `Stream` is constant-initialized with `pInner == &nullSink` and `idle`; the same is done for `NullStream` and `FileStream`. At run time, a local `Stream` that was never bound takes a byte without crashing (it goes to `nullSink`). This replaces the earlier "probe object defined before `prt1`", which can't work now that the tests run after `prt1` has booted.
- B2: `info.pInner == &console0`, `message.pInner == &console0`, and `error.pInner == &file1` (the §9 example values). Colours equal the `cfg::*Color` constants.
- B3: `console0.allowsAnsi()` matches an independent `GetConsoleMode` check on `STD_OUTPUT_HANDLE`: true in an interactive Windows 10 console, false when stdout is redirected. Run under `ctest`, or as `printer_2 > out.txt`, to exercise the fallback path.
- B4 (idempotent boot): with the RAII snapshot (§4a), set `info.pInner = &capture`, call `prt1.boot()`, and check that `info.pInner` is still `&capture`. Constructing a local `Printer` doesn't rebind either. The snapshot restores `info` at the end of the test.

`PrinterTests` uses only local objects: a local `Printer`, local `Stream`s bound to local `CaptureStream`s (`allowsAnsi = true`), with colours set per test:
- P1: `p << msg << "Hello Printer 2!" << endLine`, with `msg.color = darkGreen`, gives `"\x1b[32mHello Printer 2!\n\x1b[0m"`.
- P2 (mid-line switch): `p << a << "a" << b << "b" << endLine`, with `a` dark gray and `b` bright red, gives `"\x1b[90ma\x1b[0m"` on `a`'s capture and `"\x1b[91mb\n\x1b[0m"` on `b`'s.
- P3 (`nullptr` guard): a fresh local `Printer` has `pStream == nullptr`, and `p << "x"` emits nothing and doesn't crash.

Manual check, recorded in the handoff: running `printer_2` in a Windows 10 console shows the test summary, then "Hello Printer 2!" in dark green, and no raw `ESC[` text.

### Phase 6 — `Printer/Config/config_mc.hpp` and host compile-check
1. Write the header as in spec §9. It holds choices only:
   - `hseFrequency_Hz`, which is informational. The PLL and divider lines are commented out, because they belong to the optional CPU boot (spec §13.23).
   - `sysclkFrequency_Hz`, the clock in effect (HSI 8 MHz), and the derived `cpuFrequency_MHz` / `apb1Frequency_Hz` / `apb2Frequency_Hz`.
   - The remap choices.
   - `usart2Settings` / `usart3Settings` (`usart3Settings.bankSize == 0`, per the request).
   - `McSink` with the role targets.

   Add `static_assert(isValid(usartNSettings))`.
2. `test_config_mc_host.cpp` includes `Printer/Config/config_mc.hpp` directly, as every MC file will. It is built as a separate **compile-only** CMake target (`config_mc_check`, an object library that nothing links), not as part of `printer_2`. The `cfg` constants are `inline constexpr`, so linking MC and PC definitions into one executable would break the one-definition rule.

**Done when** the check compiles and these `static_assert`s hold:
- `cfg::sysclkFrequency_Hz == 8'000'000`, `cfg::cpuFrequency_MHz == 8`, `cfg::apb1Frequency_Hz == 8'000'000` and `cfg::apb2Frequency_Hz == 8'000'000`: the F103 reset clock (HSI, all dividers at 1), with no CPU boot.
- `cfg::platformHasColor == false`, and `cfg::usart3Settings.bankSize == 0`.
- `isValid(cfg::usart2Settings)` and `isValid(cfg::usart3Settings)`.

**Limit:** this proves that `config_mc.hpp` is self-contained and that the derived frequencies match the reset clock. Checking them against the silicon limits (APB1 ≤ 36 MHz, SYSCLK ≤ 72 MHz) is the chip header's job, and checking them against the running clock is the clock check's job (spec §10, §13.27). Both arrive with the MC Hardware plan.

## 4a. PC test suite (D-3)

Our own simple suite, in the style of the old project's `Common/Tests/pc_Tests.cpp`, with two simplifications (spec §12, §13.39, §13.41).

**Entry point.**
- `Tests/Tests.hpp` declares `struct Tests { static int runAll(); };`. It is the only file the two suites have in common, and it holds no test code.
- The PC build defines `runAll()` in `Tests/pc/pc_Tests.cpp`; the MC build will define it in `Tests/mc/mc_Tests.cpp`. Like every `pc_` / `mc_` pair, the linker selects one.
- `main()` calls it first (§1). It returns the number of failed tests, so `main()` can return non-zero on failure.

**One result type** (simplification 1). The old runner had about 20 near-identical `add()` overloads, one per suite type. Here every suite returns the same `TestResults`, so the global summary needs a single `add()`.

```cpp
// Tests/pc/TestResults.hpp — the one result type of the PC suite (PC code: std:: allowed).
#pragma once

#include <iostream>
#include <string>
#include <vector>

struct TestResults {
    explicit TestResults(const char* name) : suiteName(name) {}

    const char* suiteName;
    size_t total = 0;
    size_t passed = 0;
    size_t failed = 0;
    std::vector<std::string> failedTestNames;
    std::vector<std::string> passedTestNames;

    void check(const std::string& name, bool ok) {
        ++total;
        if (ok) {
            ++passed;
            passedTestNames.push_back(name);
        } else {
            ++failed;
            failedTestNames.push_back(std::string(suiteName) + ": " + name);
            std::cout << "  FAIL " << name << std::endl;
        }
    }
};
```

**Runner** (simplification 2: the old `bool` switches and `if` chain become one table, and every suite keeps its on/off switch). The summary keeps the old shape: per-suite breakdown, totals, duration, ✅ / ❌, failed names. Emoji are fine on the PC; the MC suite will print plain text.

```cpp
// Tests/pc/pc_Tests.cpp — PC suite runner.
#include "Tests/Tests.hpp"
#include "Tests/pc/TestResults.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

TestResults runConfigTests();
TestResults runColorTests();
TestResults runBindingTests();
TestResults runPrinterTests();

namespace {

struct Suite {
    const char* title;
    bool enabled;
    TestResults (*run)();
};

struct GlobalTestResults {
    size_t total = 0;
    size_t passed = 0;
    size_t failed = 0;
    std::vector<std::string> failedTestNames;
    std::vector<TestResults> suites;
    double durationSeconds = 0.0;

    void add(const TestResults& r) {
        total += r.total;
        passed += r.passed;
        failed += r.failed;
        failedTestNames.insert(failedTestNames.end(), r.failedTestNames.begin(), r.failedTestNames.end());
        suites.push_back(r);
    }

    void printSummary() const {
        std::cout << "\n=== GLOBAL TEST SUMMARY ===" << std::endl;
        std::cout << "Per-suite breakdown:" << std::endl;
        for (const auto& s : suites) {
            std::cout << "  " << std::setw(30) << std::left << s.suiteName
                      << " total=" << s.total << " passed=" << s.passed << " failed=" << s.failed << std::endl;
        }
        std::cout << "Total Tests: " << total << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << failed << std::endl;
        std::cout << "Duration: " << std::fixed << std::setprecision(3) << durationSeconds << " seconds" << std::endl;
        std::cout << std::endl;
        if (failed == 0) {
            std::cout << "✅ ALL TESTS PASSED" << std::endl;
        } else {
            std::cout << "❌ SOME TESTS FAILED" << std::endl;
            std::cout << "\nFailed tests:" << std::endl;
            for (const auto& name : failedTestNames) {
                std::cout << "  - " << name << std::endl;
            }
        }
        std::cout << "=============================" << std::endl;
    }
};

} // namespace

int Tests::runAll() {
    const Suite suites[] = {
        {"Config Tests",  true, runConfigTests},
        {"Color Tests",   true, runColorTests},
        {"Binding Tests", true, runBindingTests},
        {"Printer Tests", true, runPrinterTests},
    };

    std::cout << "\n=== RUNNING ALL TEST SUITES ===\n" << std::endl;
    const auto start = std::chrono::steady_clock::now();

    GlobalTestResults global;
    for (const Suite& s : suites) {
        if (!s.enabled) {
            continue;
        }
        std::cout << "\n--- " << s.title << " ---" << std::endl;
        global.add(s.run());
    }

    global.durationSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    global.printSummary();
    return static_cast<int>(global.failed);
}
```

**A suite** is one function in one `test_*.cpp` file: `TestResults runColorTests() { TestResults r("ColorTests"); …; return r; }`. Each phase that adds code adds its suite and one row to the runner's table. `TestResults` has a constructor taking the suite name, because brace-initializing only its first member (`TestResults r{"ColorTests"}`) trips `-Wmissing-field-initializers` under `-Wextra -Werror`, as the scratch build showed.

**Isolation** (spec §12, §13.41). Every test is independent of every other:
- **A test builds its own objects:** its own `Printer`, its own `Stream`s and its own `CaptureStream`s, as locals, and may drive them into any state, even an inconsistent one. Nothing it does reaches the next test.
  - A local `Printer` is cheap: its constructor calls `boot()`, which is already a no-op after `prt1` booted (idempotent, spec §9).
- **Tests don't touch the globals** (`info`, `message`, `error`, `fileN`, `console0`, `prt1`). The exception is `BindingTests`, which must look at them. It reads them, and where a test has to change one, it uses a small RAII snapshot that saves `pInner`, `color` and the idle state of `info` / `message` / `error`, and restores them at the end of the test.
- **"Safe before boot" is proved at compile time,** because tests now run after `prt1` has booted. `static_assert`s on `constexpr Stream probe{};` (and the same for `FileStream` and `NullStream`) compile only if the type can be constant-initialized.
  - Checked with g++ 15.2.0 on 2026‑10‑24 on a model of `BaseStream` / `Stream` with virtual functions: the probe compiles, and a type without a `constexpr` constructor fails with "the type … of 'constexpr' variable … is not literal".

**`ctest`** (optional, proposed): `enable_testing()` plus `add_test(NAME printer_2 COMMAND printer_2)` runs the whole demo, tests included, and reports its exit code. Under `ctest`, stdout is redirected, which also exercises the VT-mode fallback (B3).

## 5. Decisions

Decisions this plan introduced. All five are now decided by BY; the provisional defaults they replaced are noted where they differ.

| ID | Question | Decision |
|---|---|---|
| D-1 | Where do the config headers live? | **Decided by BY, 2026‑10‑24 (spec §13.37):** `Printer/Config/` holds the common `config.hpp` and the platform-architecture `config_pc.hpp` / `config_mc.hpp`; chip specifics live in `Hardware/`. Supersedes the earlier default `Config/` outside the library |
| D-2 | How does common code find the platform header? | **Decided by BY, 2026‑10‑24 (spec §13.37):** it doesn't. Common `.cpp` files include only common headers; platform `.cpp` files are selected by CMake (link-time polymorphism). Supersedes the `config_platform.hpp` forwarders |
| D-3 | Test suites and frameworks (handoff P-10) | **Decided by BY, 2026‑10‑24 (spec §13.39):** the PC suite is our own simple suite, similar to the old project's; no external framework (Catch2 / GoogleTest are not used). The MC suite is a brand-new suite, written later in the MC Hardware plan. BY's wording: "mostly agree. For PC we will write own simple suite similar to the old one. For MC we will write brand-new test suite later". **Refined the same day (spec §13.41, §4a):** all tests run from `main()` (no separate executable); every suite uses one shared `TestResults` type; emoji are fine in the PC summary, while the MC suite prints plain text; every test is isolated and builds its own `Printer`, so "even breaking up to inconsistent state" doesn't affect the next one |
| D-4 | PC `FileStream`'s `ofstream` storage | **Decided by BY, 2026‑10‑24 ("agree"):** a function-local static array of nine `std::ofstream` in `pc_FileStream.cpp`, constructed on first `open()`; `FileStream` holds an index and an `isOpen` flag. This keeps constant initialization (spec §4) and keeps `<fstream>` out of the common header (spec §1.1). Revised earlier the same day: the first proposal, "aligned storage + placement-new", would have needed `<new>` |
| D-5 | The bytes `endLine` emits, and their order | **Decided by BY, 2026‑10‑24 ("agree", spec §13.40):** `'\n'` first, then `resetColor()`. Resetting first would leave the stream idle, so the newline would re-emit the colour (§5). The line ending is `"\n"` on both platforms for now; whether MC serial terminals need `"\r\n"` is for the Printer core plan |

## 6. Spec open items adopted provisionally by this plan

The plan follows the current spec text for these. If BY decides otherwise, the cost of changing is noted.

| Spec item | Adopted here | Cost to change later |
|---|---|---|
| §13.14 unit suffixes | `cpuFrequency_MHz`, `apb1Frequency_Hz`, `hseFrequency_Hz` | Trivial rename |
| §13.17 write while closed | Dropped | Low: one branch in `FileStream::sendByteRaw` |
| §13.19 `fileN.name` | Public field | Low: becomes a setter |
| §13.20 role → target form | `PcSink` / `McSink` enums resolved in `printerInit()` | Low: one `switch` per platform |
| §13.42 `dataBits` and parity (deferred) | Data bits only: 8 with any parity, 9 without parity, 7 excluded | Low: one condition in `isValid()` |
| §13.25 the two boot functions | `printerPlatformInit()` / `printerInit()` stay public | Low: make them private or rename |

These spec open items are **not touched** by this plan and do not block it: §13.1, §13.2, §13.11, §13.12, §13.15, §13.16, §13.18, §13.22 and §13.31.

The spec's 2026‑10‑24 decisions §13.5 (keywords), §13.13 (16-colour palette with names), §13.28 (constructor boot, umbrella header, presets), §13.29 (`.hpp`) and §13.30 (bare metal) are applied here. Decisions §13.6, §13.21, §13.23, §13.24, §13.26 and §13.27 shape the layout, but their implementation belongs to the MC Hardware plan.

## 7. Risks and checks

- **Library rules (spec §1.1).** The include check catches a forbidden header, but not a call to `malloc` or `new` through an allowed one. On the PC those link silently; on the MC, the link without `_sbrk` catches them. Until the MC build exists, code review is the check: no `new`, `malloc` or `std::` outside `pc_*` files. `Tests/pc/` is PC code and may use the standard library.
- **Trivial destructors** (spec §1.1 rule 4). `BaseStream` has no virtual destructor (Phase 4), and the same goes for `Stream`, `NullStream`, `FileStream` and `Printer`. Each class gets `static_assert(__has_trivial_destructor(T))`, using the compiler built-in because `<type_traits>` is not allowed in common code. Checked with g++ 15.2.0 under `-nostdinc++` on 2026‑10‑24: `__has_trivial_destructor` works, and `__is_trivially_destructible` does not exist in that compiler.

- **Constant initialization of the streams** (§4). This depends on every stream type having a `constexpr` constructor and on `nullSink` being constant-initialized. GCC has no `-Wglobal-constructors` and C++17 has no `constinit`, so the check is test B1 plus a code-review rule: no stream type may have a member with a non-`constexpr` constructor (D-4).
- **`prt1` is dynamically initialized** by design: its constructor boots. The `nullptr` guard (P3) covers a `Printer` that hasn't been bound, not a `prt1` used before its own constructor. That case is undefined behaviour and stays a documented rule (spec §4).
- **Construction order across translation units.** `prt1`'s constructor binds streams that live in other translation units. This is safe only because those streams are constant-initialized (they exist before any constructor runs), which is the same rule as the first risk.
- **A common file including a platform config** would compile on one platform and break the other. Phase 2's include check sees only `<...>` system includes, so this is a code-review rule: no file without a `pc_` / `mc_` prefix includes `config_pc.hpp` or `config_mc.hpp`.
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
   - The brand-new MC test suite (`Tests/mc/`, with `mc_Tests.cpp` defining `Tests::runAll()`). It runs from `main()` before the endless loop, prints plain text rather than emoji, and is independent of the PC suite (spec §12, §13.39, §13.41).
3. **File markers plan,** once §13.2, §13.16 and §13.17 are decided.
