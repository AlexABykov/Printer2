# Phase 1 Plan — `Config/config.hpp` (common configuration)
**Status:** Draft, awaiting BY's approval.  
**Date:** 2026‑10‑24 (revised 2026‑10‑24: `<cstdint>`, `#pragma once` only, include allow-list instead of `-nostdinc++`)  
**Parent:** [`Config Base Plan.md`](Config%20Base%20Plan.md), Phase 1 (and the remainder of Phase 0, §2 below)  
**Spec:** [`Config Specification.md`](Config%20Specification.md) §1.1, §2, §5, §7, §8, §9 (`config.hpp`), §13.13, §13.29, §13.32, §13.33, §13.35  
**Rules:** [`AGENTS.md`](../../AGENTS.md) §2 (MC and Common code)

> **Build status (2026‑10‑24).** Nothing is changed in the repository yet. The header, both test files and the CMake additions below were **built and run in a scratch directory outside the repository**, with the header and tests extracted from this document's code blocks:
> - `g++.exe (GCC) 15.2.0` from CLion's bundled MinGW, with `-std=c++17 -Wall -Wextra -Wpedantic -Werror`;
> - CMake 4.3.1 with the "MinGW Makefiles" generator.
>
> Results:
> - both targets built; `config_common_check.exe` and the demo both exited 0;
> - adding `#include <array>` to `config.hpp` made the next plain `cmake --build` fail at configure time with "`<array>` is not allowed in Common or MC code".
>
> The toolchain is not on the shell PATH. It lives at `C:/Users/BY/AppData/Local/Programs/CLion/bin/mingw/bin` (compiler) and `…/CLion/bin/cmake/win/x64/bin` (CMake), as the existing `cmake-build-debug/CMakeCache.txt` also shows.

## 1. Goal and done criteria

