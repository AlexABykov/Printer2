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
- `config.h` currently contains a `main()` function. That is a placeholder, not a design pattern.
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
