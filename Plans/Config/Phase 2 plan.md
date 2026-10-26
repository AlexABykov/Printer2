# Phase 2 Plan — Platform selection, CMake and the PC test suite
**Status:** approved by BY(User), 2026‑10‑26 ("good enough"); implemented the same day.  
**Date:** 2026‑10‑26 (revised 2026‑10‑26: review [`Phase 2 Found Issues.md`](Phase%202%20Found%20Issues.md) resolved, §8.1; second review resolved, §8.2)  
**Parent:** [`Config Base Plan.md`](Config%20Base%20Plan.md), Phase 2 (and §4a, the PC test suite)  
**Spec:** [`Config Specification.md`](Config%20Specification.md) §1.1, §11, §12, §13.28, §13.35, §13.36, §13.37, §13.39, §13.41  
**Rules:** [`AGENTS.md`](../../AGENTS.md) §1 (code areas), §2.4 (enforcement), §4 (tests)

> **Build status (2026‑10‑26).** Nothing is changed in the repository yet. Every file in §3 was **built in a scratch copy outside the repository** (`%TEMP%/p2_phase2`), taken from today's working tree plus this document's code blocks. Toolchain: CLion's bundled MinGW `g++.exe (GCC) 15.2.0` and CMake 4.3.1, put on the PATH of the shell (Git Bash). Results:
>
> | Check | Command | Result |
> |---|---|---|
> | Configure | `cmake --preset pc` | configured; `printer` and `printer_2` generated |
> | Build | `cmake --build --preset pc` | `Built target printer`, `Built target printer_2`; no warnings under `-Werror` |
> | Run | `cmake-build-pc/printer_2.exe` | `ConfigTests total=1 passed=1 failed=0`, "✅ ALL TESTS PASSED", exit 0 |
> | Redirected run | `printer_2.exe > out.txt` | exit 0; the summary is in the file |
> | `ctest` | `ctest` in `cmake-build-pc` | `100% tests passed, 0 tests failed out of 1` |
> | Failing check | a `r.check("deliberate failure", false)` added, rebuilt, run | "❌ SOME TESTS FAILED", `- ConfigTests: deliberate failure`, exit 1; then reverted |
> | Include check, header | `#include <array>` added to `config.hpp`, plain `cmake --build` | the build re-ran configure and failed: "`<array>` is not allowed in Common or MC code"; then reverted |
> | Include check, entry point | `#include <array>` appended to `Tests/Tests.hpp` | configure failed the same way; then reverted |
> | Prefix selection | new `Printer/pc_Probe.cpp` with `<iostream>` | picked up by the glob without a manual reconfigure, compiled into `printer`, not checked |
> | Prefix selection | new `Hardware/mc_Probe.cpp` with `<array>`, PC configuration | configure failed: MC files are checked on both platforms; then removed |
> | Bad platform | `cmake … -DPRINTER_PLATFORM=XX` | "PRINTER_PLATFORM must be PC or MC, not 'XX'" |
> | MC preset | `cmake --preset mc` | first error: "arm-none-eabi-g++ was not found on the PATH …"; CMake then adds two follow-up errors (no make program, no compiler) |
> | Include path | `cmake-build-pc/CMakeFiles/printer_2.dir/includes_CXX.rsp` | `-I<repository root>` only, so no vendor header is reachable |
>
> No `arm-none-eabi` toolchain is installed, so only the "not found" path of the `mc` preset was exercised (§5 Q-6).
>
> **Rebuilt after the review (2026‑10‑26, §8).** The same scratch copy was rebuilt with the review fixes: the include check widened to `#include_next`, the guarded code-page restore, and the `enable_testing()` comment. A fresh `cmake --preset pc` and build succeeded; `printer_2.exe`, the redirected run and `ctest` passed as before. Three more checks:
>
> | Check | Result |
> |---|---|
> | `#include_next <array>` in `config.hpp`, plain build | rejected (before the fix, configure passed) |
> | `#include <array>` and `# include<array>` in `config.hpp`, plain build | both still rejected; the clean build passes again after reverting |
> | `Tests/Tests.hpp` removed, `cmake --preset pc` | configure fails: "file STRINGS file "…/Tests/Tests.hpp" cannot be read"; then restored |
> | `#include_next <stdint.h>` in `config.hpp` (second review, §8.2) | the include check **passes** it (allowed header); the compile then fails: "'#include_next' is a GCC extension [-Werror=pedantic]"; reverted, clean build green again |
>
> **Implemented (2026‑10‑26).** The §3 files are in the repository's working tree, uncommitted (BY commits). They were copied from the scratch copy after checking that it matches this document's code blocks byte for byte. `config.hpp` is unchanged. The same toolchain was used, from the repository root. Details are in the 2026‑10‑26 (session 12) handoff entry.
> - **Build and run:** `cmake --preset pc` and `cmake --build --preset pc` succeeded with no warnings. `printer_2.exe` passed 1 of 1 and exited 0, and so did the redirected run. `ctest` passed. The include path is `-IC:/Work/Printer2` only.
> - **MC preset:** `cmake --preset mc` failed with "arm-none-eabi-g++ was not found".
> - **§3.10 checks 1, 2 and 4** gave the expected results, and each was reverted.
> - **§3.10 check 3**, partly: through `cmd /c`, with `chcp 437` and with `chcp 65001`, both plain and redirected, every run exited 0 and the code page was the same afterwards. Seeing ✅ in an interactive `cmd.exe` window is still BY's check, because the agent shell has no visible console.
> - **Finding (§5 Q-8):** outside a shell that has MinGW's `bin` on the PATH, `printer_2.exe` doesn't start. It exits with `0xC0000135` (DLL not found), because it imports `libstdc++-6.dll` and `libgcc_s_seh-1.dll`. Static-link attempts are recorded in Q-8; none is applied.

