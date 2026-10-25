# Found Issues.md

**Status:** Review document for `README.md`, `Config Specification.md`, `Config Base Plan.md`, `Phase 1 plan.md`.  
**Purpose:** Record concrete gaps, inconsistencies and risky implementation paths found while checking whether the plan is ready to approve and implement.  
**Scope:** Phase 1 is checked closely; the base plan is checked for whether its later phases are safe to implement as written.

## Summary

- **Phase 1 plan (`Printer/Config/config.hpp`) is mostly sound.** The header content, compile-time checks and scratch-build notes are consistent enough to implement after a few documentation fixes.
- **The base plan is not yet safe to approve for Phases 4–5 as written.** There are concrete API/implementation gaps around `pStream == nullptr`, `autoFlush`, `constexpr` keyword objects, the PC `error` target and `fileStreams` indexing.
- **The 635-item sweep found many non-issues.** The actionable set is much smaller, but the high-severity items below are enough to show the plan should not be implemented literally without fixes.

## Severity legend

| Severity | Meaning |
|---|---|
| High | Likely compile failure, crash, silently lost output or a test/plan contradiction. Fix before approving the affected phase. |
| Medium | Under-specified behaviour that will cause rework or ambiguous tests. Fix before implementation or explicitly defer. |
| Low | Documentation, naming or minor consistency issue. |

## High-severity issues

### F-01 — Target switches call `resetColor()` on a null `pStream`

**Location:** Base plan Phase 5, `Printer::operator<<(Message/Error/Debug/File<N>/BaseStream&)`.  
**Problem:** The plan says all target-switch overloads “call `resetColor()` on the outgoing target first, then re-point `pStream`.” A fresh local `Printer` has `pStream == nullptr`; tests B5, P1 and P2 start with a local `Printer` and immediately do `p << message` or `p << stream`. That would dereference null unless every switch guards first.  
**Impact:** Crash in tests and in any `Printer` that has not selected a target yet. P3 only checks `p << "x"` with null `pStream`; it does not cover target switches.  
**Fix:** In every switch overload:
```cpp
if (pStream != nullptr && pStream != next) {
    pStream->resetColor();
}
pStream = next;
```
Add a test for `p << message` and `p << localStream` on a fresh local `Printer`.

### F-02 — `autoFlush` cannot be detected through `BaseStream*`

**Location:** Base plan Phase 5, `operator<<(const char*)`; Phase 4 `Stream::autoFlush`; test P4.  
**Problem:** `pStream` is `BaseStream*`. The plan says “If the target is a `Stream` with `autoFlush`, it then flushes.” But `BaseStream` exposes no `autoFlush` query, RTTI is disabled on MC, and there is no `operator<<(Stream&)` overload listed. P4 uses a local `Stream e` with `e.autoFlush = true` and expects flushes after each string. As written, `p << e` resolves to `operator<<(BaseStream&)`, so the auto-flush information is lost.  
**Impact:** P4 cannot pass as specified; `error` auto-flush would not work through the generic path.  
**Fix:** Choose one and write it into the spec:
1. Add `virtual bool autoFlush() const { return false; }` to `BaseStream`, override in `Stream`; or
2. Add `operator<<(Stream&)` to `Printer` and store a `Stream* pSelectedStream` alongside `BaseStream* pStream`.

Option 2 is smaller, but it must be specified in the API and tested.

### F-03 — `File<N>` keyword methods must be `const`, because keywords are `inline constexpr`

**Location:** Spec §8, base plan Phase 4 `Keywords.hpp`.  
**Problem:** The plan defines base keywords as `inline constexpr File0 file0{};` etc. A `constexpr` object is const. The spec examples then call mutating-looking methods:
```cpp
file0.setName("logs/run1.csv");
file0.open("logs/run1.csv");
file0.close();
```
If `File<N>::setName/open/close` are ordinary non-const member functions, these calls do not compile.  
**Impact:** The documented file API fails to compile.  
**Fix:** Make the keyword forwarding methods `const`:
```cpp
constexpr void setName(const char* name) const { fileStreams[N].setName(name); }
constexpr void open(const char* name, OpenMode mode = OpenMode::append) const { ... }
constexpr void close() const { ... }
```
The keyword object stays empty and const; the mutable state lives in `fileStreams[N]`.

### F-04 — PC default `errorTarget = file1` points to a closed file sink

**Location:** Spec §9 `config_pc.hpp`; base plan Phase 3/Phase 5, B2.  
**Problem:** `errorTarget = PcSink::file1`, and `FileStream::sendByteRaw` drops bytes while `isOpen == false` (Phase 4, provisional §13.17). Nothing in the plan opens `file1` during boot or binding. B2 only checks that `errorStream.pInner == &fileStreams[1]`; it does not write an error.  
**Impact:** On PC, `prt2 << error << "..."` silently loses output until the user opens `file1`. That contradicts the “`error` is like `std::cerr`” role.  
**Fix:** Decide one before Phase 5:
- change the demo PC config to `errorTarget = PcSink::console0`; or
- open `file1` explicitly in the demo/boot; or
- define an auto-open rule for `FileStream` (spec §13.17), but that is a larger behaviour change.

