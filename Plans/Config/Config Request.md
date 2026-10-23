# Request: implement Printer Configuration  settings
**Date:** 2026‑10‑23  
**Status:** Discussion (REMEMBER: only BY can change the Status)

## Proposal
Let's start with 3 configurations files:
1. Common `config.h`
2. MC specific `config_mc.h`
3. PC-specific `config_pc.h`

### Common

```cpp
// Baud Rate
USART2.settings.baudrate = 115200;
USART3.settings.buffer = 2048; // Set buffer to zero to turn off DMA
USART3.settings.baudrate = 230400; // USART3 buffer is zero by default - no DMA.
...

// Parity, Stop bits etc.
// ...
```

### MC

```cpp
// STM32F103VET6 (example)
// Registers pointers assignment to real registers (including GPIO)
// TODO: implement more platforms
...

// WARNING! No colored prints on MC

// Streams assignments (example)
streamLog.pStream = USART2.pStream;
streamMessage.pStream = USART2.pStream;
streamError.pStream = USART3.pStream;
// No color support for the whole MC platform
```

### PC

```cpp
// STM32F103VET6 (example)
// Registers pointers assignment to static variables (including GPIO)
// TODO: implement more registers

// WARNING! No colored prints in files

// Streams assignments (example)
streamLog = pcOut.console_0;
streamMessage = pcOut.console_0;
streamError = pcOut.file_1;
...

// Colors
STR_LOG.color = darkGray;
STR_MSG.color = darkGreen;
...
```

---

# Printer Specification (Draft)
**Status:** Draft (companion to the request above; to be promoted to its own `Printer Specification.md` once the request is accepted)  
**Date:** 2026‑10‑23

> The examples in the request above are illustrative and predate the decisions below. Where they differ, **this specification is authoritative**.

## 1. Scope

The new `Printer` module is a **standalone, portable** subsystem, intentionally incompatible with the old project's `Printer`. The old code is a source of algorithms only; call sites will be migrated by hand. Design goals:

- Same public `operator<<` streaming model, no dynamic allocation.
- Single global `prt1`; all targets are keywords.
- Buffering and colour are core algorithm choices, not afterthoughts.
- Platform-specific code (USART/DMA/GPIO, console, real files) is isolated from common logic.

## 2. Naming conventions

- **Types/classes:** `PascalCase` — `BaseStream`, `Stream`, `FileStream`, `UsartStream`, `ConsoleStream`, `Printer`, `Color`, `UartSettings`.
- **Instances, members, functions, `constexpr` values:** `camelCase` — `prt1`, `streamLog`, `streamMessage`, `streamError`, `file0` … `file8`, `pStream`, `pInner`, `hasColor`, `printerInit`, `printHexPrefix`, `platformHasColor`.
- **Preprocessor macros** (only when a macro is genuinely required): `SCREAMING_SNAKE_CASE` — e.g. `PLATFORM_HAS_COLOR` if a `#if` switch is needed. Preferred form is `constexpr bool` so that `#if` is avoided; if a macro is used, it is UPPERCASE.
- **Hardware register names** from CMSIS (`USART2`, `DMA1_Channel7`, `GPIOA`, `RCC`) are never renamed. Register macros collide with `USART2`-style names — the `UsartStream` instances are `mcUsart2` / `mcUsart3`, never `USART2`.

## 3. Object model

```
BaseStream  (abstract sink)
 ├─ UsartStream     MC: raw bytes to a USART (DMA or polled)
 ├─ ConsoleStream   PC: raw bytes to std::cout
 ├─ FileStream      PC: raw bytes to std::ofstream
 │                  MC: markers + raw bytes to a UsartStream
 └─ Stream          logical sink: lazy colour, forwards to an inner BaseStream
```