## 1. Goal and done criteria

Turn today's single-file build into the build that the rest of the slice grows into:
- one CMake configuration per platform, selected by `PRINTER_PLATFORM` and the `pc` / `mc` presets (spec §11);
- link-time platform selection by file prefix (`pc_*` / `mc_*`, spec §13.37);
- the include allow-list, checked at configure time over every file that the MC compiles (spec §1.1, `AGENTS.md` §2.4);
- the PC test suite, run from `main()` (spec §12, §13.41), with `ConfigTests` as its first suite.

After this phase, nothing is compiled by hand anymore: the two Phase 1 test files are part of `printer_2`, and they are rebuilt with every build.

**Done when:**
- [x] `CMakeLists.txt`, `CMakePresets.json` and `cmake/arm-none-eabi.cmake` have the content in §3.1–§3.3.
- [x] `Tests/Tests.hpp`, `Tests/pc/TestResults.hpp` and `Tests/pc/pc_Tests.cpp` exist (§3.4–§3.6); `test_config.cpp` gains `runConfigTests()` (§3.7); `main.cpp` runs the tests (§3.8).
- [x] `cmake --preset pc` and `cmake --build --preset pc` succeed with no warnings under `-Werror`.
- [x] `printer_2` prints the summary with "✅ ALL TESTS PASSED" and exits 0; `ctest` passes.
- [x] Manual checks, each reverted afterwards (§3.10): a failing `check()` prints "❌ SOME TESTS FAILED" with the test's name and exits 1; `#include <array>` in `config.hpp` fails the next plain build.
- [x] The `printer` target's only include directory is the repository root (no vendor header reachable; handoff v0 DoD item).
- [x] `cmake --preset mc` stops with the "arm-none-eabi-g++ was not found" message on a machine without the ARM toolchain.
- [x] Results (compiler, commands, exit codes) are recorded in a new `_ProjectHandOff.md` mailbox entry (`AGENTS.md` §5).

## 2. Starting point

The working tree after Phase 1 (checked 2026‑10‑26):
- `CMakeLists.txt` builds one executable, `printer_2`, from `main.cpp`, with the repository root on its include path.
- `main.cpp` includes `Printer/Config/config.hpp` and returns 0.
- `Printer/Config/config.hpp` is the only library file. `Printer/` has no `.cpp` file, and `Hardware/` doesn't exist yet.
- `Tests/pc/test_config.cpp` and `Tests/pc/test_config_address_probe.cpp` are compiled by hand only.
- `.gitignore` and `Tests/` are **untracked** (handoff session 8, open item 1). Phase 2 edits both test files, so it is easier to review if BY adds them before Phase 2 starts. That is BY's choice; the plan doesn't depend on it.

## 3. Steps

### 3.1 `CMakeLists.txt`

Replaces the current file completely. This is the scratch-built version.

