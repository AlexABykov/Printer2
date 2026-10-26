# Printer2 — Project handoff

> **Document role.** This is the active mailbox and the single place session records are kept.
> Part 1 holds orientation and the current design conversation; Part 2 holds dated session entries,
> newest first.
>
> **Split rule.** When the history here grows too large, Part 1 moves to a separate
> `_DeveloperHandoff.md` as the long-form record, and this file keeps the role of `ProjectHandOff.md`
> in the valve_6 project: the mailbox new entries are written into. Until that split happens, Part 1
> lives in this file.
>
> **Writing rule.** Plans own design; this file owns live status. A plan should state build status
> once, in a block at the top, not restate it per section. New session entries go at the top of Part 2
> with an ISO date. Do not rewrite old entries: correct them with a dated note, so the record of what
> was believed then stays intact.

---

# Part 1 — Orientation

## 1. What Printer2 is

From `README.md`: an **MC-oriented printer with streams**, intended to work in a microcontroller
environment and on the PC, similar to `std::cout`.

The intended feature set, as written in the README (design goals, **not** current functionality):

- type-safe I/O subsystem;
- dual USART support — USART2 (production), USART3 (debug/calibration), with hardware flow control;
- optional DMA on both USART2 and USART3 Tx channels;
- double-buffered output for non-blocking serial transmission;
- stream-style API with `operator<<`;
- decimal and hexadecimal output modes for all types;
- support for `int8/16/32/64`, `uint8/16/32/64`, `float`, `double`, `char`, strings;
- printf-style format specifiers (`%d`, `%u`, `%f`, `%s`, `%c`);
- cross-platform abstraction over USART and PC console/file output.

The README's discussion seed for the API is:

```cpp
cons1 << "Hello, world!" << 123 << 3.14 << '\n';
```

The name `cons1` is a proposal, not a decision. See open question **P-1**.

## 2. Current repository state (2026-10-20)

| Item | State |
|---|---|
| Source | `main.cpp` is a one-line include of `config.h`; `config.h` contains only a `main()` stub |
| Printer code | `Printer/` exists but is **empty**; no stream, sink, USART, DMA or formatting code exists |
| Build | `CMakeLists.txt` added this session: C++17, target `printer_2`, source `main.cpp`, repo root on the include path |
| Build verification | **Not yet built after the include fix.** An earlier `g++ main.cpp -o main` failed at link with `undefined reference to WinMain` because `main.cpp` was empty; that was fixed by including `config.h`. Rebuild is pending |
| Tests | None. No test framework, no test target, no golden-output harness |
| Documentation | `README.md` is the only design document before this file |
| Git | Project has early commits for README and the skeleton; `config.h` and `main.cpp` were modified this session; `CMakeLists.txt` is new and untracked |
| Local-only directories | `.idea/` and `cmake-build-debug/` are IDE/build artefacts and should stay out of source control |

**The honest summary:** this is a skeleton with an intent, not an implementation. Nothing in the
README's feature list is built or tested yet.

## 3. Reading order

| # | Document | What it gives you |
|---|---|---|
| 1 | **`_ProjectHandOff.md`** (this file) | orientation, live status, open decisions, and the session mailbox |
| 2 | **`README.md`** | the project's own statement of purpose and the intended feature set |
| 3 | **`CMakeLists.txt`** | the current build contract: one executable, C++17, host build |
| 4 | **`main.cpp`** / **`config.h`** | the current placeholder — not a design statement |
| 5 | `Printer/` | the intended home of the library; currently empty |

If more documents appear later, list them here rather than scattering pointers through the entries.

## 4. Active workstream — v0 stream API

**Goal:** the smallest end-to-end slice that proves the core idea from the README. It should compile
on the PC, print to the console, and be structurally ready for a USART sink later.

Proposed v0 scope (not owner-approved yet):

1. A stream object that accepts chained `operator<<` calls.
2. Types: `const char*`, `std::string` (host only at first), `char`, signed and unsigned integers,
   `float`/`double` with a fixed default precision.
3. A byte-sink interface with one PC implementation writing to `stdout`.
4. Newline / flush behaviour: at minimum, `'\n'` must not require an explicit flush call for the PC
   sink; whether the stream auto-flushes on every insertion is open (**P-3**).
5. A demo in `main.cpp` that exercises the README example.

Explicitly **out of scope for v0:** USART registers, DMA, double buffering, hex mode, printf-style
format specifiers, MC code size tuning, ISR safety. Those follow once the API shape is stable.

**Definition of done for v0:**

- [ ] `cmake -S . -B build` and `cmake --build build` succeed on the host.
- [ ] The executable prints the README example correctly.
- [ ] Integer edge cases are correct: `INT8_MIN`, `UINT8_MAX`, `INT64_MIN`, `UINT64_MAX`, zero.
- [ ] A `float` and a `double` print deterministically, with no locale surprises.
- [ ] The sink is an interface; the console implementation is replaceable without touching the stream.
- [ ] The library target builds with no dependency on STM32 headers.

## 5. Proposed structure (to be confirmed)

```text
Printer2/
├── CMakeLists.txt
├── README.md
├── _ProjectHandOff.md
├── main.cpp                 # demo only
├── config.h                 # build-time configuration, NOT main()
└── Printer/
    ├── Printer.h            # public stream API
    ├── ByteSink.h           # transport abstraction
    ├── PcConsoleSink.*      # host implementation
    └── format/              # numeric conversion helpers
```

`Printer/` is the intended library boundary. `main.cpp` should remain a demo, not become the only
consumer. The build should eventually expose a library target plus the demo executable, so the same
library can be linked into an MC project without `main.cpp`.

## 6. Open decisions

These are the questions that block or shape v0. None has been decided by the owner yet; the
"provisional default" column exists so implementation can start, not to pretend a decision was made.

| ID | Question | Provisional default | Owner decision |
|---|---|---|---|
| P-1 | Public names: `cons1`, `Printer`, `Stream`, namespace? | namespace `printer`; type `Printer`; demo object named `cons1` | **open** |
| P-2 | Is the API a class with `operator<<`, a free function, or both? | class with `operator<<`, plus a `print`/`flush` method | **open** |
| P-3 | Flush policy: every insertion, on `'\n'`, explicit, or buffered? | PC sink writes directly; explicit `flush()` added but not required for `'\n'` | **open** |
| P-4 | Formatting: default decimal only, or a mode flag (`dec`/`hex`)? | decimal only in v0; mode flag follows | **open** |
| P-5 | Float formatting: fixed precision, `%g`-like, or caller-specified? | fixed 6 significant digits, no locale, no `std::to_chars` dependency yet | **open** |
| P-6 | Does v0 use `std::string` and the STL, or only C strings and fixed buffers? | STL on PC; add a no-STL/fixed-buffer path for MC later | **open** |
| P-7 | Error model: exceptions, error flag, `bool` conversion, or silent? | no exceptions; sink reports failure through a status flag | **open** |
| P-8 | Transport interface granularity: byte-at-a-time vs block write? | `write(const char*, size_t)` block write; PC implements it directly | **open** |
| P-9 | Buffer ownership: stream allocates, sink allocates, or caller supplies? | caller/sink owns; stream does not allocate in v0 | **open** |
| P-10 | Tests and CI: assert-based smoke test, Catch2, GoogleTest, or none? | a small assert-based host test target, no external dependency | **open** |
| P-11 | C++ standard: keep 17, or raise to 20? | C++17, matching the current CMake file | **open** |
| P-12 | `config.h`: keep as compile-time config, or remove it? | move `main()` to `main.cpp`; keep `config.h` for build switches only | **open** |
| P-13 | MC constraints: no heap, no RTTI, no exceptions, ISR-safe? | assume no heap/RTTI/exceptions on MC; PC build may use them only in host-only code | **open** |
| P-14 | USART2/USART3 semantics: two independent stream objects, or one with a selector? | two sink instances; stream does not know the port | **open** |