- `Printer::pStream` is `BaseStream*`. Both `Stream` (with colour) and `FileStream` (without) are accepted by `operator<<`.
- `Stream` owns the colour logic. It wraps a `BaseStream* pInner`. `Stream::sendByteRaw(b)` emits an ANSI colour sequence on transition, then forwards `b` to `pInner`. Colour is skipped entirely when `pInner->allowsAnsi() == false`.
- `FileStream` is a `BaseStream` in its own right. It never carries colour. On MC it forwards text to a `UsartStream*` and emits begin/end markers; on PC it writes to a `std::ofstream`.
- `prt1.error(fmt, ...)` is **removed** as a special method. Errors are written with the ordinary stream syntax against `streamError` (e.g. `prt1 << streamError << "..." << endLine`). printf-style output is a general capability (§6).

## 4. Target keywords

| Keyword | Type | Role |
|---|---|---|
| `streamLog` | `Stream` | General log |
| `streamMessage` | `Stream` | Message / production link |
| `streamError` | `Stream` | Errors |
| `file0` … `file8` | `FileStream` | Nine named file sinks (extensible later) |

- All are globals with static storage duration. `streamLog` / `streamMessage` / `streamError` / `fileN` are defined in common code; platform translation units bind their inner sinks via `printerInit()`.
- `prt1 << streamLog` sets `pStream = &streamLog` and leaves it there until the next switch. Same for `fileN`.
- **Stream name is a unique type of operator:** each target is a distinct global object, so overload resolution distinguishes it from `const char*`/integers without a tag type. Aliases are trivial (one more global of the same type).
- `KeyWords::File{"name"}` / `ClearFile{"name"}` are **kept but must be refactored** under the new `file0..file8` model (§13.1). The simple field form is `fileN.name = "path"`; the method form is `fileN.open("path")` (§8).
- **`StreamGuard` and `hold` are both provided.** `StreamGuard` is the RAII scope for multi-line switches; `hold` is the chain-local keyword released by `endLine` (§5, §13.8).

## 5. Color (lazy)

- **Emission is lazy:** on the first byte after a target switch, `Stream::sendByteRaw` emits the active colour; on `endLine` the printer calls `pStream->resetColor()`, which emits `ESC[0m` if a colour was active and marks the stream idle.
- **Mid-line switch:** `prt1 << streamLog << "a" << streamError << "b" << endLine` calls `resetColor()` on the outgoing stream before re-pointing `pStream`, so no escape ever leaks across a switch. Then `streamError` emits its own colour before `"b"`.
- **`flushDma` never alters colour state.** The operator flushes the DMA unconditionally, wherever it appears. The terminal's colour is unaffected by a flush, so the next byte must **not** re-emit the colour. Only `endLine` (and a mid-line target switch) resets colour.
- **Files never colour:** `Stream` skips colour when `pInner->allowsAnsi() == false`. `fileN` are `FileStream` instances, not `Stream` wrappers, so they never see colour at all. This enforces "no coloured prints in files" structurally, not by convention.
- **MC never colour:** in `config_mc.h`, `platformHasColor = false`; every `Stream` is bound to a `UsartStream` whose `allowsAnsi()` is `false`. The colour path compiles out and costs nothing at runtime.
- `resetColor()` and `sendByteRaw()` are virtual on `BaseStream` (no-op default); only `Stream` overrides them meaningfully.

## 6. printf-style output

Confirmed signature — **method on the stream** (Option B):

```cpp
streamError.printf("mask %d out of range", badMask);
file0.printf("t,pos\n");
streamLog.printf("frame %u received", n);
```

- A free-function format engine (`printFormatted(BaseStream&, const char*, va_list)`) does the work; `BaseStream::printf` forwards to it. Both `Stream` and `FileStream` expose it uniformly.
- Formatting: `%d %i %u %x %X %f %s %c %p %%`, `l`/`ll` length modifiers. `%f` precision still ignored in v1.
- No auto-newline and no automatic target — callers write `<< endLine` explicitly. The old "auto-newline on error" convenience is gone.
- `prt1.error(fmt, ...)` is removed; replaced by `streamError.printf(...)` or by stream-style writes to `streamError`.

## 7. Buffering (MC)

Buffering is a core algorithm: **two buffers or no buffering**. No intermediate strategies.