```cmake
cmake_minimum_required(VERSION 3.20)
project(printer_2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Platform (spec §11): one configuration per platform, chosen at configure time ---
set(PRINTER_PLATFORM "PC" CACHE STRING "Target platform: PC or MC")
set_property(CACHE PRINTER_PLATFORM PROPERTY STRINGS PC MC)
if(NOT PRINTER_PLATFORM MATCHES "^(PC|MC)$")
    message(FATAL_ERROR "PRINTER_PLATFORM must be PC or MC, not '${PRINTER_PLATFORM}'")
endif()

# --- Library rules (spec §1.1, AGENTS.md §2) ---
# Common and MC code may include only these system headers.
set(PRINTER_ALLOWED_SYSTEM_HEADERS cstdint stdint.h stddef.h stdarg.h limits.h float.h)

function(printer_check_includes)
    foreach(file IN LISTS ARGN)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${file}")
        file(STRINGS "${file}" lines REGEX "^[ \t]*#[ \t]*include(_next)?[ \t]*<")
        foreach(line IN LISTS lines)
            string(REGEX REPLACE "^[ \t]*#[ \t]*include(_next)?[ \t]*<([^>]+)>.*" "\\2" header "${line}")
            if(NOT header IN_LIST PRINTER_ALLOWED_SYSTEM_HEADERS)
                message(FATAL_ERROR "${file}: <${header}> is not allowed in Common or MC code (AGENTS.md §2)")
            endif()
        endforeach()
    endforeach()
endfunction()

# Warnings are errors in every project target (Phase 1 plan Q-3).
function(printer_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(PRINTER_PLATFORM STREQUAL "MC")
        target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti -fno-threadsafe-statics)
    endif()
endfunction()

# --- Source collection (AGENTS.md §1): link-time platform selection by file prefix ---
file(GLOB_RECURSE library_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Printer/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/Printer/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Hardware/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/Hardware/*.cpp")

set(common_files ${library_files})
list(FILTER common_files EXCLUDE REGEX "/(pc|mc)_[^/]*$")
set(pc_files ${library_files})
list(FILTER pc_files INCLUDE REGEX "/pc_[^/]*$")
set(mc_files ${library_files})
list(FILTER mc_files INCLUDE REGEX "/mc_[^/]*$")

# pc_ headers that shared driver code includes follow the Common rules (AGENTS.md §1).
set(pc_headers_under_common_rules
    "${CMAKE_CURRENT_SOURCE_DIR}/Hardware/pc_chip.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Hardware/pc_STM32F103xE.hpp")
set(existing_pc_common_headers)
foreach(file IN LISTS pc_headers_under_common_rules)
    if(EXISTS "${file}")
        list(APPEND existing_pc_common_headers "${file}")
    endif()
endforeach()

file(GLOB_RECURSE mc_test_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/mc/*")

# Checked on both platforms: everything that is compiled for the MC, plus the shared entry points.
printer_check_includes(
    ${common_files} ${mc_files} ${existing_pc_common_headers} ${mc_test_files}
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Tests.hpp")

if(PRINTER_PLATFORM STREQUAL "PC")
    set(platform_files ${pc_files})
else()
    set(platform_files ${mc_files})
endif()

# --- printer: the library ---
add_library(printer STATIC ${common_files} ${platform_files})
set_target_properties(printer PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(printer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
printer_warnings(printer)

# --- printer_2: the demo; main() runs the platform's test suite first (spec §12, §13.41) ---
if(PRINTER_PLATFORM STREQUAL "PC")
    file(GLOB pc_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/pc/*.cpp")
    list(FILTER pc_test_sources EXCLUDE REGEX "/test_config_mc_host\\.cpp$")

    add_executable(printer_2 main.cpp ${pc_test_sources})
    target_link_libraries(printer_2 PRIVATE printer)
    printer_warnings(printer_2)

    enable_testing()   # PC only: ctest runs the demo with its tests; the MC suite runs on the target
    add_test(NAME printer_2 COMMAND printer_2)
else()
    message(STATUS "PRINTER_PLATFORM=MC: only the printer library is configured; the MC demo and suite come with the MC Hardware plan")
endif()
```

Notes on the code:
- **Which files are checked** follows `AGENTS.md` §1, "a header follows the strictest area of any file that includes it":
  - every Common file: all of `Printer/` and `Hardware/` without a `pc_` / `mc_` prefix, so all of `Printer/Config/` and every `*_regs.hpp` as well;
  - every MC file (`mc_*`), and everything under `Tests/mc/`;
  - the `pc_` headers that shared driver code includes (`pc_chip.hpp`, `pc_<chip>.hpp`). They don't exist yet. The list names them in advance so that the MC Hardware plan can't forget them. A new chip adds its `pc_<chip>.hpp` to the list;
  - `Tests/Tests.hpp`, because `mc_Tests.cpp` will include it, and `main.cpp`, because the MC compiles it too (§5 Q-3).

  The MC files are checked in **both** configurations, so a forbidden include in an `mc_*` file fails the PC build at once and doesn't wait for the first ARM build.
