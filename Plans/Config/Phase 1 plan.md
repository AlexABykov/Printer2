# Phase 1 Plan — `Config/config.hpp` (common configuration)
**Status:** Draft, awaiting BY's approval.  
**Date:** 2026‑10‑24 (revised 2026‑10‑24: no C++ library, `<stdint.h>`, `-nostdinc++`)  
**Parent:** [`Config Base Plan.md`](Config%20Base%20Plan.md), Phase 1 (and the remainder of Phase 0, §2 below)  
**Spec:** [`Config Specification.md`](Config%20Specification.md) §1.1, §2, §5, §7, §8, §9 (`config.hpp`), §13.13, §13.29, §13.32, §13.33, §13.35

> **Build status (2026‑10‑24).** Nothing is changed in the repository yet. The header and both test files listed below were **compiled and run in a scratch directory outside the repository**, extracted directly from this document's code blocks (re-run 2026‑10‑24 after the switch to `<stdint.h>`):
> - `g++.exe (GCC) 15.2.0` from CLion's bundled MinGW, with `-std=c++17 -Wall -Wextra -Wpedantic -Werror -nostdinc++ -fno-exceptions -fno-rtti`;
> - CMake 4.3.1 with the "MinGW Makefiles" generator, using the CMake additions in §4.
>
> Results: both targets built, `config_common_check.exe` exited 0, and the demo exited 0. A C file including `config.hpp` failed with the intended `#error`, and adding `#include <cstdint>` to the test failed under `-nostdinc++` ("No such file or directory"), as spec §1.1 intends. The toolchain is not on the shell PATH; it lives at `C:/Users/BY/AppData/Local/Programs/CLion/bin/mingw/bin` (compiler) and `…/CLion/bin/cmake/win/x64/bin` (CMake), as the existing `cmake-build-debug/CMakeCache.txt` also shows.

## 1. Goal and done criteria

Write the common configuration header exactly once, so the later phases only include it. The header must:
- compile **alone**, with no platform header, no vendor header and no C++ library header (`-nostdinc++`, spec §1.1);
- contain data only (`namespace cfg`, `inline constexpr`);
- reject invalid UART settings at compile time, where they are written.

**Done when:**
- [ ] `Config/config.hpp` exists with the content in §3.
- [ ] `config_common_check` builds with `-Wall -Wextra -Wpedantic -Werror -nostdinc++ -fno-exceptions -fno-rtti`, and every `static_assert` in `Tests/test_config_common.cpp` holds.
- [ ] `config_common_check.exe` exits 0 (one `cfg::defaultFileName` across two translation units).
- [ ] The demo `printer_2` still builds and exits 0.
- [ ] Manual one-off check: a C file that includes `config.hpp` fails with `#error "config.hpp is a C++ header"`.
- [ ] Manual one-off check: adding `#include <cstdint>` to the test makes the check target fail to build; then revert.
- [ ] Results (compiler, commands, exit codes) are recorded in a new `_ProjectHandOff.md` mailbox entry (house rule 5).

## 2. Prerequisite: the rest of Phase 0

The repository is further along than the base plan and handoff say. `config.h` is **already empty**, and `main()` already lives in `main.cpp`, which only includes `config.h`. So base-plan Phase 0 step 1 is already done. What remains:

1. `git mv config.h Config/config.hpp`. The header content is written in step 3.1.
2. In `main.cpp`, change `#include "config.h"` to `#include "config.hpp"`.
3. In `CMakeLists.txt`, add `${CMAKE_CURRENT_SOURCE_DIR}/Config` to `printer_2`'s include directories (§4).
4. Add `.gitignore` with `.idea/`, `cmake-build-*/`, `build/` and `*.exe`. This also stops `main.exe` (currently untracked in the root) from showing up.
5. Build; `printer_2` exits 0.

**Proposed commit:** "Phase 0: move config header to Config/config.hpp, add .gitignore". Doing Phase 0 as its own commit keeps the history readable. Merging it into the Phase 1 commit also works, if BY prefers.

The base plan's build-status note ("`config.h` still holds the `main()` stub") and the same remark in the 2026‑10‑24 handoff entry are out of date. The handoff should get a dated correction note in the Phase 1 entry (house rule 7), and the base plan's note can simply be updated.

## 3. Steps

### 3.1 Write `Config/config.hpp`

This is the content compiled in the scratch check. It follows the spec §9 sketch, with the deviations listed in §5.