- `UartSettings::buffer == 0` → **polled** transmission (wait for TXE, write `DR`). No DMA, no RAM cost.
- `UartSettings::buffer > 0` → **double-buffered DMA**: two banks of `buffer / 2` bytes. DMA drains the active bank; CPU writes into the inactive bank. On bank-full: wait up to the backpressure budget for DMA completion; if free, swap banks and start transmission of the full bank; if still busy, drop the byte (backpressure by loss, same philosophy as the old module, but the budget is computed from `buffer`).
- `buffer` must be even (`static_assert`).
- A compile-time bound (`printerMaxDmaBuffer`) guards static allocation of the bank arrays. Values above it are a build error.
- The old `update_UsartX()` (non-blocking kick-out of a partially filled bank) is retained.

## 8. File naming

Each `fileN` has a public `const char* name` field, and matching open/close methods:

```cpp
file0.name = "logs/run1.csv";   // simple field assignment
prt1 << file0 << "t,pos,vel" << endLine;

file0.open("logs/run1.csv");    // assigns name and emits BEGIN marker
prt1 << file0 << "t,pos,vel" << endLine;
file0.close();                  // flushes and emits END marker
```

- Assignment is legal only before first write or after `close()`.
- **Default name if unset:** `"noname.log"` on both platforms.
- **PC:** `open()` opens `std::ofstream(name)` (append; `ClearFile` truncates). `close()` flushes and releases.
- **MC:** `open()` emits a BEGIN marker on the backing USART; `close()` emits an END marker. Bytes between markers are plain text. No actual file is created on the MC — the markers delimit text on the USART so a PC-side capture can be split back into files.
- **Marker text is identical on both platforms** (§13.2). A single splitter parses either platform's captured byte stream and produces the same `{name → content}` map.
- **Shutdown:** on program termination, all still-open `fileN` emit their END marker (§13.3).

## 9. Configuration files

Three data-only headers + one binding entry point. **No assignments at namespace scope** — all configuration is `constexpr` data consumed by `printerInit()`.

### `config.h` — common
- `constexpr` feature flags: `printHexPrefix`, `doubleScientific`, `doublePlusInExponent`, `useColorDefault`.
- Common types: `Color { r, g, b }`, `UartSettings { baudrate, buffer, dataBits, parity, stopBits }`.
- Default `UartSettings` constants used by both platforms as defaults.
- No references to platform object names (`USART2`, `console0`, …).

### `config_mc.h` — STM32F103 (example)
- `constexpr` register addresses, DMA channel numbers, `cpuTargetFrequency_MHz`.
- `constexpr bool platformHasColor = false`.
- Role → physical `UsartStream` mapping (which `mcUsartN` each `Stream` wraps).
- `fileN` → backing `UsartStream` mapping (presumably the same as `streamError` — §13.2).
- `UartSettings` constants per USART (baudrate, buffer size, parity, stop bits).

### `config_pc.h` — PC
- `constexpr` role → `ConsoleStream` / `FileStream` mapping.
- Per-`Stream` `Color` values.
- `constexpr bool platformHasColor = true`.
- File default paths.

### `printerInit()` — binding
Declared in common, **defined per platform** (`printerInit_mc.cpp` / `printerInit_pc.cpp`), selected by the CMake configuration. Responsibilities:

1. Bind `streamLog.pInner`, `streamMessage.pInner`, `streamError.pInner` and their colours from the config.
2. Bind each `fileN` to its backing USART (MC) or set default names (PC).
3. On MC, call `hardwareInit()` (§10) to program USART + DMA + GPIO from the config.

## 10. Hardware directory (MC)

- `Hardware/` holds `hardwareInit()` and the platform-specific `UsartStream` implementation (USART CR/BRR programming, DMA channel setup, optional GPIO alternate-function setup).
- **Clock configuration is not part of the Printer module.** A future standalone MC boot layer owns RCC/PLL; the Printer receives the resulting CPU frequency via a global (`cpuTargetFrequency_MHz`) and uses it only for the DMA backpressure budget.
- GPIO AF setup ownership (Printer vs host) is TBD (§13.6).
- On PC, `Hardware/` is empty or absent; CMake links only the PC translation units.