- **Not checked:** `pc_*.cpp` files, `pc_*` headers that only `pc_*.cpp` files include, and `Tests/pc/` (PC code, `AGENTS.md` §3, §4).
- **The check is the base plan's function, widened to `#include_next`** (review P2-03). It reads each file's own `#include <...>` and `#include_next <...>` lines; the header name is now the regex's second group, hence `"\\2"`. An allowed header passes in either spelling. `-Wpedantic -Werror` already rejects `#include_next` in every file that is compiled ("'#include_next' is a GCC extension"). So the widened check matters for files that the current configuration doesn't compile, such as `mc_*` files in the PC configuration, and it reports at configure time. A commented-out `// #include <array>` is not matched, because the regex wants `#` as the first non-blank character.
- **A listed file that is missing fails configure** ("file STRINGS file … cannot be read"), so the check can't pass silently because a file doesn't exist yet (review P2-02, checked in scratch). The files of §3.1–§3.8 can therefore be written in any order. A configure before all listed files exist fails with "cannot be read", which is the intended guard; it is not a state to build in.
- **Sources are collected by glob** (§5 Q-1). `CONFIGURE_DEPENDS` makes a plain build notice a new or removed file, as the scratch build showed with `pc_Probe.cpp`. The prefix filter is the whole of link-time platform selection: on the PC, `printer` compiles the Common files and the `pc_*` files, and the `mc_*` files are never compiled.
- **`printer` has no `.cpp` file yet.** It holds `config.hpp` only, so it is an empty archive. `LINKER_LANGUAGE CXX` is needed because CMake can't infer the language from headers alone. It is a temporary workaround: Phase 4 removes the line when its first `.cpp` lands (review P2-04).
- **`printer_warnings`** applies `-Wall -Wextra -Wpedantic -Werror` to both targets (§5 Q-2), and the MC rules' `-fno-exceptions -fno-rtti -fno-threadsafe-statics` in the MC configuration. They are set per target, so a user project that links `printer` doesn't inherit `-Werror`.
- **The MC configuration defines only `printer`** (§5 Q-6). There is no `mc_Tests.cpp`, no startup code and no linker script yet, so an MC `printer_2` couldn't link. The MC Hardware plan adds it.
- **`test_config_mc_host.cpp` is excluded in advance.** Phase 6 creates it as the compile-only `config_mc_check` target, so it must never become part of `printer_2`. The base plan lists `config_mc_check` under Phase 2, but it has nothing to compile before `config_mc.hpp` exists, so it moves to Phase 6 (§4).
- **`ctest`.** `add_test` runs the whole demo, tests included, and reads its exit code. Under `ctest`, stdout is redirected, which will exercise the VT-mode fallback in Phase 5 (base plan B3).

### 3.2 `cmake/arm-none-eabi.cmake`

The toolchain file for the `mc` preset. In this phase, its job is to fail clearly when the toolchain is missing.

```cmake
# Toolchain file for the mc preset (spec §11): arm-none-eabi-gcc, bare metal.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

find_program(PRINTER_ARM_GXX arm-none-eabi-g++)
if(NOT PRINTER_ARM_GXX)
    message(FATAL_ERROR "arm-none-eabi-g++ was not found on the PATH. The mc preset needs the ARM GNU toolchain; the PC build uses the pc preset.")
endif()
get_filename_component(PRINTER_ARM_BIN "${PRINTER_ARM_GXX}" DIRECTORY)

set(CMAKE_C_COMPILER   "${PRINTER_ARM_BIN}/arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "${PRINTER_ARM_GXX}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)   # no startup code or linker script yet
```

- **`STATIC_LIBRARY` try-compile:** CMake's compiler check would otherwise try to link an executable, which fails without startup code and a linker script (spec §13.31).
- **No CPU flags yet** (`-mcpu=cortex-m3 -mthumb`, `--specs=nosys.specs`, the link without `_sbrk`). They belong to the MC Hardware plan, with the first ARM build, where they can actually be tried.
- **Untested beyond "not found"** (§5 Q-6).

### 3.3 `CMakePresets.json`

```json
{
  "version": 2,
  "configurePresets": [
    {
      "name": "pc",
      "displayName": "PC (host, MinGW)",
      "generator": "MinGW Makefiles",
      "binaryDir": "${sourceDir}/cmake-build-pc",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "PRINTER_PLATFORM": "PC"
      }
    },
    {
      "name": "mc",
      "displayName": "MC (STM32F103, arm-none-eabi)",
      "generator": "MinGW Makefiles",
      "binaryDir": "${sourceDir}/cmake-build-mc",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "PRINTER_PLATFORM": "MC",
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/arm-none-eabi.cmake"
      }
    }
  ],
  "buildPresets": [
    { "name": "pc", "configurePreset": "pc" },
    { "name": "mc", "configurePreset": "mc" }
  ]
}
```

- **Version 2** is what CMake 3.20, the current minimum, reads. Version 2 has no `toolchainFile` field, so the toolchain file is passed as `CMAKE_TOOLCHAIN_FILE`. The minimum version doesn't change.
- **Build directories** `cmake-build-pc/` and `cmake-build-mc/` are already covered by `.gitignore` (`cmake-build-*/`). CLion shows the presets as two profiles (spec §11). Its existing `cmake-build-debug/` profile is not removed by this plan; that is BY's choice in CLion.
- **Generator: MinGW Makefiles** (§5 Q-4), the one Phase 1 and this scratch build used.

