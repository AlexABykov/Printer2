# Phase 1 Plan — `Config/config.hpp` (common configuration)
**Status:** Draft, awaiting BY's approval.  
**Date:** 2026‑10‑24 (revised 2026‑10‑24: `<cstdint>`, `#pragma once` only; CMake limited to the `Config/` include path, include check and test target moved to Phase 2)  
**Parent:** [`Config Base Plan.md`](Config%20Base%20Plan.md), Phase 1 (and the remainder of Phase 0, §2 below)  
**Spec:** [`Config Specification.md`](Config%20Specification.md) §1.1, §2, §5, §7, §8, §9 (`config.hpp`), §13.13, §13.29, §13.32, §13.33, §13.35  
**Rules:** [`AGENTS.md`](../../AGENTS.md) §2 (MC and Common code)

> **Build status (2026‑10‑24).** Nothing is changed in the repository yet. The header and both test files below were **built and run in a scratch directory outside the repository**, extracted from this document's code blocks:
> - The demo was built through CMake 4.3.1 ("MinGW Makefiles"), with the one `CMakeLists.txt` change from §4.
> - The checks were compiled by hand with `g++.exe (GCC) 15.2.0` from CLion's bundled MinGW, using the §3.3 command (`-std=c++17 -Wall -Wextra -Wpedantic -Werror`).
>
> Results: `printer_2.exe` and `config_common_check.exe` both exited 0, and `config.hpp` includes only `<cstdint>`.
>
> The toolchain is not on the shell PATH. It lives at `C:/Users/BY/AppData/Local/Programs/CLion/bin/mingw/bin` (compiler) and `…/CLion/bin/cmake/win/x64/bin` (CMake), as the existing `cmake-build-debug/CMakeCache.txt` also shows.

## 1. Goal and done criteria

