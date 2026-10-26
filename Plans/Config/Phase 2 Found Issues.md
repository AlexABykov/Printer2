Here is a check of the revised Phase 2 plan against the previous review and its own §8 resolution table.

## Verdict

**Approvable.** Every finding from `Phase 2 Found Issues.md` is either fixed in the code or consciously declined with a stated reason. The changes are the ones the review recommended, they do not introduce new behaviour, and the scratch re-build evidence in the build-status block backs the tricky ones (P2-03 `#include_next`, P2-01 redirected code page, P2-02 missing file).

Two items in §8 need verification **outside** this document because they claim edits to files not in front of me.

## Per-finding verification

### P2-01 — Code page restore guard

**§3.6:**
```cpp
#ifdef _WIN32
    if (previousCodePage != 0) {   // 0: no console attached, nothing to restore
        SetConsoleOutputCP(previousCodePage);
    }
#endif
```
✅ Fixed. The comment names the exact condition the review described. §3.10 check 3 now exercises the redirected path under two code pages, which was the missing coverage.

### P2-02 — Missing checked file

The plan's position is **"no change needed, the premise doesn't hold"**, and it says `file(STRINGS)` on a missing file fails configure. I independently believe this is right (default CMake behaviour since very early versions), and the scratch table now records:

> `Tests/Tests.hpp` removed, `cmake --preset pc` → configure fails: `file STRINGS file "…/Tests/Tests.hpp" cannot be read`

✅ Resolved, with evidence. The §3.1 note ("A listed file that is missing fails configure … The order in which §3.1–§3.8 are written therefore doesn't matter") is now accurate.

One minor wording nit, not blocking: "order of writing doesn't matter" is true only in the sense that any order will eventually produce a valid tree, because a build mid-way fails loudly. A reader might take it to mean "you can build at any point". Suggest: *"The order in which §3.1–§3.8 are written doesn't matter; a build before all listed files exist fails configure with 'cannot be read', which is the intended guard."*

### P2-03 — `#include_next`

**§3.1** now uses:

```cmake
file(STRINGS "${file}" lines REGEX "^[ \t]*#[ \t]*include(_next)?[ \t]*<")
…
string(REGEX REPLACE "^[ \t]*#[ \t]*include(_next)?[ \t]*<([^>]+)>.*" "\\2" header "${line}")
```

✅ Fixed. Tracing both cases:

- `#include <array>` → group 1 empty, group 2 = `array`. ✓
- `#include_next <array>` → group 1 = `_next`, group 2 = `array`. ✓
- `# include<array>` → `^[ \t]*#[ \t]*include` still matches; group 2 = `array`. ✓

The extra `_next` cannot cause a false positive for `#include X` because `(_next)?` only matches when the next characters are `_next`, and then `<` must follow. `#includeX` → no match. Good. §6 is updated to say the check now covers `include_next`. Scratch evidence confirms the three spellings are rejected.

### P2-04 — `LINKER_LANGUAGE CXX` on the empty archive

**§3.1 note:** "It is a temporary workaround: Phase 4 removes the line when its first `.cpp` lands."

✅ Fixed as a note. The optional `PrinterVersion.cpp` was declined with the rationale "a file with no spec basis". Reasonable; the workaround costs nothing at this stage and Phase 4 removes it naturally.

One nit: the note says Phase 4 "removes the line", but a reader should not remove it while the archive is still empty. Since Phase 4 adds `Stream.cpp` in the same step, the wording is fine.

### P2-05 — `add_test` target name

No change, with a stated rationale: `add_test(COMMAND printer_2)` resolves a target name to its output file, which is documented CMake behaviour; a rename would change the test's `NAME` and the `COMMAND` together. ✅ Reasonable.

### P2-06 — `enable_testing()` placement

**§3.1:**
```cmake
enable_testing()   # PC only: ctest runs the demo with its tests; the MC suite runs on the target
```
✅ Fixed with an inline comment. Placement inside the `if` is now justified.

