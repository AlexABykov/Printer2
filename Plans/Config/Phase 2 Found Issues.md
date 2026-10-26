Here is a check of the Phase 2 plan against the base plan, the spec, `AGENTS.md` and the earlier `Found Issues.md`. I traced the CMake logic, the glob filters, the include-check regex, the console handling and the test-runner additions, and cross-checked the claimed scratch-build results.

## Verdict

**Approvable**, with a handful of small fixes. The plan is unusually thorough: it documents every departure from the base plan (§4), it names the assumptions (§5), and it shows the actual scratch-build evidence for the tricky parts (glob pickup, include check on both platform configurations, `-Werror` clean build, redirected run, `ctest`, failing check). Nothing in it contradicts the spec.

The issues below are mostly robustness of the new CMake and PC console code, not plan-level design problems.

## What is solid and does not need discussion

- **Platform selection by prefix** matches `AGENTS.md` §1 and spec §13.37 exactly: unprefixed files go to `common_files`, `pc_*` to `pc_files`, `mc_*` to `mc_files`; `printer` compiles `common + (pc or mc)`.
- **Include check** follows `AGENTS.md` §1 (“a header follows the strictest area of any file that includes it”): `common_files` + `mc_files` + the named `pc_` chip headers + `Tests/mc/` + `main.cpp` + `Tests/Tests.hpp`. The `pc_headers_under_common_rules` list captures the rule that shared driver code pulling in PC chip headers forces those headers under Common rules. Correct.
- **`printer_check_includes`** is the base plan's function, unchanged; the regex `^[ \t]*#[ \t]*include[ \t]*<` covers the legitimate spelling variants (`# include <x>`, `#include<x>`); commented-out lines are correctly skipped.
- **`CONFIGURE_DEPENDS`** on the globs is what makes the “new file picked up, new `<array>` fails the build” checks work without a manual reconfigure. The scratch evidence confirms both.
- **Move of `config_mc_check` to Phase 6** is correct: the base plan already said it was “for Phase 6”; the Phase 2 plan resolves the ambiguity and documents it in §4.
- **MC preset behaviour change** (build `printer` when a toolchain exists, rather than stop with “not available yet”) is a real departure from the base plan, but the plan documents it and justifies it (§5 Q-6, §4). The alternative would have stalled the first ARM compile until the MC Hardware plan.
- **`main.cpp` moves from “includes config.hpp” to “runs the tests”**, which is exactly the interim the base plan’s §1 program implies before Phase 5 adds the Printer lines back.
- **`TestResults.hpp` gets `<cstddef>` for `size_t`**; the base plan’s version got it only indirectly. Good catch.
- **UTF-8 console handling** on Windows is a real problem the base plan didn’t address, and the emoji summary needs it.

## Issues introduced by Phase 2

### P2-01 (Medium) — Console code page restore with `previousCodePage == 0`

**Location:** §3.6 `pc_Tests.cpp`, the `_WIN32` block.

```cpp
const UINT previousCodePage = GetConsoleOutputCP();
SetConsoleOutputCP(CP_UTF8);
…
SetConsoleOutputCP(previousCodePage);
```

When `printer_2` is run under `ctest`, or as `printer_2 > out.txt`, there may be no attached console. `GetConsoleOutputCP()` can return `0` in that case. `SetConsoleOutputCP(0)` at the end is not a valid code-page value (valid pages start at 1; `0` is not in the documented set), so the restore silently fails. The forward `SetConsoleOutputCP(CP_UTF8)` is also a no-op when there is no console; that is harmless, but the code should not pretend it can restore to `0`.

**Fix:**

```cpp
if (previousCodePage != 0) {
    SetConsoleOutputCP(previousCodePage);
}
```

The forward call can stay unconditional; it either works or is a no-op. This also matters because the scratch run did exercise the redirected path (`printer_2.exe > out.txt`, §3.9) and reported exit 0, which it would, but the restore line was not checked.

### P2-02 (Medium) — File creation order matters; nothing in the plan states it