```cpp
#pragma once
#ifndef __cplusplus
#error "config.hpp is a C++ header"
#endif

// Common Printer configuration (Config Specification §9). Data only, namespace cfg.

#include <stdint.h>   // compiler-provided C header; no C++ library (spec §1.1)

namespace cfg {

// Feature flags
inline constexpr bool printHexPrefix       = true;
inline constexpr bool doubleScientific     = false;
inline constexpr bool doublePlusInExponent = false;
inline constexpr bool useColorDefault      = true;   // project switch, §5 precedence

// Colour: 16-colour SGR foreground code, emitted as ESC[<sgr>m (§13.13)
struct Color { uint8_t sgr; };
inline constexpr Color black{30},      red{31},        darkGreen{32}, yellow{33},
                       blue{34},       magenta{35},    cyan{36},      lightGray{37},
                       darkGray{90},   brightRed{91},  green{92},     brightYellow{93},
                       brightBlue{94}, brightMagenta{95}, brightCyan{96}, white{97};

// UART settings
enum class Parity      : uint8_t { none, even, odd };
enum class StopBits    : uint8_t { one, two };
enum class FlowControl : uint8_t { none, rtsCts };

struct UartSettings {
    uint32_t baudrate;
    uint32_t bankSize;     // one bank; RAM = 2 x bankSize; 0 = polled, no DMA (§7)
    uint8_t  dataBits;     // data bits, parity bit not counted (Q-1)
    Parity        parity;
    StopBits      stopBits;
    FlowControl   flowControl;
};

inline constexpr uint32_t printerMinBankSize = 4;      // §7, §13.33; 1-3 not yet proven
inline constexpr uint32_t printerMaxBankSize = 4096;   // upper bound only, §7; <= 65535 (CNDTR)

inline constexpr UartSettings uartDefault{115200, 0, 8, Parity::none, StopBits::one, FlowControl::none};

inline constexpr const char* defaultFileName = "noname.log";   // §8, both platforms

// A settings constant is valid when the USART can be programmed from it as written.
constexpr bool isValid(const UartSettings& s) {
    const bool bankOk = s.bankSize == 0
                     || (s.bankSize >= printerMinBankSize && s.bankSize <= printerMaxBankSize);
    const bool bitsOk = s.dataBits == 8 || (s.dataBits == 9 && s.parity == Parity::none);
    return s.baudrate > 0 && bankOk && bitsOk;
}

static_assert(printerMinBankSize >= 1 && printerMinBankSize <= printerMaxBankSize);
static_assert(printerMaxBankSize <= 65535, "DMA CNDTR is a 16-bit register");
static_assert(isValid(uartDefault));

} // namespace cfg
```

Notes on the code:
- **`inline constexpr`** on every variable gives one object per program instead of one per translation unit. The second test file checks this for `defaultFileName` (§3.2).
- **`constexpr` functions are implicitly `inline`,** so `isValid` can live in the header.
- **The header's own `static_assert`s** guard the bounds themselves. Platform headers (Phase 3 and Phase 6) will add `static_assert(cfg::isValid(usartNSettings))` for each of their settings constants.
- **`<stdint.h>`, not `<cstdint>`** (spec §1.1, §13.35). The C header comes with the compiler, holds only typedefs and macros, and costs no RAM and no code; it exists in the bare-metal MC build too. `<cstdint>` is a C++ library header and is forbidden in common code, so the types are written unqualified: `uint8_t`, `uint32_t`.

### 3.2 Write the checks

`Tests/test_config_common.cpp` holds the compile-time checks. It includes **only** `config.hpp`.

```cpp
// Phase 1 check: config.hpp compiles alone, and its constants and isValid() behave as specified.
// Everything here is a static_assert, except the single-address check in main().

#include "config.hpp"

namespace {

using namespace cfg;

constexpr bool sameText(const char* a, const char* b) {
    while (*a != '\0' && *a == *b) { ++a; ++b; }
    return *a == *b;
}

constexpr UartSettings withBank(uint32_t bankSize) {
    UartSettings s = uartDefault;
    s.bankSize = bankSize;
    return s;
}

constexpr UartSettings withBits(uint8_t dataBits, Parity parity) {
    UartSettings s = uartDefault;
    s.dataBits = dataBits;
    s.parity = parity;
    return s;
}

// Colours (spec §9 presets)
static_assert(darkGray.sgr == 90);
static_assert(darkGreen.sgr == 32);
static_assert(brightRed.sgr == 91);

// Bounds
static_assert(printerMinBankSize == 4);
static_assert(printerMaxBankSize <= 65535);

// Bank size: 0 or printerMinBankSize..printerMaxBankSize
static_assert(isValid(uartDefault));
static_assert(isValid(withBank(0)));
static_assert(isValid(withBank(4)));
static_assert(isValid(withBank(printerMaxBankSize)));
static_assert(!isValid(withBank(1)));
static_assert(!isValid(withBank(3)));
static_assert(!isValid(withBank(printerMaxBankSize + 1)));

// Baud rate
static_assert(!isValid(UartSettings{0, 0, 8, Parity::none, StopBits::one, FlowControl::none}));

// Data bits (Q-1: parity not counted)
static_assert(isValid(withBits(8, Parity::even)));
static_assert(isValid(withBits(9, Parity::none)));
static_assert(!isValid(withBits(9, Parity::even)));
static_assert(!isValid(withBits(7, Parity::none)));

// Default file name
static_assert(sameText(defaultFileName, "noname.log"));

} // namespace

const char* const* defaultFileNameAddressHere() { return &cfg::defaultFileName; }
```