## 7. Conventions and house rules

These are proposed project rules; replace them when the owner states different ones.

1. **PC first.** Build and test the host path before adding STM32 code. A USART sink is not the place
   to discover an API mistake.
2. **One source of truth per concern.** The stream owns formatting; the sink owns transport. Do not
   put USART knowledge into `Printer`.
3. **No hidden allocation on the MC path.** Host-only conveniences must be separated by a clear
   boundary, not scattered through the library.
4. **Build directories and IDE files are not source.** `cmake-build-debug/` and `.idea/` stay
   untracked.
5. **Record who ran the build and what the result was.** In this file, "built" means an actual
   command was run, with the compiler and result named.
6. **Do not silently change the meaning of a test or a golden output.** If a test is re-baselined,
   say why in the entry.
7. **Correct old entries with a dated note, not by rewriting them.** The record of earlier reasoning
   is useful even when it turns out to be wrong.

## 8. Glossary

| Term | Meaning in this project |
|---|---|
| stream | the user-facing object that receives `<<` insertions |
| sink | the transport that accepts bytes; console, file, or USART |
| flush | force buffered bytes to the sink/transport |
| bank switching | alternating between two output buffers so one can drain while the other fills |
| DMA | direct memory access, used to move a Tx buffer without CPU copying |
| WUD/SSH | not used in Printer2; those belong to the valve_6 friction project |

---

# Part 2 — Session mailbox

New entries go directly below this line, newest first.

## Mailbox entry — 2026-10-26 (session 11) — **Second Phase 2 review resolved: "approvable, no re-review needed". One new scratch fact: `-Wpedantic -Werror` already rejects any `#include_next` at compile time. Plan awaits BY's approval.**

**State.** Documentation only; the source is still unchanged. BY replaced the first review in `Phase 2 Found Issues.md` with the second one; the first review stays in `31013f4` and its resolution in `17d8116`. The changes below are in the working tree, uncommitted (BY commits).

### What changed

- **`Plans/Config/Phase 2 plan.md`:**
  - **§8** is split: §8.1 (first review) and a new §8.2 (second review).
  - **Build-status note:** a new row for the `#include_next <stdint.h>` check.
  - **§3.1 notes:** the `#include_next` scope (what the check adds over `-Wpedantic`) and the tightened "any order" wording.
  - **§3.10 check 3:** a non-redirected run after `chcp 437`.
- **`Plans/Config/Config Base Plan.md`:** in the Phase 2 note, "accepts `#include_next` lines" became "reads".
- **`Plans/Config/Phase 2 Found Issues.md`:** a "Resolution (2026‑10‑26)" section at the end; the review text is unchanged.

### Built

In the scratch copy (`%TEMP%/p2_phase2`), with MinGW g++ 15.2.0 and CMake 4.3.1:
- With `#include_next <stdint.h>` added to `config.hpp`, the include check passed, and the compile failed with "'#include_next' is a GCC extension [-Werror=pedantic]".
- After reverting, the build was green again.

### Decisions

None new.

### Open

1. **BY's approval of `Phase 2 plan.md`**, and answers to §5 Q-1 … Q-7.
2. **Still open:** as listed in session 9.

### Notes for the incoming developer

- **`AGENTS.md` doesn't mention `main.cpp` on purpose.** It changes only if BY confirms Q-3 (plan §3.11).

## Mailbox entry — 2026-10-26 (session 10) — **The review `Plans/Config/Phase 2 Found Issues.md` is resolved: 6 findings fixed, 2 needed no change. The include check now also catches `#include_next`. Rebuilt in scratch, still green. The Phase 2 plan awaits BY's approval.**

**State.** Documentation only in the repository; the source is still unchanged. The changes below are in the working tree, uncommitted (BY commits).

### What changed

- **`Plans/Config/Phase 2 plan.md`:**
  - **§3.1:** the include-check regex accepts `#include_next` (P2-03), and `enable_testing()` has a comment (P2-06). New notes say that a missing listed file fails configure (P2-02) and that `LINKER_LANGUAGE CXX` goes away in Phase 4 (P2-04).
  - **§3.6:** the code-page restore is guarded against 0 (P2-01).
  - **§3.10:** two manual checks added (redirected runs under two code pages, a missing checked file).
  - **§3.11:** the `AGENTS.md` `main.cpp` row, depending on Q-3 (P2-08).
  - **New §8:** the resolution table, plus a build-status addendum.
- **`Plans/Config/Config Base Plan.md`:** a dated note under Phase 2 that points to the detailed plan's departures (P2-07).
- **`Plans/Config/Phase 2 Found Issues.md`:** a "Resolution (2026‑10‑26)" section at the end; the review text is unchanged.

### Built

The same scratch copy (`%TEMP%/p2_phase2`), toolchain and commands as session 9, after the fixes:
- a fresh `cmake --preset pc` and build succeeded;
- `printer_2.exe` passed 1 of 1 and exited 0, and so did the redirected run;
- `ctest` passed.

`#include_next <array>`, `#include <array>` and `# include<array>` in `config.hpp` are each rejected. With `Tests/Tests.hpp` removed, configure fails with "cannot be read".

### Decisions

None new. P2-02 was rejected on evidence: its premise, that a missing file passes silently, was disproved in scratch. P2-05 was declined as style. The plan's §5 questions are still open.

### Open

1. **BY's approval of `Phase 2 plan.md`**, and answers to §5 Q-1 … Q-7. Q-3 also decides the `AGENTS.md` edit.
2. **Still open:** as listed in session 9.

### Notes for the incoming developer

- **Correction (2026‑10‑26), session 9's heredoc note.** The regex's replacement is now `"\\2"`, not `"\\1"`, because of the added `(_next)?` group. The trap is the same: the shell heredoc collapsed `"\\2"` in this session's plan text too, and it was fixed with the Edit tool. Check both `\\` spots after writing `CMakeLists.txt`.

