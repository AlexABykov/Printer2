# AGENTS.md — rules for working on Printer2

This file is for anyone who changes code in this repository: AI agents and human developers alike. It holds the **code rules** for each platform. Design lives in the plans, and live status lives in the handoff:

| Read | For |
|---|---|
| `_ProjectHandOff.md` | Current state, open decisions, session mailbox (newest entry first). Read it before starting |
| `Plans/Config/Config Specification.md` | The design. Its §13 "Decisions" table is authoritative |
| `Plans/Config/Config Base Plan.md` and `Plans/Config/Phase N plan.md` | What to build next, and how to verify it |
| `README.md` | The project's purpose |

If this file and the specification disagree, the specification wins, and this file must be fixed.

## 1. Code areas

Every source file belongs to exactly one area. The area decides which rules apply.

| Area | Where | Rules |
|---|---|---|
| **Common** | All of `Printer/Config/` (`config.hpp`, and also `config_pc.hpp` / `config_mc.hpp`, so that one include check covers the directory); `Printer/` and `Hardware/` files without a `pc_` / `mc_` prefix; `*_regs.hpp` | §2: MC and Common rules |
| **MC** | `mc_*` files, including `mc_chip.hpp` and `mc_<chip>.hpp` | §2: MC and Common rules |
| **PC** | `pc_*.cpp` files; `pc_*` headers included **only** by `pc_*.cpp` files | §3: PC rules |
| **PC tests** | `Tests/pc/` | §4: Tests |
| **MC tests** | `Tests/mc/` | §4: Tests |

**A header follows the strictest area of any file that includes it.** For example:
- Shared driver code includes `pc_chip.hpp` and `pc_<chip>.hpp` on the PC, so those follow the Common rules.
- `config_pc.hpp` is included only by `pc_*` files, but it lives in `Printer/Config/` and stays data-only under the Common rules, like the rest of that directory.

In practice, only `pc_*.cpp` files (and headers private to them) are really free.

### 1.1 Who includes what, and how a platform is selected

- **Common `.cpp` files include only common headers**: `Printer/Config/config.hpp`, never `config_pc.hpp` or `config_mc.hpp`.
- **Platform `.cpp` files** (`pc_*`, `mc_*`) include their own platform config and chip selection (`pc_chip.hpp` / `mc_chip.hpp`) directly.
- **The platform is selected by the build, not by macros.** `CMakeLists.txt` compiles the common files plus either every `pc_*.cpp` or every `mc_*.cpp` file, so the platform difference is resolved by the linker. A build with both prefixes fails to link with duplicate symbols.
- **Other build systems** (IAR, CubeMX-generated projects) must do the same when collecting files: an MC project excludes every `pc_*.cpp`, and a PC project excludes every `mc_*.cpp`. The prefix makes that one file pattern, but such tools may need it set up by hand.
- All includes are written from the repository root: `"Printer/Printer.hpp"`, `"Printer/Config/config.hpp"`, `"Hardware/PrinterInit.hpp"`.

## 2. MC and Common code rules

The microcontroller has **no operating system**, very little RAM, and a poor heap allocator with no defragmentation. Even with free heap, allocating anything larger than a byte can't be relied on. Common code runs on the MC too, so the same rules apply to it. (Spec §1.1, decision 13.35.)

### 2.1 Forbidden

1. **The C++ standard library**, except `<cstdint>` (§2.2). No `std::` anything, and no other C++ library header: not `<cstring>`, `<array>`, `<new>`, `<type_traits>`, `<algorithm>`, `<iostream>`, `<string>`, `<vector>`, and so on.
2. **The heap.** No `new`, `delete`, `malloc`, `free`, or anything that calls them. All storage is static and sized at compile time.
3. **Exceptions and RTTI.** No `throw`, `try`, `dynamic_cast` or `typeid`.
4. **Globals with non-trivial destructors.** Such a global is registered through `__cxa_atexit`, and `atexit` may allocate. Check each global's class with `static_assert(__has_trivial_destructor(T))`. That built-in works in g++ 15.2; `__is_trivially_destructible` does not exist there.
5. **Function-local statics with dynamic initialization.** Their guards pull in `__cxa_guard_*`.
6. **Vendor and CMSIS headers.** The project is bare metal (decision 13.30). Register layouts are written from the reference manual (RM0008 for the STM32F103), and names follow it: `USART2`, `RCC`, `CR1`, `TXE`.
7. **`reinterpret_cast` in constant expressions.** Register addresses are `constexpr uintptr_t` integers, cast to register-block pointers at run time in the chip accessors.

### 2.2 Allowed

- **System headers on the allow-list only**, which hold types and macros only and cost no RAM and no code:
  - `<cstdint>`, the one allowed C++ header;
  - the compiler's own freestanding C headers `<stdint.h>`, `<stddef.h>`, `<stdarg.h>`, `<limits.h>` and `<float.h>`.

  Types are written unqualified: `uint8_t`, `uint32_t`, `size_t`.
- Compiler built-ins, such as `__has_trivial_destructor` and `__builtin_memcpy`, where a standard header would otherwise be needed.
- `constexpr`, templates, `enum class`, `static_assert` and plain classes with virtual functions (no virtual destructors).