### 3.4 `Tests/Tests.hpp`

The only file the two suites share (spec §12, `AGENTS.md` §4). It is checked under the MC rules (§3.1).

```cpp
#pragma once

// Test entry point (spec §12, §13.41). Defined by Tests/pc/pc_Tests.cpp or Tests/mc/mc_Tests.cpp,
// selected at link time. Returns the number of failed tests. Holds no test code.

struct Tests {
    static int runAll();
};
```

### 3.5 `Tests/pc/TestResults.hpp`

The base plan's §4a code, with one addition: `#include <cstddef>` for `size_t`, which the base plan's version got only indirectly.

```cpp
// Tests/pc/TestResults.hpp — the one result type of the PC suite (PC code: std:: allowed).
#pragma once

#include <cstddef>
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

### 3.6 `Tests/pc/pc_Tests.cpp`

The base plan's §4a runner. Two differences:
- the table has only the one suite that exists, `ConfigTests`; Phases 4 and 5 add one row each;
- on Windows, the runner switches the console to UTF-8 for the emoji and restores the previous code page at the end (§5 Q-5).

```cpp
// Tests/pc/pc_Tests.cpp — PC suite runner (base plan §4a).
#include "Tests/Tests.hpp"
#include "Tests/pc/TestResults.hpp"

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

TestResults runConfigTests();

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
#ifdef _WIN32
    const UINT previousCodePage = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);   // the summary's emoji are UTF-8; restored below
#endif

    const Suite suites[] = {
        {"Config Tests", true, runConfigTests},
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

#ifdef _WIN32
    if (previousCodePage != 0) {   // 0: no console attached, nothing to restore
        SetConsoleOutputCP(previousCodePage);
    }
#endif
    return static_cast<int>(global.failed);
}
```

`pc_Tests.cpp` lives in `Tests/pc/`, not in `Printer/` or `Hardware/`, so the library glob doesn't see it. Its `pc_` prefix names its pair, `Tests/mc/mc_Tests.cpp` (spec §12); the selection between them is the test-source glob in §3.1, not the library filter.

### 3.7 `ConfigTests`: the two Phase 1 files

**`Tests/pc/test_config.cpp`.** The `static_assert`s and the anonymous namespace stay **unchanged** (`AGENTS.md` §4: no silent change to what a test checks). Three edits:
1. The header comment loses its "Phase 1 / Phase 2" wording:
   ```cpp
   // ConfigTests: checks of Printer/Config/config.hpp (spec §9).
   // The static_asserts run when this file compiles; runConfigTests() holds the run-time checks.
   ```
2. A second include, after `config.hpp`. `config.hpp` stays first, so the file still shows that the header compiles alone:
   ```cpp
   #include "Printer/Config/config.hpp"
   #include "Tests/pc/TestResults.hpp"
   ```
3. After the anonymous namespace, the suite function with the run-time address check the Phase 1 plan promised (§3.2 there):
   ```cpp
   const char* const* defaultFileNameAddressFromProbe();   // Tests/pc/test_config_address_probe.cpp

   TestResults runConfigTests() {
       TestResults r("ConfigTests");
       r.check("one defaultFileName object per program", &cfg::defaultFileName == defaultFileNameAddressFromProbe());
       return r;
   }
   ```

**`Tests/pc/test_config_address_probe.cpp`.** Code unchanged. The comment's "ConfigTests (Phase 2) compares it" becomes "runConfigTests() compares it":

```cpp
// Address probe for ConfigTests: reports &cfg::defaultFileName as seen from this source file.
// runConfigTests() compares it with the address seen from test_config.cpp. Because the
// variable is inline constexpr, both must be the same object (spec §9): one per program, not
// one per source file.

#include "Printer/Config/config.hpp"

const char* const* defaultFileNameAddressFromProbe() { return &cfg::defaultFileName; }
```

The probe and `runConfigTests()` are declared in the file that uses them, not in a shared header. `-Wall -Wextra` doesn't warn about a definition without a previous declaration (that is `-Wmissing-declarations`, which is not enabled), as the scratch build confirmed.

### 3.8 `main.cpp`

```cpp
#include "Tests/Tests.hpp"