## Mailbox entry — 2026-10-26 (session 9) — **`Plans/Config/Phase 2 plan.md` written: presets, prefix-based source selection, the include allow-list, and the PC test suite with `ConfigTests`. Prototyped in a scratch copy, green; the repository's source is unchanged. Awaiting BY's approval.**

**State.** Documentation only in the repository: the new plan and this entry, uncommitted (BY commits). No source file or `CMakeLists.txt` was changed. `.gitignore` and `Tests/pc/` are still untracked (session 8, open item 1).

### What changed

- **`Plans/Config/Phase 2 plan.md`, new:** the full content of `CMakeLists.txt`, `CMakePresets.json`, `cmake/arm-none-eabi.cmake`, `Tests/Tests.hpp`, `Tests/pc/TestResults.hpp`, `Tests/pc/pc_Tests.cpp`, the `runConfigTests()` addition to `test_config.cpp`, and the new `main.cpp`. Also done criteria, manual checks, the differences from the base plan (§4), seven questions (§5) and risks.

### Built

In a scratch copy (`%TEMP%/p2_phase2`), not in the repository. MinGW g++ 15.2.0 and CMake 4.3.1, in Git Bash. The plan's build-status table has the full list: `cmake --preset pc` and `cmake --build --preset pc` succeeded under `-Werror`; `printer_2.exe` ran `ConfigTests` (1 of 1 passed) and exited 0; `ctest` passed. A deliberately failing check gave exit 1. `<array>` in `config.hpp` or `Tests/Tests.hpp` failed the next plain build. `cmake --preset mc` stopped with "arm-none-eabi-g++ was not found". No ARM toolchain is installed.

### Decisions

None new. The plan's §5 questions Q-1 … Q-7 carry provisional answers, and the main ones are:
- sources are collected by glob;
- `-Werror` is set per target;
- `main.cpp` counts as Common;
- the generator is MinGW Makefiles;
- the runner switches the console to UTF-8 and restores the old code page;
- with an ARM toolchain, the MC configuration defines `printer` only.

### Open

1. **BY's approval of `Phase 2 plan.md`**, and answers to its §5 questions.
2. **`Config Base Plan.md` approval** (session 7, open item 1). The Phase 2 plan moves the `config_mc_check` target to Phase 6 (plan §4).
3. **Still open from earlier sessions:** as listed in sessions 6 and 8.

### Notes for the incoming developer

- **The heredoc trap.** Writing `"\\1"` through a Bash heredoc in this environment produced `"\1"`, and CMake then failed with "Invalid character escape '\1'". When implementing §3.1, write the file with an editor tool, then check line 24 for `"\\1"` and the test filter for `\\.cpp`.

## Mailbox entry — 2026-10-26 (session 8) — **Implementation review 000 of `config.hpp` resolved. BY's edits (`baudRate`, `auto`, indentation) stay, and the spec and plans now follow the code. The build is still green.**

**State.** Documentation only; no source file changed in this session. BY's own edits to `Printer/Config/config.hpp` were rebuilt and pass. BY committed session 7 and the review (`b9061d6`, `bf12d3a`), but **`.gitignore` and `Tests/pc/` are still untracked**, so those commits don't include them. The changes below are in the working tree, uncommitted (BY commits).

### What changed

- **`Plans/Config/Config Specification.md`:**
  - **§9:** `baudRate` in the `UartSettings` bullet, the `isValid` text and the sketch; `defaultFileName` is `auto` in the sketch; a dated revision note under the sketch.
  - **§13:** new decisions 13.61 and 13.62.
- **`Plans/Config/Phase 1 plan.md`:** `baudRate` and `auto` in the §3.1 code, plus a dated note "Changed after the build".
- **`Plans/Config/Config Base Plan.md`:** `baudRate` in Phase 1 item 3.
- **`Plans/Config/Config hpp implementation review 000.md`:** a "Resolution (2026‑10‑26)" section at the end. The review text is unchanged.

### Built

The same toolchain and commands as session 7 (MinGW g++ 15.2.0, CMake 4.3.1), on BY's edited `config.hpp`:
- `cmake --build build` built `printer_2`, and `build/printer_2.exe` exited 0.
- Both `g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I. -c` check compiles exited 0 with no warnings.
- `grep '#include <'` found `<cstdint>` only.

No test changed. The checks use positional initialisation, so the rename doesn't reach them.

### Decisions

Recorded in spec §13 with BY's wording.

| ID | Decision | Date |
|---|---|---|
| 13.61 | `UartSettings::baudRate` (camelCase, two words), not `baudrate`: "Keep baudRate" | 2026‑10‑26 |
| 13.62 | `defaultFileName` stays `inline constexpr auto`: "Keep auto" | 2026‑10‑26 |

Also BY's choice, not a §13 decision: **no style rule for namespace indentation** (review D-3).

### Open

1. **Untracked files:** `.gitignore` and `Tests/pc/` (both test files) are BY's to add.
2. **Still open from session 7:** `Config Base Plan.md` approval before Phase 2.
3. **Still open from earlier sessions:** as listed in session 6.

### Notes for the incoming developer