## 11. Build system

- CMake, one configuration per platform (MC / PC), chosen at configure time — same approach as the old project.
- `mc_*` / `pc_*` translation units are selected per configuration; polymorphism is link-time (abstract `BaseStream` in common, concrete implementations in `mc_` or `pc_` files).
- Exact target layout is deferred; the old project's scheme will be reused. Requirement: `Printer` is a self-contained CMake target that links into a new project without pulling in the old `Common/` tree.

## 12. Testing

- Tests target the new API only. The old `set_sendByte_*_override` hooks are **not** carried over; the new project will have a different test fracture.
- Recommended hook: a `CaptureStream : BaseStream` that records raw bytes, installed by tests either as a `pInner` on a `Stream` or directly as `prt1.pStream`. No test hook lives inside production `UsartStream` / `ConsoleStream`.
- Exact framework and file layout TBD (follow the new project's conventions).

## 13. Open items

1. **`KeyWords::File{"name"}` / `ClearFile{"name"}` refactoring.** Decided: the form needs refactoring. Proposal to confirm: drop the constructor form entirely and rely on `fileN.name = "..."` and `fileN.open("...")` / `fileN.close()`. The constructor form was only a way to redirect the single old `file` target; with nine explicit `fileN` keywords it is redundant.
2. **Marker text and trigger point — restated.**
   - *Requirement:* the marker string is identical on MC and PC, so one splitter produces the same `{name → content}` map from either platform's captured bytes. (Confirmed by "parsed string equal for both platforms".)
   - *Proposal:* `=== BEGIN <name> ===` / `=== END <name> ===`, where `<name>` is exactly `fileN.name`. On MC, markers go to the backing USART. On PC, the same markers are written **into the file itself** so a splitter sees identical bytes on both platforms.
   - *Trigger question, restated concretely:* if `file0.name = "run.csv"` and then `file0.close()` is called **without any bytes written**, should the output still contain `=== BEGIN run.csv ===` + `=== END run.csv ===` (empty file, markers present), or nothing at all (file never opened)? My proposal: markers are emitted at `open()` (or first assignment + first use), so an empty file still yields a marker pair. Confirm, or specify "markers only on first byte".
   - *Sub-question:* does the PC file contain the markers, or are they stripped before writing? (Proposal: kept, for platform parity.)
3. **Shutdown marker** — confirmed: emit END markers for still-open files on program termination. Need to decide *how* shutdown is detected on MC (atexit is unavailable/risky on bare metal; probably an explicit `printerDeinit()` called by the host boot layer).
4. **printf signature** — confirmed: `stream.printf(fmt, ...)` (Option B).
5. **Naming convention** — confirmed: types `FooBar`, instances `fooBar`, macros UPPERCASE. Glossary migration applied: `STR_LOG` → `streamLog`, `STR_MSG` → `streamMessage`, `STR_ERR` → `streamError`.
6. **GPIO AF setup** — owned by `Hardware/` (Printer) or by the host MC boot layer? Still open.
7. **Default `fileN.name`** — confirmed: `"noname.log"` on MC (and on PC for consistency).
8. **`StreamGuard` vs `hold`** — confirmed: **both**. `StreamGuard` is RAII for multi-line scopes; `hold` is a keyword that saves `pStream` into a single slot and is released by `endLine` (single level, no nesting). Interaction with `flushDma` and `printf`: only `endLine` releases a hold. `StreamGuard` construction on top of an active hold is allowed and is scoped independently.
9. **`flushDma`** — confirmed: the operator flushes DMA unconditionally, wherever it appears. Colour state is **not** changed by a flush (terminal retains colour; next byte does not re-emit).
10. **Glossary migration** — confirmed and applied throughout this draft (`streamLog` / `streamMessage` / `streamError`; `file0..file8`; `BaseStream`; `pStream`; `mcUsartN`).