`Tests/test_config_common_main.cpp` is the second translation unit, which checks the `inline constexpr` claim at run time:

```cpp
// Phase 1 check, second translation unit: inline constexpr gives one cfg::defaultFileName for
// the whole program, so its address is the same in both translation units.

#include "config.hpp"

const char* const* defaultFileNameAddressHere();

int main() {
    return defaultFileNameAddressHere() == &cfg::defaultFileName ? 0 : 1;
}
```

### 3.3 Build and run

From CLion, or from a shell with the toolchain on the PATH:

```sh
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
build/config_common_check.exe     # expect exit 0
build/printer_2.exe               # expect exit 0
```

Manual guard check, run once and not part of the build: compile a one-line C file `#include "config.hpp"` with `gcc -fsyntax-only -IConfig`, and expect `error: #error "config.hpp is a C++ header"`.

### 3.4 Record and commit

- Add a mailbox entry to `_ProjectHandOff.md`: compiler and version, commands, exit codes, and the dated correction about `config.h` (§2).
- **Proposed commit:** "Phase 1: common config header Config/config.hpp with compile-time checks".

## 4. CMake changes

Only two additions to the existing `CMakeLists.txt`: the `Config/` include directory, and the check target. Phase 2 replaces the check target with `printer_tests` under `ctest` and adds the `PRINTER_PLATFORM` switch and presets. This keeps Phase 1 from building Phase 2's infrastructure early.

```cmake
target_include_directories(printer_2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Config
)

# Phase 1 check: config.hpp alone. Folded into printer_tests in Phase 2.
add_executable(config_common_check
    Tests/test_config_common.cpp
    Tests/test_config_common_main.cpp
)

target_include_directories(config_common_check PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Config
)

target_compile_options(config_common_check PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -nostdinc++ -fno-exceptions -fno-rtti   # spec §1.1: no C++ library in common code
)
```

`-Werror` is applied only to the check target in this phase. Whether the whole project builds with `-Werror` is a Phase 2 question (Q-3).

## 5. Deviations from the spec sketch, and questions for BY

**Deviations** (spelling only, no change in meaning):

| Where | Spec sketch | Here | Why |
|---|---|---|---|
| All variables | `constexpr` | `inline constexpr` | One object per program (base plan Phase 1 step 2) |
| Include | — | `<stdint.h>` | Spec §1.1: no C++ library headers in common code; the sketch now says the same |
| Header start | `#pragma once` | `#pragma once` + `__cplusplus` guard | Spec §2, §13.29 |
| Additions | — | `isValid()` and three `static_assert`s | Base plan Phase 1 step 3 |

**Questions** (each has a provisional answer, already used in the code):

| ID | Question | Provisional answer |
|---|---|---|
| Q-1 | Does `dataBits` count the parity bit? On the F103, the word length (M bit) is 8 or 9 bits *including* parity, so "8 data bits + parity" needs M = 9, and "9 data bits + parity" is impossible. | `dataBits` counts data bits only; valid combinations are 8 (with any parity) and 9 (without parity). 7 data bits with parity (M = 8) is possible on the F103 but excluded for now, matching the base plan's "8 or 9" |
| Q-2 | Phase 0 as its own commit, or merged into Phase 1? | Its own commit (§2) |
| Q-3 | `-Wall -Wextra -Wpedantic -Werror` project-wide from Phase 2 on? | Yes, decided in Phase 2; Phase 1 applies it to the check target only |
| Q-4 | Keep the check as an executable (it has one run-time check), or make it compile-only? | Executable: the single-address check needs two linked translation units |

## 6. Risks

- **Silent copies instead of one object.** If someone later writes plain `constexpr` for a new variable, each translation unit gets its own copy. Nothing breaks for integers, but addresses differ. The two-TU check covers `defaultFileName` only. A code-review rule covers the rest: every `cfg::` variable is `inline constexpr`.
- **`using namespace cfg` in the test** is confined to an anonymous namespace in a test file. Production code always writes `cfg::`.
- **Toolchain not on the PATH.** The §3.3 commands need CLion's MinGW and CMake `bin/` directories on the PATH (paths in the build-status note). Otherwise, build from CLion.

## 7. Next

Phase 2 of the base plan: the `PRINTER_PLATFORM` switch, `CMakePresets.json` (`pc`, `mc`), `config_platform.hpp` selection, the `printer` library target, and `printer_tests` under `ctest`, which absorbs `config_common_check`.