- **The review is partly out of date.** It lists F-10 and F-01 … F-06 as open, but they were resolved in session 6 (§13.59, and `Found Issues.md`'s Resolution table). Read its Resolution section first.

## Mailbox entry — 2026-10-26 (session 7) — **First code: the rest of Phase 0 and all of Phase 1 are done. `Printer/Config/config.hpp` exists, and its compile-time checks pass. The demo builds and exits 0. Next is Phase 2.**

**State.** Built and green, uncommitted (BY commits). Nothing is staged except the rename that `git mv` stages itself (`config.h` → `Printer/Config/config.hpp`). `CMakeLists.txt`, `README.md`, `AGENTS.md` and the spec are unchanged.

### What changed

- **`config.h` → `Printer/Config/config.hpp`** (`git mv`). The header now holds the Phase 1 plan's §3.1 content, unchanged from the plan.
- **`main.cpp`:** `#include "config.h"` → `#include "Printer/Config/config.hpp"`.
- **`.gitignore`, new:** `.idea/`, `cmake-build-*/`, `build/`, `*.exe`. The untracked root `main.exe` no longer shows up.
- **`Tests/pc/test_config.cpp`, new:** the plan's §3.2 compile-time checks.
- **`Tests/pc/test_config_address_probe.cpp`, new:** the address probe. It is only compiled for now; the comparison runs in Phase 2's `ConfigTests`.
- **`Plans/Config/Phase 1 plan.md`:** the done criteria are ticked, and a dated "Implemented" note is added to the build-status block.

### Built

Toolchain: CLion's bundled MinGW `g++.exe (GCC) 15.2.0` and CMake 4.3.1, put on the PATH for the shell (Git Bash). Commands from the repository root, as in plan §3.3:

| Command | Result |
|---|---|
| `cmake -S . -B build -G "MinGW Makefiles"` | configured |
| `cmake --build build` | `Built target printer_2` |
| `build/printer_2.exe` | exit 0 |
| `g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I. -c Tests/pc/test_config.cpp -o build/test_config.o` | exit 0, no warnings: every `static_assert` holds |
| `g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I. -c Tests/pc/test_config_address_probe.cpp -o build/test_config_address_probe.o` | exit 0, no warnings |
| `grep -n '#include <' Printer/Config/config.hpp` | `5:#include <cstdint>` only |

> **Correction (2026‑10‑26), session 2 entry (2026‑10‑24) and Part 1 §2.** Those say the root `config.h` still held the `main()` stub. By this session it was already empty, and `main()` already lived in `main.cpp` (review F-12). So Phase 0 step 1 had nothing left to do. The old text is left as written.

### Decisions

None new.

### Open

1. **Plan approval.** The Phase 1 plan's header says "approved by BY", which session 6's open item 1 still listed as pending. `Config Base Plan.md` still awaits approval before Phase 2.
2. **Commit split (plan Q-2), BY's choice.** Suggested messages:
   - "Phase 0: move config header to Printer/Config/config.hpp, add .gitignore" for the rename, `main.cpp` and `.gitignore`;
   - "Phase 1: common config header Printer/Config/config.hpp with compile-time checks" for the header content, `Tests/pc/` and the plan and handoff updates.
3. **Still open from earlier sessions:** as listed in session 6.

### Notes for the incoming developer

- **The two test files have no CMake target until Phase 2.** After any edit to `config.hpp`, rerun the two `g++` commands above by hand (plan §6).
- **`build/` is the hand-build directory** and is ignored. CLion still uses `cmake-build-debug/`.

## Mailbox entry — 2026-10-26 (session 6) — **The AI review in `Plans/Config/Found Issues.md` is resolved: 14 findings fixed in the plan or the spec, 1 documented as a limit, 1 needed no change, 1 rejected. PC errors now go to the console in red. Documentation only; the base plan is ready for BY's approval again.**

**State.** Documentation only. No source file was created or changed and nothing was built. Session 5 and the review file are committed by BY (`e65d732`, `56df4d0`). The changes below are in the working tree, uncommitted (BY commits). `README.md` and `AGENTS.md` are unchanged.

### What changed

- **`Plans/Config/Found Issues.md`:** a new "Resolution (2026‑10‑26)" section at the end, with one row per finding (outcome and where it was fixed), the mapping of the risky paths R-01 … R-12 to tests, and one issue found while resolving. The review text itself is unchanged.
- **`Plans/Config/Config Base Plan.md`:**
  - **Phase 3:** the `PcSink` validity `static_assert` is removed (F-17).
  - **Phase 4:**
    - `BaseStream::autoFlush()` (F-02);
    - `Stream::bind()` / `inner()` / `setAutoFlush()` (F-11);
    - `FileStream` index constructor and the explicitly initialized `fileStreams[9]` (F-05);
    - the default file name (F-09) and `OpenMode` (F-14);
    - `const` forwarding methods on `File<N>` (F-03), and `Keywords.hpp` including `FileStream.hpp`;
    - `EndLine` (F-08);
    - `CaptureStream` defaulting to `allowsAnsi = false` (F-15).
  - **Phase 5:**
    - one private `select()` helper for all target switches (F-01, F-06);
    - null guards for `endLine` and for a null `const char*` (F-07, F-16);
    - auto-flush through `pStream->autoFlush()`.
  - **Tests:** B1, B2, B4 and P4 updated; **B7, P5 and P6 are new.**
  - **Header and Phase 0:** dated corrections. `config.h` is already empty and `main()` already lives in `main.cpp` (F-12).
- **`Plans/Config/Config Specification.md`:**
  - **§3:** `autoFlush()` on `BaseStream`; `bind()` / `inner()` on `Stream`.
  - **§4, §5:** every operator is null-safe; selecting the current target again is not a switch.
  - **§8:** the file keyword methods are `const`.
  - **§9:**
    - the PC example's `errorTarget` is `console0`, with a dated note;
    - `isValid()`'s enum limit is documented.
  - **§13:** new decisions 13.58, 13.59 and 13.60.
- **`Plans/Config/Phase 1 plan.md`:** one note under §3.1, "what `isValid` does not check" (F-10). The header in §3.1 is unchanged, so its scratch build still stands.

**Tests re-baselined (plan only, nothing is built):**
- **B2:** `errorStream.inner()` is now `&console0`, not `&fileStreams[1]`, because the PC example's error target changed (13.60).
- **B1, B2, B4:** they read and write the binding through `inner()` / `bind()` instead of the member `pInner` (F-11), and P4 sets auto-flush with `setAutoFlush(true)`. What these tests check is unchanged.

### Decisions

Recorded in spec §13 with BY's wording.

| ID | Decision | Date |
|---|---|---|
| 13.58 | `autoFlush()` is a virtual query on `BaseStream` (default `false`), overridden by `Stream` (F-02) | 2026‑10‑26 |
| 13.59 | `isValid()` checks no enum ranges; the limit is documented (F-10) | 2026‑10‑26 |
| 13.60 | Errors go to the PC console in red and to USART3 on the MC: "On PC to console, use red color. On MC to USART3". The demo prints them with `prt2 << hold << error << … << endLine` (F-04) | 2026‑10‑26 |

### Open

1. **BY's approval** of `Config Base Plan.md` and `Phase 1 plan.md`. The review recommended approving Phase 1 after minor cleanup, which is done (F-10, F-12). It recommended holding Phases 4–5 until F-01 … F-06 were fixed; they now are.
2. **Still open from earlier sessions:**
   - the known limit of `none` (a silenced call still formats);
   - spec 13.19 (misuse assert), 13.50, 13.51, 13.52, 13.53, 13.56;
   - spec 13.1, 13.2, 13.11, 13.12, 13.14, 13.16, 13.17, 13.20, the rest of 13.22, 13.25, 13.31, 13.34, 13.38 and 13.42.

### Notes for the incoming developer

- **Read `Found Issues.md` with its Resolution table.** The review describes the plan as it stood before session 6; the table says where each point now stands.
- **Do not copy the review's F-03 snippet literally.** It marks the `File<N>` methods `constexpr`. They must be `const`, but not `constexpr`, because they change `fileStreams[N]` at run time.
- **With the default configuration, all three PC roles share `console0`.** They are told apart by colour only: green `message`, red `error`, gray `debug`.

## Mailbox entry — 2026-10-26 (session 5) — **A third output role, `debug`, and a `none` target that switches any role off. Documentation only; next is Phase 0 / Phase 1.**

**State.** Documentation only. No source file was created or changed and nothing was built. Session 4 is committed by BY (`36fd760`, `94a1ce2`). The changes below are in the working tree, uncommitted (BY commits). `Plans/Config/Config Request.md`, `AGENTS.md` and `Plans/Config/Phase 1 plan.md` are unchanged: `config.hpp` holds no role constants, so Phase 1 is not affected.

### What changed

- **`Plans/Config/Config Specification.md`:**
  - **§2, §3:** `debug`, `Debug` and `debugStream` are added to the names, the instances and the keyword types.
  - **§4:**
    - a `debug` row in the keyword table;
    - a new bullet, "`debug`, the third output role": why it isn't `info` again, the `none` target, and the known limit;
    - a dated note superseding "a project that needs a separate debug channel adds a keyword back";
    - `Debug` in the overload list, and `debug` in the safe-names and base-keyword lists.
  - **§4.1, §6:** `prt2.debug(…)` in the flush table and the method example; `::debug` among the hidden names; `cfg::debugAutoFlush`.
  - **§9:**
    - `McSink` and `PcSink` gain `none`;
    - `debugTarget` is `usart3` on the MC and `console0` on the PC;
    - `debugColor = darkGray` (PC) and `debugAutoFlush = false` (both platforms);
    - `printerInit()` binds `debugStream` and maps `none` to `&nullSink`.
  - **§13:** new decision 13.57; a dated note on 13.44 ("extended"); `Debug` and `debugStream` added to open item 13.56.
- **`Plans/Config/Config Base Plan.md`:**
  - `debug` / `Debug` / `debugStream` in the layout and in Phases 3–6.
  - `none` in both enums, and `printerInit()` binds three streams.
  - B2 and the RAII snapshot cover `debugStream`.
  - **B6 (`none` → `&nullSink`) is new.** For it, the PC sink resolver becomes a plain function in a new PC-only header, `Hardware/pc_PrinterInit.hpp`.
- **`README.md`:** the status date, `debug` in the keyword list, and `prt2.debug(…)` in the formatted-output bullet.

**Tests re-baselined (plan only, nothing is built):** B2 also checks `debugStream`'s binding, colour and `autoFlush`, and the snapshot also saves `debugStream`, because there is a third global stream. B6 is new.

### Decisions

Recorded in spec §13 with BY's wording.

| ID | Decision | Date |
|---|---|---|
| 13.57 | A third output role, `debug`, with `prt2.debug(…)`. Defaults: USART3 on the MC (the debug / calibration link), `console0` in `darkGray` on the PC, no `autoFlush`. `McSink` / `PcSink` gain `none`, which binds a role to `nullSink`, so any role can be switched off from the configuration. "I have decided that a one more stream "debug" can be useful"; "Yes for all 3 questions" | 2026‑10‑26 |

### Open

1. **Known limit of `none`** (spec §4, recorded, not a question yet): a silenced call still evaluates and formats its arguments. Removing the calls entirely would need a macro at the call site. Revisit if the cost shows up on the MC.
2. **Spec 13.56, names:** now also `Debug` / `debugStream`. `Debug` is another common word that may clash with user types.
3. **Still open from sessions 3 and 4:** spec 13.19 (misuse assert), 13.50, 13.51, 13.52 and 13.53.
4. **Still open from before:** BY's approval of `Config Base Plan.md` and `Phase 1 plan.md`; spec 13.1, 13.2, 13.11, 13.12, 13.14, 13.16, 13.17, 13.20, the rest of 13.22, 13.25, 13.31, 13.34, 13.38 and 13.42.

### Notes for the incoming developer

- **`debug` is not the dropped `info`.** `info` was removed (13.44) because it went to the same sink as `message`. `debug` earns its place by going to a different link by default, and by being able to go nowhere (`none`).
- **On the MC, `error`, `debug` and the file backing all share USART3** in the example configuration. Interleaving between them is the same question as two open files on one USART (spec 13.16).

## Mailbox entry — 2026-10-25 (session 4) — **Keywords are types: any object of a keyword's type is that keyword. Base file keywords `file0` … `file8`, with aliases and no `file(i)`. The streams behind the keywords are separate objects. Documentation only; next is Phase 0 / Phase 1.**

**State.** Documentation only. No source file was created or changed and nothing was built. Session 3 is committed by BY as `c5c307c`; the changes below are in the working tree, uncommitted (BY commits). `Plans/Config/Config Request.md` and `AGENTS.md` are unchanged in this session.

### What changed

- **`Plans/Config/Config Specification.md`:**
  - **§3:** a new "Keyword types" layer in the object model. The streams are `messageStream`, `errorStream` and `fileStreams[0..8]`. `operator<<` takes a keyword, or a `BaseStream&` for an object the caller owns (tests, project-bound sinks).
  - **§4:**
    - The keyword table shows each keyword's type (`Message`, `Error`, `File0` … `File8`) and the stream it selects.
    - New bullets: "each base keyword has its own type, and the type is the keyword", and "base keywords and aliases", with `inline constexpr File6 recorder{};` as the example.
    - `File<N>` has `static_assert(N < 9)`.
    - The 2026‑10‑23 text "each target is a distinct global object … without a tag type" is replaced.
  - **§4.1:** `prt2.flush(…)` takes a keyword (one overload per type) or a `BaseStream&`.
  - **§8:** file names are set by `fileN.setName(…)`, not by a public `name` field, because a keyword is an empty tag and can't hold the name. `file0.name = …` is gone from the examples.
  - **§9:** the binding targets the internal streams (`messageStream.pInner`, `PcSink::file1` → `&fileStreams[1]`). The keywords need no binding.
  - **§13:**
    - new decisions 13.54 and 13.55;
    - a dated note on 13.19 (the form is now `setName`; enforcement stays open);
    - the former 13.55 open item (the alias form) points to the Decisions table;
    - a new open item, 13.56 (names).
- **`Plans/Config/Config Base Plan.md`:**
  - A new `Printer/Keywords.hpp` (Phase 4 item 6) holds the tag types, the base keywords, and `File<N>` forwarding `setName` / `open` / `close` to `fileStreams[N]`.
  - The `Stream` globals are `messageStream` / `errorStream`, and the PC `FileStream`s are the array `fileStreams[9]`, which hold the name pointer.
  - `Printer` gets one `operator<<` per keyword type, plus `operator<<(BaseStream&)`; every one returns `Printer&`.
  - B2, B4, the RAII snapshot and the "tests don't touch the globals" list use the stream names. **B5 (keywords and aliases) is new.**
  - §6 row §13.19: `setName`.
- **`README.md`:** one sentence on aliases (`constexpr File6 recorder{};`).

**Tests re-baselined (plan only, nothing is built):** B2 and B4 now name `messageStream` / `errorStream` / `fileStreams[1]` instead of `message` / `error` / `file1`, because the keywords are no longer the stream objects (§13.55). What they check is unchanged.

### Decisions

Recorded in spec §13 with BY's wording.

| ID | Decision | Date |
|---|---|---|
| 13.54 | Base keywords `file0` … `file8`; on the PC each is a file with its own unique name. No indexed `file(i)` keyword; `prt2.file(index, …)` stays the only indexed access. Projects name the base keywords with aliases | 2026‑10‑25 |
| 13.55 | Each base keyword has its own type, and any object of that type is that keyword: "`BaseKeyWordTypeFoo bar;` - now bar will work exactly like foo". Keywords are empty tags; `Printer` selects the stream by the type. This is the old project's model (`KeyWords::` structs, one `operator<<` per struct). Example: when the Recorder needs a file, it creates its own alias, `File6 recorder;`. Which file it picks doesn't matter: "recorder has the same type as file6. Or file2. Doesn't matter" | 2026‑10‑25 |

### Open

1. **Spec 13.56, names** (provisional):
   - **Keyword types:** `Message` / `Error` / `File<N>` (`File0` … `File8`). `Message` and `Error` are common words and may clash with user types. Alternatives are `MessageKeyword` / `ErrorKeyword` / `FileKeyword<N>`, or a namespace like the old `KeyWords::`.
   - **Streams:** `messageStream` / `errorStream` / `fileStreams[9]`.
2. **Spec 13.19:** the form is `setName`; whether misuse (renaming an open file) gets a debug assert is still open.
3. **Still open from session 3:** spec 13.50 (stuck-detection details), 13.51 (flush details), 13.52 (unsupported specifiers) and 13.53 (8 or 9 files).
4. **Still open from before:** BY's approval of `Config Base Plan.md` and `Phase 1 plan.md`; spec 13.1, 13.2, 13.11, 13.12, 13.14, 13.16, 13.17, 13.20, the rest of 13.22, 13.25, 13.31, 13.34, 13.38 and 13.42.

### Notes for the incoming developer

- **Keywords are types, not streams** (spec §4, §13.55). `message` is an empty `Message` tag; the stream it selects is `messageStream`. Code that reads or binds a stream uses the stream name; code that prints uses the keyword.
- **The old project is a good reference for the keyword mechanism:** `C:/Work/valve_6/Common/Printer/KeyWords.h` and `PrinterOperators.cpp`. One difference matters: its `operator<<` returns `Printer` **by value**, so after the first `<<` the rest of a chain works on a temporary copy, and a switch made later in the chain doesn't persist. The new `operator<<` returns `Printer&`.
- **Tests select their local streams with `operator<<(BaseStream&)`.** The keyword overloads always select the global streams, so a test that uses a keyword must snapshot and restore that stream (B5).

## Mailbox entry — 2026-10-25 (session 3) — **Milestone: the Printer's user API is settled: `prt2`, keywords `message` / `error` / `file0` … `file8`, formatted output on the printer (`prt2.message` / `error` / `file(index, …)`), the flush model, blocking writes with stuck detection, `hold`. Documentation only; next is Phase 0 / Phase 1.**

**State.** Documentation only. No source file was created or changed and nothing was built; the repository still has the root `config.h` and `main.cpp`, and `Printer/` is empty. All changes below are in the working tree, uncommitted (BY commits). `Plans/Config/Config Request.md` is unchanged.

### What changed

- **`Plans/Config/Config Specification.md`:**
  - **§3 and §6:** formatted output is now a method on the printer: `prt2.message(fmt, …)`, `prt2.error(fmt, …)` and `prt2.file(index, fmt, …)`. `stream.printf` and `BaseStream::printf` are removed. §6 is retitled "Formatted output".
  - **§4:** the keyword table has `message` (like `std::cout`), `error` (like `std::cerr`) and `file0` … `file8`; `info` is gone.
  - **§4.1, new:** "`endLine`, `flush` and `hold`": the flush table, flush per hardware stream, `autoFlush`, and `hold` with BY's use case.
  - **§5:** `endLine` order is `'\n'`, `resetColor()`, flush. A `'\n'` byte doesn't reset the colour. Added what colour is for on the PC.
  - **§7:** writes block, with no loss; stuck detection; no ISR code in the Printer.
  - **§9:** the role targets drop `info`, and `messageAutoFlush` / `errorAutoFlush` are added to both platform configs. `cpuFrequency_MHz` is now the stuck-detection budget.
  - **Everywhere:** `prt1` → `prt2`, except in quoted owner wording and history.
  - **§13:** new decisions 13.15, 13.18, "13.22 (part)" and 13.43–13.49. Dated notes were added on 13.4, 13.5, 13.8, 13.9, 13.10, 13.27, 13.28 and 13.40. New open items are 13.50–13.53. Open items 13.15 and 13.18 now point to the Decisions table, 13.22 is updated, and 13.34 now says "blocking wait and stuck drop".
- **`Plans/Config/Config Base Plan.md`:**
  - `prt2` throughout.
  - The `Stream` globals are `message` / `error`, with an `autoFlush` flag.
  - `endLine` flushes.
  - `CaptureStream` records flush marks.
  - Tests B2, B4, P1 and P2 are updated, and P4 (`autoFlush`) is new.
  - D-5 has a dated revision note.
  - §6 and §8 are updated.
- **`README.md`:** the status date, `prt2`, the keywords, the error example (the old claim that `error.printf(...)` printed "the same line" was wrong: it printed no newline), and new bullets for `hold`, flushing, formatted output and blocking output.
- **`AGENTS.md`:** `prt1` → `prt2` (§2.3, §4).

**Tests re-baselined (plan only, nothing is built):** P1 and P2 now expect a flush mark after `endLine`, because §13.40 was revised so that `endLine` flushes. B2 and B4 use `message` instead of `info`, because `info` no longer exists.

### Decisions

Recorded in spec §13 with BY's wording.

| ID | Decision | Date |
|---|---|---|
| 13.43 | The printer instance is `prt2`, not `prt1`: "they are not fully compatible" | 2026‑10‑25 |
| 13.44 | Keywords `message` (≈ `std::cout`) and `error` (≈ `std::cerr`) plus `file0` … `file8`; `info` dropped as redundant; `error` is a role, bindable to any sink | 2026‑10‑24 / 25 |
| 13.45 | `prt2.message(…)`, `prt2.error(…)`, `prt2.file(index, …)`; no "printf" in the name ("not honest"); no automatic newline; the target is restored after the call; a bad file index prints a line to `error` | 2026‑10‑24 / 25 |
| 13.46 | `<< flush`, `<< endLine`, `prt2.flush(stream)` flush one hardware stream; `prt2.flush()` flushes all; flush blocks; `'\n'` is just a byte. This settles P-3 | 2026‑10‑25 |
| 13.47 | Blocking writes with no loss; a stuck stream drops bytes, is re-checked on every write, and returns at once while stuck | 2026‑10‑25 |
| 13.48 | No ISR code in the Printer: interrupts are enabled for the DMA only | 2026‑10‑25 |
| 13.49 | `error` flushes itself like `std::cerr`; `autoFlush` is a per-role config constant | 2026‑10‑25 |
| 13.18 | `hold` makes the next target switch temporary | 2026‑10‑25 |
| 13.15 | No direct stream printing; formatted output goes through `prt2` (follows from 13.45) | 2026‑10‑25 |
| 13.22 (part) | Flush on the PC: `std::cout.flush()` / `ofstream::flush()`; on the MC every stream, files included, is a USART link | 2026‑10‑25 |

Revised by these decisions: 13.4 (superseded), 13.5, 13.8, 13.9 (superseded), 13.10, 13.28, 13.40, and §7's "backpressure by loss" rule.

### Open

1. **Spec 13.50, stuck-detection details** (provisional): progress = `CNDTR` moves or `TXE` sets; budget = a few byte-times from `cfg::cpuFrequency_MHz`. BY may describe the old Printer's mechanism so it can be copied.
2. **Spec 13.51, flush details** (provisional):
   - `prt2.flush()` visits each hardware sink once.
   - `prt2.flush(stream)` takes a `BaseStream&`.
   - `autoFlush` flushes after each `<<` item, not only at `'\n'`.
3. **Spec 13.52:** unsupported format specifiers are printed literally (provisional).
4. **Spec 13.53:** 8 or 9 files? BY's wording mentioned "a bank of 8 files"; the spec keeps `file0` … `file8` until confirmed.
5. **Still open from before:** BY's approval of `Config Base Plan.md` and `Phase 1 plan.md`; spec 13.1, 13.2, 13.11, 13.12, 13.14, 13.16, 13.17, 13.19, 13.20, the rest of 13.22, 13.25, 13.31, 13.34, 13.38 and 13.42.

### Notes for the incoming developer

- **Read spec §4.1 before writing any `Printer` code.** It is the single place where `endLine`, `flush`, `autoFlush` and `hold` meet.
- **Inside `Printer`, the method names hide the keywords:** a member function must write `::message`, `::error` and `::flush` to reach the global streams and keyword (spec §6).
- **Blocking writes need a PC register model that can stall.** Without one, the stuck path can't be tested (base plan §8).
- **Part 1 of this file is out of date** on names and flushing: `cons1`, P-1 and P-3. The spec's §13 is authoritative.

## Mailbox entry — 2026-10-24 (session 2) — **Config Specification reviewed and revised; `Config Base Plan.md` written and awaiting BY's approval. Still no code. Next is plan approval, then Phase 0.**

> **Correction (2026-10-26).** The root `config.h` was already empty at this point, and `main()` was
> already in `main.cpp` (review F-12). See the session 7 entry. The text below is left as written.

**State.** Documentation only. No source file was created or changed and nothing was built. `Printer/` is still empty, and the root `config.h` still holds the `main()` stub. The documents are committed by BY up to `ee8bc0e`; only this entry is not yet committed. `Plans/Config/Config Request.md` is unchanged (Status: Discussion; only BY can change it). Toolchain note: in the shell used this session, `g++` and `cmake` are not on the PATH, and no `arm-none-eabi` toolchain was found. Builds are expected through CLion's bundled MinGW 15.2.

### What changed

- **`Plans/Config/Config Specification.md`:**
  - Reviewed against the Request, the README and this file, then revised in several rounds (2026‑10‑23 / 24).
  - Retitled "Printer Configuration Specification". §9 is the configuration proper; §§3–8 and §§10–12 are Printer-wide context.
  - Technical fixes:
    - BRR uses the APB clock, not the CPU clock.
    - No `constexpr` register pointers; addresses are integers, cast at runtime.
    - Windows VT mode is enabled for console colour.
    - Banks are sized per USART instance.
    - The colour path compiles out with `if constexpr`.
    - A null sink makes printing before boot safe.
  - Added header sketches for `config.hpp`, `config_mc.hpp` and `config_pc.hpp`.
  - Added §10 Hardware: chip files, the MC boot sequence, the clock check and the Printer's assert.
  - Added §11 CMake presets.
  - §13 split into a Decisions table (owner wording, dated) and stable-numbered open items.
- **`Plans/Config/Config Base Plan.md`:** new. It is the first implementation slice, Phases 0–6:
  - Phase 0: repo prep (`main()` out of `config.h`, `Config/`, `.gitignore`).
  - Phase 1: `config.hpp`.
  - Phase 2: CMake, `config_platform.hpp` selection, `pc` / `mc` presets.
  - Phase 3: `config_pc.hpp`.
  - Phase 4: minimal stream core (`BaseStream`, `NullStream`, `Stream`, `ConsoleStream`, PC `FileStream`).
  - Phase 5: PC boot, binding and a minimal `Printer` that runs `prt1 << message << "Hello Printer 2!" << endLine;`.
  - Phase 6: `config_mc.hpp`, checked on the host.

  Every phase lists its test cases and its done criteria. MC hardware, chip files and startup code are deferred to a separate MC Hardware plan.
- **`_ProjectHandOff.md`:** added a dated correction under the session 1 entry (`Config Demand.md` was renamed `Config Request.md`), and this entry.

### Decisions

Recorded in spec §13 "Decisions" with the owner's wording; the summary here is for orientation.

| ID | Decision | Owner wording / date |
|---|---|---|
| 13.5 | Keywords `info` / `message` / `error` (replace `streamLog` / `streamMessage` / `streamError`; avoid C-library clashes such as `log`) | "streamMessage appeared harder to remember. Let's try to simplify keywords", 2026‑10‑24 |
| 13.6 | GPIO setup (remap if needed, GPIO for used interfaces) is owned by the Printer's MC boot | 2026‑10‑24 |
| 13.21 | PC emulates registers with static variables; a timed simulation with interrupts may replace them | 2026‑10‑24 |
| 13.23 | Printer boots only the hardware it needs; **CPU boot (PLL, clocks) is out of scope**, and the Printer must start without it, at the reset clock (F103: HSI 8 MHz). This revised a same-day version that included the CPU boot | "Printer must boot needed hardware only. CPU boot is out of scope", 2026‑10‑24 |
| 13.24 | Public `mc_chip.hpp` / `pc_chip.hpp` are the single chip-selection points; `mc_<chip>.hpp` / `pc_<chip>.hpp` / `<chip>_regs.hpp`; PC-only chip headers allowed | 2026‑10‑24 |
| 13.26 | HSE start failure is outside the Printer (it belongs to the CPU boot) | follows from 13.23, 2026‑10‑24 |
| 13.27 | Clock check: the Printer asserts if the running clock differs from `cfg::` at boot, or if it changes later; always on | "assert, we are on a strict MC territory", 2026‑10‑24 |
| 13.28 | User code is `#include "Printer/Printer.hpp"` + `Printer prt1 = {};`; the constructor boots; `boot()` is idempotent; `pStream` is `nullptr`-safe; two CMake configurations (PC, MC) | 2026‑10‑24 |
| 13.29 | Headers are `.hpp` throughout, each with a `__cplusplus` guard | "I'd go with .hpp throughout too", 2026‑10‑24 |
| 13.30 | Bare metal only: no vendor or CMSIS headers | 2026‑10‑24 |
| 13.32 | `bankSize` = one bank; RAM = 2 × `bankSize`; 0 = polled; no even-size rule | "B with bankSize – agree", 2026‑10‑24 |
| 13.33 | Minimum `bankSize` is 4 until sizes 1–3 are proven | "let's say minimal bank size is 4 elements, but we promise to test it somehow later", 2026‑10‑24 |
| Plan D-2 | Keep `Config/pc/config_platform.hpp` / `Config/mc/config_platform.hpp` (include-path selection) | 2026‑10‑24 |

Also kept from the 2026‑10‑23 draft: 13.3, 13.4, 13.7–13.10. Of the earlier open decisions in Part 1 §6, the spec effectively settles P-1 (names), P-2 (class with `operator<<`), P-12 (keep the config header, move `main()` out) and P-14 (separate USART sink instances). They are not marked decided here, because BY has not stated them as P-item decisions.

### Open

1. **BY's approval of `Config Base Plan.md`**, including its own decisions: D-1 (`Config/` location), D-3 (assert-based tests + `ctest`), D-4 (PC `FileStream` `ofstream` in aligned storage), and D-5 (`endLine` sends `'\n'` and then resets the colour).
2. **Spec §13.31:** who provides the bare-metal startup code (vector table, `Reset_Handler`), and the optional `cpuBoot()` hook plus a small 72 MHz CPU-boot example. Needed before the MC Hardware plan.
3. **Spec §13.34:** the promised bank-size 1–3 edge-case tests (head / body / tail states) decide whether the minimum drops to 1.
4. Remaining spec open items: 13.1, 13.2 (file markers), 13.11 / 13.12 / 13.25 (naming and visibility of the boot functions), 13.13 (16-colour model), 13.14 (unit suffixes), 13.15 (direct `printf` vs `prt1` state), 13.16 / 13.17 / 13.19 (files on MC, implicit open, `name` lifetime), 13.18 (`hold` by example), 13.20 (role mapping via enums), 13.22 (`flushDma` and `UartSettings` on PC, ISR writes, P-13).

### Notes for the incoming developer

- **Read the spec's §13 Decisions table before the body text.** Several decisions were revised during the session: the keyword names, the CPU boot, and `bufferSize` → `bankSize`. The table records both the current and the superseded wording.
- **Two traps found during review, both handled in the plan:**
  - (a) A PC `FileStream` holding a `std::ofstream` member is not constant-initialized, which breaks the "safe before boot" rule. Hence D-4.
  - (b) `endLine` must send `'\n'` *before* resetting the colour. In the other order, the stream is idle when the newline arrives, so the newline re-emits the colour.
- **`prt1` is dynamically initialized** (its constructor boots the hardware). Printing through `prt1` from another global's constructor is undefined behaviour; only the streams themselves are safe before boot.
- **The MC check in Phase 6 must stay a compile-only target.** Linking the MC and PC `inline constexpr` `cfg::` definitions into one executable would violate the one-definition rule.
- Start with Phase 0 only after BY approves the plan, and record the compiler and the result of every build here (house rule 5).

## Mailbox entry — 2026-10-20 (session 1) — **`Plans/Config/Config Demand.md` file created; the project is still a skeleton; Next is the demand discussion; then v0 stream API.**

> **Correction (2026-10-23).** `Config Demand.md` was later renamed `Plans/Config/Config Request.md`
> (commit ef0cd95). Its companion spec is `Plans/Config/Config Specification.md`. The heading above
> is left as written.

## Mailbox entry — 2026-10-20 (session 0) — **Bootstrap handoff written; the project is still a skeleton. Next is the v0 stream API.**

**State.** No implementation exists beyond the placeholder. `Printer/` is empty. No test suite, no
build verification after the `main.cpp` include fix, no owner decisions recorded.

### What exists

- `README.md`: purpose, intended features, and the `cons1 << ...` API seed.
- `CMakeLists.txt`: `cmake_minimum_required(3.20)`, project `printer_2`, C++17, executable
  `printer_2` from `main.cpp`, include root added.
- `main.cpp`: `#include "config.h"`.
- `config.h`: a `main()` stub returning 0.
- Empty `Printer/` directory.
- Local `.idea/` and `cmake-build-debug/` directories; the latter has a CMake cache from CLion
  (`CMAKE_PROJECT_NAME=printer_2`, MinGW 15.2.0, `MinGW Makefiles`).

### What was done this session

1. Added `CMakeLists.txt` (simple host build, no library target yet).
2. Wrote this handoff document.
3. `main.cpp` was found empty; the first compile failed at link with
   `undefined reference to 'WinMain'`. Fixed by including `config.h`, which is where `main()` lives.
4. Recorded the decision questions in Part 1 rather than inventing answers.

### What was not done

- The project has **not** been rebuilt after the include fix.
- No source file has been split into a library and a demo.
- No tests were written or run.
- Nothing was committed.

### Immediate next steps, in order

1. **Decide v0 scope.** Confirm or amend Part 1 §4 and the open decisions in §6. The first blocking
   choices are P-1 (names), P-3 (flush), P-6 (STL on MC), and P-7 (error model).
2. **Clean up the placeholder.** Move `main()` from `config.h` into `main.cpp`; keep `config.h` for
   compile-time switches only, or delete it if it has no content.
3. **Define the interfaces.** `Printer/ByteSink.h` and `Printer/Printer.h`, with no USART code.
4. **Implement the PC console sink** and the decimal integer/string/char insertions.
5. **Add the demo** in `main.cpp` using the README example.
6. **Add a library target** in `CMakeLists.txt` so `Printer/` can be linked independently of the
   demo.
7. **Add a host smoke test** (assert-based is enough for v0) covering integer boundaries and the
   replaceable sink.
8. **Then** add float/double formatting, hex mode, and only after that the USART abstraction.

### Suggested first commit boundary

One commit that contains: the API header, the sink interface, the PC sink, the demo, the library
target, and the smoke test. Keep the CMake/handoff bootstrap separate if the repository owner prefers
small history.

### Notes for the incoming developer

- Treat the README feature list as a roadmap, not as a description of the current code.
- Do not start with USART/DMA code. The stream API and the sink boundary are the parts that are hard
  to change later.
- `config.h` currently is empty. That is a placeholder, not a design pattern.
- The CLion/CMake cache names the project `printer_2`; keep the target name consistent unless the
  owner renames it.
- When a decision in Part 1 §6 is settled, move it into a short "Decisions" subsection in Part 2
  with the date and the owner's wording. Do not leave it only in prose.

---

## Template for new mailbox entries

```markdown
## Mailbox entry — YYYY-MM-DD (session) — **one-line headline**

**State.** What is built, what was run, what is green/red, what is committed/uncommitted.

### What changed

- file: change
- file: change

### Decisions

| ID | Decision | Owner wording / date |
|---|---|---|

### Open

1. ...
2. ...

### Notes for the incoming developer

- ...
```