int main() {
    const int failed = Tests::runAll();   // all tests run from main (spec §12, §13.41)
    return failed == 0 ? 0 : 1;
}
```

This is the base plan's §1 program without the `Printer` lines, which arrive with Phase 5. `main.cpp` no longer includes `config.hpp`, because it uses nothing from it. `Printer/Printer.hpp` brings it back in Phase 5.

### 3.9 Build and run

From a shell with the toolchain on the PATH (paths in `AGENTS.md` §5), or from CLion's `pc` profile:

```sh
cmake --preset pc
cmake --build --preset pc
cmake-build-pc/printer_2.exe                # expect the summary, "✅ ALL TESTS PASSED", exit 0
cmake-build-pc/printer_2.exe > out.txt      # expect exit 0 (redirected stdout)
(cd cmake-build-pc && ctest)                # expect 1 of 1 passed
cmake --preset mc                           # without the ARM toolchain: expect "arm-none-eabi-g++ was not found"
```

The Phase 1 hand-compile commands (`g++ … -c Tests/pc/test_config.cpp`) and the hand-build directory `build/` are no longer needed. `build/` stays in `.gitignore`.

### 3.10 Manual checks, each reverted

1. **A failing check.** Add `r.check("deliberate failure", false);` before `return r;` in `runConfigTests()`, then build and run. Expect "❌ SOME TESTS FAILED", `- ConfigTests: deliberate failure` and exit 1. Revert.
2. **A forbidden include.** Add `#include <array>` to `Printer/Config/config.hpp`, then run a plain `cmake --build --preset pc`, without reconfiguring. Expect configure to re-run and fail with "`<array>` is not allowed in Common or MC code". Revert, and build again.
3. **The console** (not in the scratch run). Run `printer_2.exe` from `cmd.exe` and check that ✅ shows as an emoji, not as mojibake, and that `chcp` shows the old code page afterwards (§5 Q-5). Then run it again after `chcp 437` (or the machine's default non-UTF-8 code page). The runner switches to UTF-8 itself, so ✅ should still show correctly, and `chcp` shows 437 afterwards. If the console refused UTF-8, the expected result is mojibake in the summary, but still exit 0 and 437 restored. Repeat both with `printer_2.exe > out.txt`, once after `chcp 65001` and once after `chcp 437`: in both cases exit 0, and `chcp` shows the code page that was set before the run (review P2-01, second review §8.2).
4. **A missing checked file.** Rename `Tests/Tests.hpp` away, then run `cmake --preset pc`. Expect configure to fail with "cannot be read" (review P2-02). Restore it.

### 3.11 Record

- A new mailbox entry in `_ProjectHandOff.md`: compiler and version, the §3.9 commands, the §3.10 results and exit codes.
- Tick the done criteria in §1, and add a dated "Implemented" note to this plan's build-status block, as the Phase 1 plan did.
- **`AGENTS.md` §1** (review P2-08): if BY confirms §5 Q-3, add `main.cpp` to the Common row of the area table in the same revision, so the rule is in the table and not only in this plan. If BY decides otherwise, `main.cpp` leaves the §3.1 check list instead.
- **Commit:** BY's (`AGENTS.md` §5). Suggested message: "Phase 2: platform presets, prefix-based source selection, include allow-list, PC test suite with ConfigTests". It covers `CMakeLists.txt`, `CMakePresets.json`, `cmake/arm-none-eabi.cmake`, `main.cpp`, `Tests/Tests.hpp` and `Tests/pc/` (four files: `TestResults.hpp`, `pc_Tests.cpp`, and the two edited ones), plus `AGENTS.md` if Q-3 is confirmed.

## 4. Differences from the base plan

| Base plan, Phase 2 | This plan | Why |
|---|---|---|
| `config_mc_check` compile-only target (item 4) | Moved to Phase 6; §3.1 only excludes `test_config_mc_host.cpp` from `printer_2` in advance | There is nothing to compile before `config_mc.hpp` exists |
| `mc` preset "stops with a clear message if no ARM toolchain is found" (item 5) | The toolchain file stops with that message; with a toolchain, the MC configuration defines `printer` only | An MC `printer_2` can't link before `mc_Tests.cpp`, the startup code and a linker script exist (MC Hardware plan) |
| `printer`: "static library with the common sources plus the platform's prefixed sources" | The same, collected by glob; for now it holds `config.hpp` only, as an empty archive with `LINKER_LANGUAGE CXX` | No `.cpp` file exists until Phase 4 |
| `printer_check_includes` "applied to every Common and MC source" | Also applied to `main.cpp`, `Tests/Tests.hpp`, `Tests/mc/` and the named `pc_` chip headers | `AGENTS.md` §1: a file follows the strictest area that includes or compiles it |
| `TestResults.hpp` (§4a) | Adds `#include <cstddef>` | `size_t` was only reachable indirectly |
| Runner (§4a) | One suite row; UTF-8 console code page, restored at the end | Only `ConfigTests` exists; the emoji need UTF-8 in a Windows console |
| Warnings (Phase 1 plan Q-3) | `-Wall -Wextra -Wpedantic -Werror` on `printer` and `printer_2` | The Q-3 answer was "decided in Phase 2"; proposed here as Q-2 |

## 5. Questions

Each has a provisional answer, already used in the §3 code.

| ID | Question | Provisional answer |
|---|---|---|
| Q-1 | Collect library and test sources by **glob** (with `CONFIGURE_DEPENDS`) or by **explicit lists**? | **Glob.** "Every `pc_*.cpp` or every `mc_*.cpp`" (`AGENTS.md` §1.1) is a file pattern, and the glob is that pattern. An explicit list would need two edits per new file (the list and the include-check list), and a forgotten entry in the check list would go unnoticed. The cost: CMake re-runs the glob on every build, which takes milliseconds at this size |
| Q-2 | `-Wall -Wextra -Wpedantic -Werror` project-wide (Phase 1 plan Q-3)? | **Yes,** per target on `printer` and `printer_2`, not through `CMAKE_CXX_FLAGS`, so a project that links `printer` doesn't inherit `-Werror` |
| Q-3 | `main.cpp` isn't in `AGENTS.md` §1's area table. Which area is it? | **Common:** the MC compiles it too, and it is included in the check (§3.1). If BY agrees, `AGENTS.md` §1 gets a row (or a word) for it; this plan doesn't change `AGENTS.md` |
| Q-4 | Preset generator: **MinGW Makefiles** or **Ninja** (CLion bundles `ninja.exe`)? | **MinGW Makefiles**, the generator every recorded build so far used. Ninja is faster but isn't on the PATH outside CLion either. Changing it later is one line per preset |
| Q-5 | Should the PC runner switch the Windows console to UTF-8 for the emoji? | **Yes, and restore it** (§3.6). A console's code page outlives the process in `cmd.exe`, so it is put back. The alternative is to leave the console alone and accept mojibake in a non-UTF-8 console. In the scratch run, Git Bash showed the emoji correctly; CLion's run window and `cmd.exe` are not checked yet (§3.10 check 3) |
| Q-6 | With an ARM toolchain present, should the `mc` preset configure `printer` only, or stop with "MC build not available yet"? | **`printer` only** (§3.1), so the first ARM compile of the Common files can happen as soon as a toolchain is installed. This path is **untested**: no ARM toolchain is installed (build-status note) |
| Q-7 | Presets set `CMAKE_BUILD_TYPE=Debug`. Add `pc-release` / `mc-release`? | **Not yet.** One preset per platform, as spec §11 describes. Release presets can come with the first size or speed measurement |
| Q-8 | *Added 2026‑10‑26, during implementation.* `printer_2.exe` imports `libstdc++-6.dll` and `libgcc_s_seh-1.dll`, so it starts only where MinGW's `bin` is on the PATH (CLion, or a prepared shell). Make the exe start from any `cmd.exe`? | **Open, nothing applied.** Tried in scratch, 2026‑10‑26:<br>• `-static` doesn't link with CLion's MinGW (undefined `__intrinsic_setjmpex` in `libwinpthread`, `__ms_vsnprintf` in `libmsvcrt`).<br>• `-static-libgcc -static-libstdc++` links, but the exe still imports `libwinpthread-1.dll`, and adding `-Wl,-Bstatic -lwinpthread` doesn't change that.<br>The remaining options are: copy the three DLLs next to the exe after each build, or keep the rule "run it from CLion, or with MinGW's `bin` on the PATH". The plan isn't changed, so until BY decides, the rule applies. It matters for the interactive console checks (§3.10 check 3, base plan B3) |

## 6. Risks

- **The named `pc_` chip headers are a list, not a pattern.** A new chip's `pc_<chip>.hpp` must be added to `pc_headers_under_common_rules` by hand. A missing entry means that header isn't checked, and the `EXISTS` filter makes a typo silent too. The MC Hardware plan must say so where it adds the chip files.
- **The check sees only `#include <...>` and `#include_next <...>` lines** (base plan §7, `AGENTS.md` §2.4): not what an allowed header includes in turn, not `malloc` or `new` through an allowed header, and not a Common file including `config_pc.hpp` or `config_mc.hpp` through `"..."`. Code review still covers those.
- **The MC configuration with a real toolchain is untried** (§5 Q-6). The first ARM build (MC Hardware plan) may need changes to the toolchain file, such as CPU flags or `--specs`.
- **CLion and presets.** Once `CMakePresets.json` exists, CLion offers the `pc` and `mc` profiles. If the `mc` profile is enabled, CLion reports the "not found" error on every reload until the ARM toolchain is installed. It can be disabled in CLion's CMake settings.
- **`-Werror` and a newer compiler.** A future compiler may add warnings to `-Wall` / `-Wextra` that break the build without any code change. That is the intended trade-off, and the fix is in the code, not in the flags.
- **Emoji in the source.** `pc_Tests.cpp` holds UTF-8 literals. GCC reads source and writes strings as UTF-8 by default, so this works with MinGW. A compiler with another default charset would need `-finput-charset` / `-fexec-charset`. The MC suite prints plain text (`AGENTS.md` §4), so this stays PC-only.

## 7. Next

Phase 3 of the base plan: `Printer/Config/config_pc.hpp` (spec §9). It is data only, under the Common rules, so the §3.1 include check covers it from the moment it is created. `ConfigTests` gains its compile-time checks, or a new PC test file does, which is Phase 3's choice.

## 8. Review resolution (2026‑10‑26)

### 8.1 First review

The first review (committed as `31013f4`; the file [`Phase 2 Found Issues.md`](Phase%202%20Found%20Issues.md) now holds the second review) judged the plan approvable with small fixes. Each finding was checked against the scratch build before deciding.

| ID | Finding | Resolution |
|---|---|---|
| P2-01 | `SetConsoleOutputCP(previousCodePage)` with `previousCodePage == 0` when no console is attached | **Fixed** in §3.6: the restore runs only for a non-zero code page. §3.10 check 3 now covers redirected runs under two code pages |
| P2-02 | A listed file that doesn't exist yet would pass the check silently, so the order of writing the files matters | **No change needed.** The premise doesn't hold: `file(STRINGS)` on a missing file fails configure ("cannot be read"), checked in scratch (build-status note). Recorded in the §3.1 notes, and added as §3.10 check 4 |
| P2-03 | `#include_next <header>` bypasses the check | **Fixed in the code**, not only documented: the regex accepts `include(_next)?`, and the header is the second group (`"\\2"`). Checked in scratch: `#include_next <array>` is now rejected, and `#include <array>` and `# include<array>` still are. §6 updated |
| P2-04 | `LINKER_LANGUAGE CXX` on an empty archive is a workaround | **Fixed** as a note in §3.1: Phase 4 removes it with its first `.cpp`. The optional `Printer/PrinterVersion.cpp` is **not** added: it would be a file with no spec basis, written only to fill the archive |
| P2-05 | `add_test(COMMAND printer_2)` vs `$<TARGET_FILE:printer_2>` | **No change.** The review calls it style only. A target name in `add_test(COMMAND …)` is replaced by the target's file, which is documented CMake behaviour, and a rename would also change the test's `NAME` in the same line |
| P2-06 | `enable_testing()` inside the `if` | **Fixed** with a comment on the line (§3.1). It stays inside the `if`, because `ctest` has nothing to run in the MC configuration |
| P2-07 | The base plan still describes the original Phase 2 | **Fixed:** a dated note in the base plan's Phase 2 points to this plan's §4 |
| P2-08 | `main.cpp` is checked but not in `AGENTS.md` §1 | **Fixed** as an implementation step (§3.11), depending on BY's answer to §5 Q-3. `AGENTS.md` is not changed before that decision |

**Stale parts of the review** (no action). Its section on the earlier `Found Issues.md` lists F-10 as open and D-1 / D-2 of implementation review 000 as "worth fixing in the next spec revision". All three were already resolved:
- F-10 is documented as a limit (spec §13.59, session 6);
- D-1 and D-2 are decisions 13.61 (`baudRate`) and 13.62 (`auto`), from session 8.

F-13 (the manual include check) is resolved by this phase, as the review says.

### 8.2 Second review

The second review (the current `Phase 2 Found Issues.md`) checked §8.1 and judged the plan **approvable**, with no re-review needed. Its points:

| Point | Resolution |
|---|---|
| Confirm P2-07 in the base plan | **Confirmed:** the dated note is under the base plan's Phase 2 heading. Its wording "also accepts `#include_next` lines" became "also reads", because the check reads those lines and still rejects a header that isn't allowed |
| Confirm P2-08 in `AGENTS.md` | **Confirmed as designed:** `AGENTS.md` doesn't mention `main.cpp` yet. The edit waits for BY's answer to §5 Q-3 (§3.11) |
| Positive case: `#include_next <stdint.h>` must build | **Checked in scratch, with a different result than the review expected.** The include check passes it, as it should. The compile then fails, but because of `-Wpedantic -Werror` ("'#include_next' is a GCC extension"), not because of the regex. So no project file that is compiled can use `#include_next` at all. The widened check still matters for files the current configuration doesn't compile. Recorded in the build-status note and the §3.1 notes; no code change |
| A non-redirected run after `chcp 437` | **Added** to §3.10 check 3. The expected result differs from the review's: ✅ shows correctly, because the runner sets UTF-8 itself, and 437 is restored afterwards. Mojibake is expected only if the console refuses UTF-8 |
| Wording: "the order … doesn't matter" | **Fixed** in the §3.1 notes: files can be written in any order, and a configure before all of them exist fails, which is the intended guard |
