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
| **Common** | All of `Config/` (including `config_pc.hpp`, `config_mc.hpp`, `pc/` and `mc/`); `Printer/` and `Hardware/` files without a `pc_` / `mc_` prefix; `*_regs.hpp` | §2: MC and Common rules |
| **MC** | `mc_*` files, including `mc_chip.hpp` and `mc_<chip>.hpp` | §2: MC and Common rules |
| **PC** | `pc_*.cpp` files; `pc_*` headers included **only** by `pc_*.cpp` files | §3: PC rules |
| **PC tests** | `Tests/pc/` | §4: Tests |
| **MC tests** | `Tests/mc/` | §4: Tests |

**A header follows the strictest area of any file that includes it.** For example:
- Common code includes `config_platform.hpp`, which forwards to `config_pc.hpp` on the PC, so `config_pc.hpp` follows the Common rules.
- Shared driver code includes `pc_chip.hpp` and `pc_<chip>.hpp` on the PC, so those follow the Common rules too.

In practice, only `pc_*.cpp` files (and headers private to them) are really free.

## 2. MC and Common code rules

The microcontroller has **no operating system**, very little RAM, and a poor heap allocator with no defragmentation. Even with free heap, allocating anything larger than a byte can't be relied on. Common code runs on the MC too, so the same rules apply to it. (Spec §1.1, decision 13.35.)

### 2.1 Forbidden

1. **The C++ standard library.** No `std::` anything, and no C++ library header: not `<cstdint>`, `<cstring>`, `<array>`, `<new>`, `<type_traits>`, `<algorithm>`, `<iostream>`, `<string>`, `<vector>`, and so on.
2. **The heap.** No `new`, `delete`, `malloc`, `free`, or anything that calls them. All storage is static and sized at compile time.
3. **Exceptions and RTTI.** No `throw`, `try`, `dynamic_cast` or `typeid`.
4. **Globals with non-trivial destructors.** Such a global is registered through `__cxa_atexit`, and `atexit` may allocate. Check each global's class with `static_assert(__has_trivial_destructor(T))`. That built-in works in g++ 15.2; `__is_trivially_destructible` does not exist there.
5. **Function-local statics with dynamic initialization.** Their guards pull in `__cxa_guard_*`.
6. **Vendor and CMSIS headers.** The project is bare metal (decision 13.30). Register layouts are written from the reference manual (RM0008 for the STM32F103), and names follow it: `USART2`, `RCC`, `CR1`, `TXE`.
7. **`reinterpret_cast` in constant expressions.** Register addresses are `constexpr uintptr_t` integers, cast to register-block pointers at run time in the chip accessors.

### 2.2 Allowed

- The compiler's own freestanding C headers, which hold types and macros only and cost no RAM and no code: `<stdint.h>`, `<stddef.h>`, `<stdarg.h>`, `<limits.h>`, `<float.h>`. Types are written unqualified: `uint8_t`, `uint32_t`, `size_t`.
- Compiler built-ins, such as `__has_trivial_destructor` and `__builtin_memcpy`, where a standard header would otherwise be needed.
- `constexpr`, templates, `enum class`, `static_assert` and plain classes with virtual functions (no virtual destructors).

### 2.3 Required style

- **Configuration is data:** `inline constexpr` variables in `namespace cfg`, with no assignments at namespace scope.
- **Streams and `nullSink` are constant-initialized** (`constexpr` constructors), so they are safe to use before boot. `prt1` is the one dynamically initialized global, because its constructor boots the hardware.
- **Headers are `.hpp`**, starting with:
  ```cpp
  #pragma once
  #ifndef __cplusplus
  #error "<name>.hpp is a C++ header"
  #endif
  ```
- **Naming** (spec §2): types `PascalCase`; instances, functions and `constexpr` values `camelCase`; macros `SCREAMING_SNAKE_CASE`, only when really needed; physical quantities may end in a unit suffix (`cpuFrequency_MHz`).

### 2.4 Enforcement

- Common and MC sources compile with **`-nostdinc++`**: any C++ library header becomes a build error, while the C headers above still work. MC builds add `-fno-exceptions -fno-rtti -fno-threadsafe-statics`.
- The MC link provides **no `_sbrk`**, so heap use is a link error, not a runtime failure.
- `-nostdinc++` doesn't catch `malloc` through a C header on the PC build. Until the MC build exists, code review covers that: no `new`, `malloc` or `std::` outside PC files.

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
| **PC** | `Tests/pc/` | May use everything the PC offers: the standard library, any test framework, files, processes, the Windows or Linux API. It also tests the Common code, compiled for the host. A PC test that proves a Common header is clean (for example, that `config.hpp` compiles alone) builds that file with `-nostdinc++` |
| **MC** | `Tests/mc/` | Does its own job, under the MC and Common rules (§2). It is built and run by its own means, independently of the PC suite |

- Production code contains no test hooks. Tests observe behaviour through public interfaces, for example a `CaptureStream` installed as a stream's inner sink (spec §12).
- Do not silently change what a test checks or what output it expects. If a test is re-baselined, say why in the handoff entry.

## 5. Working rules

- **Only BY changes the Status of a request** (`Plans/*/… Request.md`).
- **Decisions are BY's.** When a choice is open, propose a default, mark it provisional, and record it as an open item. Recorded decisions go into the specification's §13 table with the owner's wording and the date.
- **Plans own design; `_ProjectHandOff.md` owns live status.** New mailbox entries go at the top of its Part 2, with an ISO date.
- **Don't rewrite history.** Correct an old entry or a superseded decision with a dated note, so the earlier reasoning stays on record.
- **"Built" means a command actually ran.** Record the compiler, the command and the result. The host toolchain is CLion's bundled MinGW (g++ 15.2.0) and CMake (4.3.1), under `C:/Users/BY/AppData/Local/Programs/CLion/bin/mingw/bin` and `…/CLion/bin/cmake/win/x64/bin`; neither is on the shell PATH by default.
- **Build directories and IDE files are not source:** `cmake-build-*/`, `build/` and `.idea/` stay untracked.