### 2.3 Required style

- **Configuration is data:** `inline constexpr` variables in `namespace cfg`, with no assignments at namespace scope.
- **Streams and `nullSink` are constant-initialized** (`constexpr` constructors), so they are safe to use before boot. `prt1` is the one dynamically initialized global, because its constructor boots the hardware.
- **Headers are `.hpp`** (C++17, not C-compatible) and start with `#pragma once` alone.
- **Naming** (spec §2): types `PascalCase`; instances, functions and `constexpr` values `camelCase`; macros `SCREAMING_SNAKE_CASE`, only when really needed; physical quantities may end in a unit suffix (`cpuFrequency_MHz`).

### 2.4 Enforcement

- **CMake include allow-list.** At configure time, CMake reads every `#include <...>` line of each Common and MC file and fails if the header isn't on the §2.2 list. Editing a checked file re-runs the check on the next build. Project includes (`#include "..."`) are not checked. MC builds also add `-fno-exceptions -fno-rtti -fno-threadsafe-statics`.
- The MC link provides **no `_sbrk`**, so heap use is a link error, not a runtime failure.
- The include check doesn't catch `malloc` through an allowed C header, or a call to `new`, on the PC build. Until the MC build exists, code review covers that: no `new`, `malloc` or `std::` outside PC files.

## 3. PC code rules

The PC platform runs on a big developer's machine with a full operating system. PC code is free to use:

- the full C++ standard library (`std::cout`, `std::ofstream`, `std::string`, containers, `std::abort()`, …);
- the heap, exceptions and RTTI;
- operating-system APIs (Windows API, POSIX).

The **one boundary:** no `std` type, OS type or PC library header may appear in any header that Common code includes (§1). PC details stay inside `pc_*.cpp` files. Example: the PC `FileStream` keeps its `std::ofstream` objects inside `pc_FileStream.cpp`, and the common `FileStream.hpp` holds only an index.

## 4. Tests

There are **two absolutely independent test suites, one per platform.** They share no test code, no test helpers and no test framework.

| Suite | Where | Rules |
|---|---|---|
| **PC** | `Tests/pc/` | Our own simple suite, similar to the old project's, with no external test framework (decision 13.39). Otherwise it may use everything the PC offers: the standard library, files, processes, the Windows or Linux API. The summary may use emoji. It also tests the Common code, compiled for the host. That a Common file is clean is checked by the CMake include check (§2.4), not by the tests |
| **MC** | `Tests/mc/` | A brand-new suite, written later (decision 13.39). It does its own job, under the MC and Common rules (§2), and prints plain text, not emoji |

- **Tests run from `main()`** (decision 13.41). There is no separate test executable. `Tests/Tests.hpp` declares `struct Tests { static int runAll(); };`, the only file the two suites share. It holds no test code, and `pc_Tests.cpp` or `mc_Tests.cpp` defines it. `main()` calls it first, on the MC before the endless loop, and it returns the number of failed tests.
- **One result type per suite family.** Every PC suite returns the same `TestResults` (`Tests/pc/TestResults.hpp`) and counts with `check(name, ok)`. A new suite is one function in one `test_*.cpp` file plus one row in the runner's table.
- **Every test is isolated** (decision 13.41). A test builds its own `Printer`, `Stream`s and `CaptureStream`s as locals, and may break them freely; nothing it does may reach the next test. Tests don't change the global keyword streams or `prt1`. A binding test that must change one restores it with an RAII snapshot.
- Production code contains no test hooks. Tests observe behaviour through public interfaces, for example a `CaptureStream` installed as a stream's inner sink (spec §12).
- Do not silently change what a test checks or what output it expects. If a test is re-baselined, say why in the handoff entry.

## 5. Working rules

- **Only BY changes the Status of a request** (`Plans/*/… Request.md`).
- **Only BY commits.** Agents never run `git commit`, `git push` or anything else that rewrites history (`git commit --amend`, `rebase`, `reset`). Leave every change in the working tree, say which files changed, and suggest a commit message if useful. Staging is left to BY too. File moves use `git mv` so history follows the file, but the commit is BY's.
- **Decisions are BY's.** When a choice is open, propose a default, mark it provisional, and record it as an open item. Recorded decisions go into the specification's §13 table with the owner's wording and the date.
- **Plans own design; `_ProjectHandOff.md` owns live status.** New mailbox entries go at the top of its Part 2, with an ISO date.
- **Don't rewrite history.** Correct an old entry or a superseded decision with a dated note, so the earlier reasoning stays on record.
- **"Built" means a command actually ran.** Record the compiler, the command and the result. The host toolchain is CLion's bundled MinGW (g++ 15.2.0) and CMake (4.3.1), under `C:/Users/BY/AppData/Local/Programs/CLion/bin/mingw/bin` and `…/CLion/bin/cmake/win/x64/bin`; neither is on the shell PATH by default.
- **Build directories and IDE files are not source:** `cmake-build-*/`, `build/` and `.idea/` stay untracked.