At minimum, add a test that writes an error before `file1.open()` and records the expected result.

### F-05 — `fileStreams[N]` index initialization is unspecified

**Location:** Base plan Phase 4, PC `FileStream`; D-4.  
**Problem:** `FileStream` holds “its index” so `pc_FileStream.cpp` can select the correct `std::ofstream` from the function-local static array. But `inline FileStream fileStreams[9];` default-constructs every element; nothing in the plan says how element `N` receives index `N`. If all indices default to 0, `file6.open()` writes through `ofstream[0]`.  
**Impact:** Wrong file is opened; B5 only checks `pStream == &fileStreams[6]`, not which `ofstream` is used.  
**Fix:** Specify an explicit initializer, for example:
```cpp
inline FileStream fileStreams[9] = {
    FileStream{0}, FileStream{1}, ..., FileStream{8}
};
```
or use a `constexpr` index-aware factory. Add a test that opens `file6` and verifies it reaches `ofstream[6]`.

### F-06 — Same-target switches reset colour unnecessarily

**Location:** Base plan Phase 5, all target-switch overloads.  
**Problem:** The plan says every switch calls `resetColor()` on the outgoing target. If the new target is the same stream, this emits `ESC[0m` and marks the stream idle, so the next byte re-emits the colour. For example, `p << message << "a" << message << "b"` would produce a reset/colour sequence between `a` and `b` instead of continuous colour. B5 repeats `message` through an alias; with `allowsAnsi == true` the capture would not be plain `"ab"`.  
**Impact:** Visual glitch and a test inconsistency.  
**Fix:** Reset only when `pStream != next`. This also combines naturally with F-01.

### F-07 — `endLine` has no null guard

**Location:** Base plan Phase 5, `endLine` keyword.  
**Problem:** `endLine` sends `'\n'`, calls `pStream->resetColor()`, then `pStream->flush()`. A fresh local `Printer` has `pStream == nullptr`. P3 does not test `p << endLine`.  
**Impact:** Crash if `endLine` is streamed before any target is selected.  
**Fix:** Treat null `pStream` like `nullSink`: do nothing and return. Add a test `p << endLine` on a fresh local `Printer`.

## Medium-severity issues

### F-08 — `EndLine` keyword type is missing from the Phase 4 keyword list

**Location:** Base plan Phase 4 `Keywords.hpp`; Phase 5 `endLine`.  
**Problem:** Phase 4 lists `Message`, `Error`, `Debug`, `File<N>` and base keywords. Phase 5 introduces `endLine` as “a keyword object”, but no `EndLine` type or global is listed.  
**Impact:** Implementation has to invent the type/overload.  
**Fix:** Add `struct EndLine {}; inline constexpr EndLine endLine{};` and the corresponding `Printer::operator<<(EndLine)` to the plan.

### F-09 — `FileStream` default name is not specified in Phase 4

**Location:** Base plan Phase 4, PC `FileStream`.  
**Problem:** Spec §8 says the default name is `cfg::defaultFileName` (`"noname.log"`). Phase 4 describes `setName`, `open`, `close`, `sendByteRaw`, `flush`, `allowsAnsi`, but not the constructor’s default name.  
**Impact:** `fileStreams[N]` may start with a null name; behaviour depends on an unspecified path.  
**Fix:** Specify the `constexpr FileStream` constructor to default `name` to `cfg::defaultFileName`, or explicitly say the name is unset until `setName/open`.

### F-10 — `isValid()` does not validate `stopBits` or `flowControl`

**Location:** Phase 1 plan, `config.hpp`.  
**Problem:** `isValid()` checks `baudrate`, `bankSize` and `dataBits/parity`, but not `stopBits` or `flowControl`. An invalid enum value can only appear through a cast, so this is not urgent, but the function is presented as “the USART can be programmed from it as written.”  
**Impact:** A bad casted config passes `static_assert`.  
**Fix:** Either add checks:
```cpp
const bool stopOk = s.stopBits == StopBits::one || s.stopBits == StopBits::two;
const bool flowOk = s.flowControl == FlowControl::none || s.flowControl == FlowControl::rtsCts;
```
or document that enum validity is the caller’s responsibility.

### F-11 — B1’s `static_assert` may not compile if `pInner` is private

**Location:** Base plan Phase 5, B1.  
**Problem:** B1 proposes `static_assert`s on `constexpr Stream probe{};` showing `probe.pInner == &nullSink`. If `pInner` is private (as a member), the test cannot access it. The plan does not specify a public accessor or friendship.  
**Impact:** The compile-time “safe before boot” test may need API changes at implementation time.  
**Fix:** Add a `constexpr BaseStream* inner() const { return pInner; }` accessor, or make the test a friend, or explicitly make `pInner` public for the demo.

### F-12 — Base plan build status and Phase 0 are stale