Write the common configuration header exactly once, so the later phases only include it. The header must:
- compile **alone**, with no platform header and no vendor header;
- include no system header outside the allow-list (`<cstdint>` and the compiler's C headers, `AGENTS.md` §2);
- contain data only (`namespace cfg`, `inline constexpr`);
- reject invalid UART settings at compile time, where they are written.

**Done when:**
- [ ] `Config/config.hpp` exists with the content in §3.1.
- [ ] The CMake include check (§4) passes for `config.hpp`.
- [ ] `config_common_check` builds with `-Wall -Wextra -Wpedantic -Werror`, and every `static_assert` in `Tests/pc/test_config_common.cpp` holds.
- [ ] `config_common_check.exe` exits 0 (one `cfg::defaultFileName` across two translation units).
- [ ] The demo `printer_2` still builds and exits 0.
- [ ] Manual one-off check: adding `#include <array>` to `config.hpp` makes the next build fail with "not allowed in Common or MC code"; then revert.
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

This is the content built in the scratch check. Spec §9 now shows the same code (§5).

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
- **`<cstdint>`** is the one C++ library header allowed in Common and MC code (`AGENTS.md` §2). It holds only typedefs, costs no RAM and no code, and is also available in a bare-metal ARM toolchain. The types are written unqualified (`uint8_t`), as the "no `std::`" rule asks. Strictly, the standard guarantees only `std::uint8_t` from `<cstdint>`. GCC's `<cstdint>` includes `<stdint.h>`, which declares the global names as well, so the unqualified spelling works with GCC, and it is the common practice in embedded C++.
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

These are PC-suite tests (`AGENTS.md` §4). They may use anything the PC offers. They happen to need nothing beyond `config.hpp`, which also shows that the header compiles alone.

### 3.3 Build and run

From CLion, or from a shell with the toolchain on the PATH:

```sh
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
build/config_common_check.exe     # expect exit 0
build/printer_2.exe               # expect exit 0
```

Manual check, run once and then reverted: add `#include <array>` to `config.hpp`, run `cmake --build build`, and expect a configure error naming `<array>`.

### 3.4 Record and commit

- Add a mailbox entry to `_ProjectHandOff.md`: compiler and version, commands, exit codes, and the dated correction about `config.h` (§2).
- **Proposed commit:** "Phase 1: common config header Config/config.hpp with compile-time checks".

## 4. CMake changes

Three additions to the existing `CMakeLists.txt`:
1. the include allow-list check;
2. the `Config/` include directory;
3. the check target.

Phase 2 replaces the check target with `printer_tests` under `ctest`, applies the include check to every Common and MC source, and adds the `PRINTER_PLATFORM` switch and presets.

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

printer_check_includes(${CMAKE_CURRENT_SOURCE_DIR}/Config/config.hpp)

target_include_directories(printer_2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Config
)

# Phase 1 check (PC suite): config.hpp alone. Folded into printer_tests in Phase 2.
add_executable(config_common_check
    Tests/pc/test_config_common.cpp
    Tests/pc/test_config_common_main.cpp
)

target_include_directories(config_common_check PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Config
)

target_compile_options(config_common_check PRIVATE -Wall -Wextra -Wpedantic -Werror)
```

How the include check works:
- It runs at configure time and reads each listed file's `#include <...>` lines. Project includes (`#include "..."`) are not checked.
- `CMAKE_CONFIGURE_DEPENDS` makes CMake re-run the check whenever a listed file changes, so a forbidden include fails the very next build.
- It checks each file's own lines, not what an allowed header includes in turn. That is the intent: the allow-listed headers are known to be type-only.

`-Werror` is applied only to the check target in this phase. Whether the whole project builds with `-Werror` is a Phase 2 question (Q-3).

## 5. Agreed with BY (2026‑10‑24), and questions

The earlier deviations from the spec sketch were settled as follows, and spec §9 now shows the Phase 1 code:

| Item | Decision |
|---|---|
| `constexpr` vs `inline constexpr` | `inline constexpr` everywhere |
| `<stdint.h>` vs `<cstdint>` | `<cstdint>`, types unqualified. "It works fine in similar SIL projects." `-nostdinc++` is therefore replaced by the include allow-list (§4) |
| `__cplusplus` guard | Dropped: `#pragma once` alone; the `.hpp` name says C++17, not C-compatible |
| `isValid()` and the three `static_assert`s | Added to `config.hpp` and to the spec sketch |

**Questions** (each has a provisional answer, already used in the code):

| ID | Question | Provisional answer |
|---|---|---|
| Q-1 | Does `dataBits` count the parity bit? On the F103, the word length (M bit) is 8 or 9 bits *including* parity, so "8 data bits + parity" needs M = 9, and "9 data bits + parity" is impossible. | `dataBits` counts data bits only; valid combinations are 8 (with any parity) and 9 (without parity). 7 data bits with parity (M = 8) is possible on the F103 but excluded for now, matching the base plan's "8 or 9" |
| Q-2 | Phase 0 as its own commit, or merged into Phase 1? | Its own commit (§2) |
| Q-3 | `-Wall -Wextra -Wpedantic -Werror` project-wide from Phase 2 on? | Yes, decided in Phase 2; Phase 1 applies it to the check target only |
| Q-4 | Keep the check as an executable (it has one run-time check), or make it compile-only? | Executable: the single-address check needs two linked translation units |
| Q-5 | Allow-list contents. Besides `<cstdint>`, should the other type-only C++ wrappers (`<cstddef>`, `<climits>`, `<cfloat>`, `<cstdarg>`) be allowed too, for consistency? | Not yet: only `<cstdint>` was asked for. The C forms (`<stddef.h>`, `<stdarg.h>`, …) cover the same needs |

## 6. Risks

- **Silent copies instead of one object.** If someone later writes plain `constexpr` for a new variable, each translation unit gets its own copy. Nothing breaks for integers, but addresses differ. The two-TU check covers `defaultFileName` only. A code-review rule covers the rest: every `cfg::` variable is `inline constexpr`.
- **The include check only sees what it is given.** In Phase 1 that is `config.hpp`. Phase 2 must pass every Common and MC source to `printer_check_includes`, or new files go unchecked.
- **Unqualified names from `<cstdint>`** rely on GCC providing the global names (§3.1 notes). Both the host MinGW and `arm-none-eabi-gcc` do. A different compiler would need a check.
- **`using namespace cfg` in the test** is confined to an anonymous namespace in a test file. Production code always writes `cfg::`.
- **Toolchain not on the PATH.** The §3.3 commands need CLion's MinGW and CMake `bin/` directories on the PATH (paths in the build-status note). Otherwise, build from CLion.

## 7. Next

Phase 2 of the base plan: the `PRINTER_PLATFORM` switch, `CMakePresets.json` (`pc`, `mc`), `config_platform.hpp` selection, the `printer` library target, the include check over all Common and MC sources, and `printer_tests` (PC suite) under `ctest`, which absorbs `config_common_check`.