Write the common configuration header exactly once, so the later phases only include it. The header must:
- compile **alone**, with no platform header and no vendor header;
- include no system header outside the allow-list (`<cstdint>` and the compiler's C headers, `AGENTS.md` §2);
- contain data only (`namespace cfg`, `inline constexpr`);
- reject invalid UART settings at compile time, where they are written.

The build system changes only as far as the moved header requires. The include allow-list check and the test target arrive with Phase 2, which rebuilds `CMakeLists.txt` anyway.

**Done when:**
- [ ] `Config/config.hpp` exists with the content in §3.1.
- [ ] `CMakeLists.txt` differs from today's file only by the `Config/` include directory (§4).
- [ ] The demo `printer_2` builds through CMake and exits 0.
- [ ] The checks compile by hand with the §3.3 command, every `static_assert` in `Tests/pc/test_config_common.cpp` holds, and `config_common_check.exe` exits 0 (one `cfg::defaultFileName` across two translation units).
- [ ] `config.hpp`'s only `#include <...>` is `<cstdint>` (manual check, §3.3).
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

This is the content built in the scratch check. Spec §9 shows the same code (§5).

```cpp
#pragma once

// Common Printer configuration (Config Specification §9). Data only, namespace cfg.

#include <cstdint>

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
    uint32_t    baudrate;
    uint32_t    bankSize;     // one bank; RAM = 2 x bankSize; 0 = polled, no DMA (§7)
    uint8_t     dataBits;     // data bits, parity bit not counted (Q-1)
    Parity      parity;
    StopBits    stopBits;
    FlowControl flowControl;
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
- **`<cstdint>`** is the one C++ library header allowed in Common and MC code (`AGENTS.md` §2). It holds only typedefs, costs no RAM and no code, and is also available in a bare-metal ARM toolchain. The types are written unqualified (`uint8_t`), as the "no `std::`" rule asks.
  - Strictly, the standard guarantees only `std::uint8_t` from `<cstdint>`. GCC's `<cstdint>` includes `<stdint.h>`, which declares the global names as well, so the unqualified spelling works with GCC, and it is the common practice in embedded C++.
- **`#pragma once` only.** The `.hpp` extension already says the header is C++17 and not C-compatible, so no `__cplusplus` guard is added.

### 3.2 Write the checks (PC suite)

`Tests/pc/test_config_common.cpp` holds the compile-time checks. It includes **only** `config.hpp`.

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

`Tests/pc/test_config_common_main.cpp` is the second translation unit, which checks the `inline constexpr` claim at run time:

```cpp
// Phase 1 check, second translation unit: inline constexpr gives one cfg::defaultFileName for
// the whole program, so its address is the same in both translation units.

#include "config.hpp"

const char* const* defaultFileNameAddressHere();

int main() {
    return defaultFileNameAddressHere() == &cfg::defaultFileName ? 0 : 1;
}
```

These are PC-suite tests (`AGENTS.md` §4). They may use anything the PC offers. They happen to need nothing beyond `config.hpp`, which also shows that the header compiles alone. In this phase they aren't part of any CMake target; Phase 2 adds them to `printer_tests`.

### 3.3 Build and run

From a shell with the toolchain on the PATH (or from CLion for the demo):

```sh
# Demo, through CMake
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
build/printer_2.exe                                   # expect exit 0

# Checks, compiled by hand (no CMake target until Phase 2)
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -IConfig \
    Tests/pc/test_config_common.cpp Tests/pc/test_config_common_main.cpp \
    -o build/config_common_check.exe
build/config_common_check.exe                         # expect exit 0

# Include check, by hand (automated in Phase 2)
grep -n '#include <' Config/config.hpp                # expect only <cstdint>
```

The check binary lands in `build/`, which `.gitignore` excludes.

### 3.4 Record and commit

- Add a mailbox entry to `_ProjectHandOff.md`: compiler and version, commands, exit codes, and the dated correction about `config.h` (§2).
- **Proposed commit:** "Phase 1: common config header Config/config.hpp with compile-time checks". It contains `Config/config.hpp` and the two `Tests/pc/` files; `CMakeLists.txt` already changed in the Phase 0 commit.

## 4. CMake change

One change, needed because the header moves into `Config/`: `main.cpp` must still find it. The rest of today's `CMakeLists.txt` stays as it is.

```cmake
target_include_directories(printer_2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Config
)
```

This belongs to the Phase 0 remainder (§2 step 3). It is the only `CMakeLists.txt` edit before Phase 2. The include allow-list check (`printer_check_includes`) and the test target are specified, already tested, in the base plan's Phase 2.

## 5. Agreed with BY (2026‑10‑24), and questions

The earlier deviations from the spec sketch were settled as follows, and spec §9 now shows the Phase 1 code:

| Item | Decision |
|---|---|
| `constexpr` vs `inline constexpr` | `inline constexpr` everywhere |
| `<stdint.h>` vs `<cstdint>` | `<cstdint>`, types unqualified. "It works fine in similar SIL projects." Enforcement is an include allow-list (Phase 2) instead of `-nostdinc++` |
| `__cplusplus` guard | Dropped: `#pragma once` alone; the `.hpp` name says C++17, not C-compatible |
| `isValid()` and the three `static_assert`s | Added to `config.hpp` and to the spec sketch |
| CMake in Phase 1 | Only the `Config/` include path; the include check and the test target move to Phase 2 |

**Questions** (each has a provisional answer, already used in the code):

| ID | Question | Provisional answer |
|---|---|---|
| Q-1 | Does `dataBits` count the parity bit? On the F103, the word length (M bit) is 8 or 9 bits *including* parity, so "8 data bits + parity" needs M = 9, and "9 data bits + parity" is impossible. | `dataBits` counts data bits only; valid combinations are 8 (with any parity) and 9 (without parity). 7 data bits with parity (M = 8) is possible on the F103 but excluded for now, matching the base plan's "8 or 9" |
| Q-2 | Phase 0 as its own commit, or merged into Phase 1? | Its own commit (§2) |
| Q-3 | `-Wall -Wextra -Wpedantic -Werror` project-wide from Phase 2 on? | Yes, decided in Phase 2; in Phase 1, the hand-compiled checks use it |
| Q-4 | Keep the check as an executable (it has one run-time check), or make it compile-only? | Executable: the single-address check needs two linked translation units |
| Q-5 | Allow-list contents for the Phase 2 check. Besides `<cstdint>`, should the other type-only C++ wrappers (`<cstddef>`, `<climits>`, `<cfloat>`, `<cstdarg>`) be allowed too, for consistency? | Not yet: only `<cstdint>` was asked for. The C forms (`<stddef.h>`, `<stdarg.h>`, …) cover the same needs |

## 6. Risks

- **Silent copies instead of one object.** If someone later writes plain `constexpr` for a new variable, each translation unit gets its own copy. Nothing breaks for integers, but addresses differ. The two-TU check covers `defaultFileName` only. A code-review rule covers the rest: every `cfg::` variable is `inline constexpr`.
- **No automatic include check until Phase 2.** Between Phase 1 and Phase 2, a forbidden include in `config.hpp` is caught only by the manual `grep` (§3.3) and by review.
- **The checks are not rebuilt automatically.** They have no CMake target in Phase 1, so a later edit to `config.hpp` must be followed by the §3.3 `g++` command by hand, until Phase 2 adds `printer_tests`.
- **Unqualified names from `<cstdint>`** rely on GCC providing the global names (§3.1 notes). Both the host MinGW and `arm-none-eabi-gcc` do. A different compiler would need a check.
- **`using namespace cfg` in the test** is confined to an anonymous namespace in a test file. Production code always writes `cfg::`.
- **Toolchain not on the PATH.** The §3.3 commands need CLion's MinGW and CMake `bin/` directories on the PATH (paths in the build-status note). Otherwise, build the demo from CLion.

## 7. Next

Phase 2 of the base plan:
- the `PRINTER_PLATFORM` switch and `CMakePresets.json` (`pc`, `mc`);
- `config_platform.hpp` selection and the `printer` library target;
- the include allow-list check (`printer_check_includes`) over every Common and MC source;
- `printer_tests` (PC suite) under `ctest`, which picks up the two Phase 1 test files.