**Location:** Base plan header and Phase 0; Phase 1 plan §2.  
**Problem:** Base plan says `Printer/` is empty and root `config.h` still holds the `main()` stub. Phase 1 plan says `config.h` is already empty and `main()` already lives in `main.cpp`, so Phase 0 step 1 is already done.  
**Impact:** Confusing approval state; implementer may repeat or skip a step.  
**Fix:** Update the base plan’s build-status note and Phase 0 to match the actual repository, or move the correction into the handoff and reference it.

### F-13 — Include check is manual until Phase 2

**Location:** Phase 1 plan §3.3, §6.  
**Problem:** Phase 1 relies on a manual `grep` and review to enforce the `<cstdint>`-only rule. The automatic check arrives in Phase 2.  
**Impact:** A forbidden include can slip in between Phase 1 and Phase 2.  
**Fix:** Acceptable for one phase, but record it as a known gap and run the manual check in the Phase 1 handoff entry.

## Low-severity issues

### F-14 — `OpenMode` is used before being defined

**Location:** Base plan Phase 4, PC `FileStream`.  
**Problem:** The plan mentions `open(name, OpenMode = append)` but does not define `enum class OpenMode { append, truncate }` in the Phase 4 text. Spec §8 defines it.  
**Fix:** Add the enum to `FileStream.hpp` or explicitly reference spec §8.

### F-15 — `CaptureStream` default `allowsAnsi` is unspecified

**Location:** Base plan Phase 4, `Tests/pc/CaptureStream`.  
**Problem:** C tests set `allowsAnsi`; B5 expects plain `"ab"` and may rely on `allowsAnsi == false`. The default is not stated.  
**Fix:** Specify the default, preferably `false`, and set it explicitly in tests that need colour.

### F-16 — `operator<<(const char*)` null pointer behaviour is unspecified

**Location:** Base plan Phase 5.  
**Problem:** `p << (const char*)nullptr` would dereference. Not likely in tests, but the API is public.  
**Fix:** Either guard and treat null as empty, or document it as undefined.

### F-17 — PcSink validity `static_assert` is not specified

**Location:** Base plan Phase 3.  
**Problem:** Phase 3 says “`static_assert` that each role target is a valid `PcSink` value”, but `PcSink` is an enum class; the plan does not show how validity is checked.  
**Fix:** Specify a small `constexpr bool isValid(PcSink)` or an explicit disjunction.

## Potential risky paths to test

These are the paths most likely to expose plan gaps during implementation.

| ID | Risky path | Why it is risky | Suggested test |
|---|---|---|---|
| R-01 | Fresh local `Printer`, then `p << message` | `pStream == nullptr`; switch may call `resetColor()` on null | Add to PrinterTests: `p << message << "x"` on a fresh `Printer` |
| R-02 | Fresh local `Printer`, then `p << endLine` | `endLine` dereferences `pStream` | Add P5: `p << endLine` does not crash and emits nothing |
| R-03 | `p << streamA << "a" << streamA << "b"` with colour enabled | Same-target switch resets colour | Add P6: capture should be continuous colour, no intermediate `ESC[0m` |
| R-04 | `p << e << "a" << "b"` with `e.autoFlush == true` | `autoFlush` not reachable through `BaseStream*` | P4, plus a compile check that `p << e` selects the `Stream&` overload |
| R-05 | `p << static_cast<BaseStream&>(e) << "a"` where `e` is a `Stream` with `autoFlush` | If only `BaseStream&` is used, auto-flush is lost | Decide and document whether this flushes; test the chosen rule |
| R-06 | `file0.setName(...)`, `file0.open(...)`, `file0.close()` | `file0` is `inline constexpr`; methods must be `const` | Compile-only test in ConfigTests or FileTests |
| R-07 | `file6.open(...)`, write, close | `fileStreams[6]` must use `ofstream[6]` | Add a PC test that opens `file6` and verifies the written file, not `file0` |
| R-08 | `prt2 << error << "x"` before `file1.open()` | Default PC error target is a closed file sink | Add a test and decide: dropped, auto-open, or change target to `console0` |
| R-09 | `prt2.boot()` after `messageStream.pInner` was replaced | Idempotent boot must not rebind | B4 already covers; keep it |
| R-10 | Run `printer_2` with stdout redirected | VT mode may fail; `console0.allowsAnsi()` must become false | B3 under `ctest` or `printer_2 > out.txt` |
| R-11 | `File<9>` | Must be a compile error | One manual compile check, then remove |
| R-12 | `#include <array>` in `config.hpp` after Phase 2 | Include allow-list must fail configure/build | Manual check in Phase 2 |

## Approval recommendation

- **Phase 1 plan:** Approve after minor cleanup: update the stale Phase 0/build-status text and optionally tighten `isValid()` for `stopBits`/`flowControl`. The header itself is ready to implement.
- **Base plan Phases 4–5:** Do not approve as written until F-01, F-02, F-03, F-04, F-05 and F-06 are resolved in the plan and the spec. These are not cosmetic; they affect whether the documented API compiles and whether tests P1/P2/P4/B5 can pass.
- **Spec updates needed:** The fixes for F-02, F-03 and F-04 change or clarify the public model, so the spec should be updated together with the base plan: how `autoFlush` is exposed, that `File<N>` forwarding methods are `const`, and what the PC `error` target does when its file is closed.