**Location:** §3.1 `printer_check_includes(...)` lists `Tests/Tests.hpp` and `main.cpp`; §3.4–§3.8 create or edit them.

If CMake configures **before** `Tests/Tests.hpp` exists, `file(STRINGS "<missing>")` produces no lines and the check silently passes. The scratch build worked because all files already existed. In the actual implementation, the CMakeLists.txt from §3.1 can only be written after §3.4–§3.8 exist, otherwise the check is trivially green for those entries and the reader will believe the check ran when it did not.

**Fix:** add an explicit step order to §3: “Create `Tests/Tests.hpp` and `Tests/pc/*` first (§3.4–§3.8), then write `CMakeLists.txt`, `CMakePresets.json` and `cmake/arm-none-eabi.cmake` (§3.1–§3.3).” Optionally, make `printer_check_includes` fail when a listed path does not exist:

```cmake
if(NOT EXISTS "${file}")
    message(FATAL_ERROR "${file}: included in the check but missing")
endif()
```

That turns a silent gap into a configure error.

### P2-03 (Low) — `#include_next <header>` bypasses the check

**Location:** §3.1, regex `^[ \t]*#[ \t]*include[ \t]*<`.

`#include_next` is a GCC extension, rarely used in this project, but the check regex won't match it. If a Common or MC file ever contains `#include_next <array>`, the check passes and the code compiles on the host, then fails at the MC link or on the ARM compiler’s include path. The plan’s §6 Risks lists “only `#include <...>` lines” but does not call out `#include_next` specifically.

**Fix (cheap):** widen the file-level filter to include `include_next` and then a literal `<` in the header extraction, or add a sentence to §6: “`#include_next <…>` is not checked; code review covers it.” I would do the sentence; there is no real risk of anyone using `#include_next` in this project.

### P2-04 (Low) — Empty `printer` archive + `LINKER_LANGUAGE CXX`

**Location:** §3.1, `add_library(printer STATIC …)` with `set_target_properties(printer PROPERTIES LINKER_LANGUAGE CXX)`.

This works on MinGW (scratch evidence). It is a workaround for “no `.cpp` yet”; as soon as Phase 4 adds `Stream.cpp`, the property is unnecessary and can be removed. On a different linker or `ar` implementation, an empty archive could warn. The plan already notes it is temporary. No change needed beyond:

- **Do not carry `LINKER_LANGUAGE CXX` forever.** Remove it in Phase 4 when the first `.cpp` arrives.
- Optionally, create a one-line `Printer/Version.cpp` (or `Hardware/PrinterVersion.cpp`) that returns `const char* printerVersion();`, so the archive is real from Phase 2. That gives a natural symbol to link and removes the workaround. Not necessary; a note is enough.

### P2-05 (Low) — `add_test(COMMAND printer_2)` vs `$<TARGET_FILE:printer_2>`

**Location:** §3.1, `add_test(NAME printer_2 COMMAND printer_2)`.

CMake resolves a bare `printer_2` here to the target’s output location because a target by that name exists. This works and is documented behaviour. The more explicit spelling is:

```cmake
add_test(NAME printer_2 COMMAND $<TARGET_FILE:printer_2>)
```

It is unambiguous and survives a rename of the target. The plan’s version is correct; the explicit form is a style preference, not a bug.

### P2-06 (Low) — `enable_testing()` placement

**Location:** §3.1, inside `if(PRINTER_PLATFORM STREQUAL "PC")`.

`enable_testing()` is conventionally called at the top level. Inside an `if` in the same directory scope it still works (`add_test` follows immediately), but the placement is non-standard and will look odd to reviewers. Acceptable; a comment next to it would help (“only enabled on the PC; the MC suite does not run under `ctest`”).

### P2-07 (Documentation consistency, Low) — Base plan still says Phase 2 builds `config_mc_check` and stops the `mc` preset

**Location:** Base plan Phase 2, item 4 and item 5; this plan §4.

