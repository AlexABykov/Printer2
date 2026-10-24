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