### P2-07 — Base plan still describes the original Phase 2

The plan's §8 claims a "dated note in the base plan's Phase 2 points to this plan's §4". ⚠️ **Cannot verify** — the base plan is not in this bundle. Please confirm the base plan actually has the note, or add it as a follow-up step in §3.11.

If the note is missing, the practical consequence is that the base plan's Phase 2 table (item 4 "`config_mc_check` in Phase 2", item 5 "`mc` preset stops with a clear message") reads as current and contradicts §4 of this plan. That is exactly the kind of doc drift the handoff entries are meant to prevent.

### P2-08 — `main.cpp` in `AGENTS.md` §1

The plan schedules this as an implementation step (§3.11): "add `main.cpp` to the Common row of the area table in the same revision, so the rule is in the table and not only in this plan. If BY decides otherwise, `main.cpp` leaves the §3.1 check list instead."

✅ Correct shape: the doc change is coupled to the code decision, not silently made. ⚠️ Depends on Q-3's answer.

## What is still open (and correctly so)

- **P2-07 / P2-08 verification.** Outside this document.
- **§5 Q-1…Q-7.** All have provisional answers used in §3. Not approval blockers; the plan is implementable against the provisional answers, and Q-6 (ARM toolchain present, `printer` only) is explicitly flagged as untested.

## Risk paths that survive the fixes

The two paths worth a second manual check in §3.10 after the review:

1. **A Common file with `#include_next <array>` was correctly rejected in scratch, but the plan does not specify a check for a legitimate `#include_next <stdint.h>`.** This is the interesting case — the allow-list contains `stdint.h`, so `#include_next <stdint.h>` should pass. Worth adding as a fifth manual check: in a Common file, `#include_next <stdint.h>` must build (in scratch it isn't tested). If it fails, the regex is too strict and the fix is to only reject `_next` when the header is *not* allowed, or to accept `include_next` with the same allow-list.

   In practice this is a corner case unlikely to matter — `#include_next` is a libc mechanism, not user code — but it is the natural boundary of the fix and cheap to test.

2. **Code page restore under `chcp 65001 → run → chcp` is tested (§3.10 check 3), but the same check under `chcp 437 → run` is also in the plan, which is good.** Adding a third case: run from a shell that already has a non-UTF-8 code page set (e.g. `chcp 437` in cmd, then launch the exe) and confirm emoji is mojibake but no crash and code page restored to 437. This validates that the fallback path behaves reasonably when UTF-8 can't be set (e.g. the console rejects it).

## Approval recommendation

**Approve Phase 2** with:

1. Confirm P2-07 and P2-08 in the actual files (base plan, `AGENTS.md`) before implementing, since the plan's §8 says they are fixed.
2. Optional: add the `#include_next <stdint.h>` positive case to §3.10 as a fifth check. Not blocking.
3. Optional: tighten the §3.1 wording "The order … doesn't matter" to make clear that a *build* mid-way fails; only *writing* in any order works.

No re-review needed for the fixes themselves — they are exact and their evidence is stated. The plan is ready to implement against the provisional answers in §5 once the two outside checks are confirmed.

## Resolution (2026‑10‑26)

Resolved in [`Phase 2 plan.md`](Phase%202%20plan.md) §8.2. The review text above is unchanged.
- **P2-07 confirmed:** the base-plan note exists; "accepts" became "reads".
- **P2-08 confirmed as designed:** it waits for Q-3.
- **The `#include_next <stdint.h>` case was run in scratch.** The include check passes it, but `-Wpedantic -Werror` rejects every `#include_next` at compile time, so it can't build. Recorded; no code change.
- **The `chcp 437` run was added** to §3.10 check 3. Because the runner sets UTF-8 itself, the expected result is a correct ✅, not mojibake.
- **The wording was tightened.**