The base plan says:
- `config_mc_check` is a Phase 2 target → moved to Phase 6 here.
- `mc` preset stops with a clear message if no ARM toolchain → still true in the not-found path, but with a toolchain this plan builds `printer`, which the base plan didn’t anticipate.

Both departures are documented in §4, but the base plan itself is not updated. This will confuse a reader who goes from base plan to Phase 2 plan. **Recommendation:** a short Phase 2 note in the base plan, or a note in the handoff entry, saying “Phase 2 plan §4 documents two departures from this table; the table reflects the pre-implementation plan”.

### P2-08 (Very Low) — `main.cpp` is added to the include check but the file is not mentioned in `AGENTS.md` §1

**Location:** §3.1, §5 Q-3.

The plan does the right thing (checking `main.cpp` under Common rules, because the MC compiles it) but `AGENTS.md` §1’s area table doesn’t mention `main.cpp`. Q-3 proposes adding the row. That is a doc change, not a code change; no blocker. It should be done in the same revision as Phase 2, so future contributors see the rule in the table rather than only in the plan.

## Items from `Found Issues.md` that this plan does not resolve (and correctly so)

None of the High/Medium items in `Found Issues.md` are Phase 2’s responsibility:

- **F-01 … F-06, F-08, F-11** — Phase 4/5 API and test issues. Unchanged, correctly deferred.
- **F-09** — `FileStream` default name. Phase 4.
- **F-10** — `isValid()` does not check `stopBits`/`flowControl`. Still in `config.hpp`. This plan doesn’t touch `config.hpp`, so it remains an open Medium item to resolve before Phase 3 static_asserts on `isValid`.
- **F-12** — Stale base-plan build status. This plan correctly does not fix the base plan’s text, but P2-07 above is a related doc-consistency point.
- **F-13** — Manual include check until Phase 2. This plan **resolves** it: the include check becomes automatic from this phase on.
- **F-14 … F-17** — Phase 4/5.

Also unchanged from the earlier review of `config.hpp`: **D-1** (`baudRate` vs `baudrate` in spec) and **D-2** (`auto` vs explicit `const char*` for `defaultFileName`). Not Phase 2’s concern; still worth fixing in the next spec revision.

## Risk paths worth adding to the plan

The plan’s §3.10 manual checks cover the include-check path and the failing-check path well. Two more risk paths are worth adding to the manual-check list:

| New check | Why |
|---|---|
| Delete `Tests/Tests.hpp`, run `cmake --preset pc`, restore | Confirms P2-02: either the check errors (with the recommended guard) or the silent-gap behaviour is observed and recorded |
| Run `printer_2.exe > out.txt` under a UTF-8 console and a non-UTF-8 console | Confirms P2-01: with the recommended guard, no code page is corrupted; without the guard, `SetConsoleOutputCP(0)` is a silent no-op and `chcp` shows the original code page afterwards |

## Recommendation

**Approve Phase 2** with the following amendments folded into the plan before implementation:

1. **P2-01:** guard `SetConsoleOutputCP(previousCodePage)` with `previousCodePage != 0`.
2. **P2-02:** state the file creation order in §3 (tests and `Tests/Tests.hpp` first, then CMake files), and optionally make `printer_check_includes` fail on a missing file.
3. **P2-03:** add one sentence to §6 that `#include_next <…>` is not caught by the check and is covered by code review.
4. **P2-04:** note in §3.1 that `LINKER_LANGUAGE CXX` is temporary and is removed in Phase 4 when `Stream.cpp` lands; optionally add a one-line `Printer/PrinterVersion.cpp` instead, so the archive is real from Phase 2.
5. **P2-07:** add a Phase 2 note in the base plan (or the handoff entry) recording the two departures documented in §4, so the base plan does not read as current.
6. **P2-08:** add the `main.cpp` row to `AGENTS.md` §1 in the same revision, per §5 Q-3.
7. Optional style items P2-05 (`$<TARGET_FILE:printer_2>`) and P2-06 (`enable_testing()` placement + comment).

None of these change the plan’s structure or the scratch evidence; they are robustness and documentation fixes that are cheap to make now and expensive to discover later.