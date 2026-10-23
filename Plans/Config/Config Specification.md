# Printer Configuration Specification (Draft)
**Status:** Draft. Companion to [`Config Request.md`](Config%20Request.md) in this folder.  
**Date:** 2026‑10‑23 (revised 2026‑10‑23 after review)

> The examples in `Config Request.md` are illustrative and predate the decisions below. Where they differ, **this specification is authoritative**.
>
> **Scope of this document.** §9 is the configuration specification proper. §§3–8 and §§10–12 are **Printer-wide context** the configuration depends on; they will move to a future `Printer Specification.md` once the request is accepted. §13 tracks decisions and open items for both.

## 1. Scope

The new `Printer` module is a **standalone, portable** subsystem, intentionally incompatible with the old project's `Printer`. The old code is a source of algorithms only; call sites will be migrated by hand. Design goals:

- Same public `operator<<` streaming model, no dynamic allocation.
- Single global `prt1`; all targets are keywords.
- Buffering and colour are core algorithm choices, not afterthoughts.
- Platform-specific code (USART/DMA/GPIO, console, real files) is isolated from common logic.

## 2. Naming conventions

- **Types/classes:** `PascalCase` — `BaseStream`, `Stream`, `FileStream`, `UsartStream`, `ConsoleStream`, `NullStream`, `Printer`, `Color`, `UartSettings`.
- **Instances, members, functions, `constexpr` values:** `camelCase` — `prt1`, `streamLog`, `streamMessage`, `streamError`, `file0` … `file8`, `pStream`, `pInner`, `hasColor`, `printerInit`, `printerPlatformInit`, `printHexPrefix`, `platformHasColor`.
- **Unit-suffix exception:** a `constexpr` holding a physical quantity may end in `_Unit` — `cpuFrequency_MHz`, `apb1Frequency_Hz`, `apb2Frequency_Hz`. The suffix keeps the unit visible at every use site (§13.14).
- **Configuration constants** live inside `namespace cfg`. The namespace is `cfg`, not `Config`.
- **Preprocessor macros** (only when a macro is genuinely required): `SCREAMING_SNAKE_CASE` — e.g. `PLATFORM_HAS_COLOR` if a `#if` switch is needed. Preferred form is `constexpr bool` so `#if` is avoided; if a macro is used, it is UPPERCASE.
- **Hardware register names** from CMSIS (`USART2`, `DMA1_Channel7`, `GPIOA`, `RCC`) are never renamed. Register macros collide with `USART2`-style names — the `UsartStream` instances are `mcUsart2` / `mcUsart3`, never `USART2`.

## 3. Object model

```
BaseStream  (abstract sink)
 ├─ NullStream      both: drops every byte (default pInner, §4)
 ├─ UsartStream<N>  MC: raw bytes to a USART (N > 0: DMA, N == 0: polled)
 ├─ ConsoleStream   PC: raw bytes to std::cout
 ├─ FileStream      PC: raw bytes to std::ofstream
 │                  MC: markers + raw bytes to a UsartStream
 └─ Stream          logical sink: lazy colour, forwards to an inner BaseStream

Instances:
  MC transports:     mcUsart2, mcUsart3
  PC transports:     console0
  Common keywords:   streamLog, streamMessage, streamError (Stream)
                     file0..file8 (FileStream)
```

- `Printer::pStream` is `BaseStream*`. Both `Stream` (with colour) and `FileStream` (without) are accepted by `operator<<`.
- `Stream` owns the colour logic. It wraps a `BaseStream* pInner`. `Stream::sendByteRaw(b)` emits an ANSI colour sequence on transition, then forwards `b` to `pInner`. When colour is disabled (§5, precedence) the bytes go straight through.
- `FileStream` is a `BaseStream` in its own right. It never carries colour. On MC it forwards text to a `UsartStream` and emits begin/end markers; on PC it writes to its own `std::ofstream`. A `Stream` may also use a `fileN` as its `pInner` (e.g. `streamError` → `file1` on PC, §9); colour is then skipped because `FileStream::allowsAnsi()` is `false`.
- `BaseStream` exposes `printf(fmt, ...)` (§6), `sendByteRaw`, `flush`, `allowsAnsi()`, and `resetColor()`. `sendByteRaw` and `flush` are **pure virtual**; only `allowsAnsi()` (default `false`) and `resetColor()` (default no-op) have defaults.
- `prt1.error(fmt, ...)` is **removed** as a special method. Errors are written with ordinary stream syntax against `streamError` (e.g. `prt1 << streamError << "..." << endLine`). printf-style output is a general capability (§6).

## 4. Target keywords

| Keyword | Type | Role |
|---|---|---|
| `streamLog` | `Stream` | General log |
| `streamMessage` | `Stream` | Message / production link |
| `streamError` | `Stream` | Errors |
| `file0` … `file8` | `FileStream` | Nine named file sinks (extensible later) |

- All are globals with static storage duration. `streamLog` / `streamMessage` / `streamError` / `fileN` are defined in common code; platform translation units bind their inner sinks via `printerInit()`.
- **Use before `printerInit()` is safe.** Every `Stream::pInner` (and the MC `FileStream` backing pointer) defaults to a common `NullStream nullSink`, which drops bytes. The constructors are `constexpr`, so the default is set during constant initialization and holds even for prints from other static constructors or early boot. Output written before `printerInit()` is lost, not a crash.
- `prt1 << streamLog` sets `pStream = &streamLog` and leaves it there until the next switch. Same for `fileN`.
- **Stream name is a unique type of operator:** each target is a distinct global object, so overload resolution distinguishes it from `const char*`/integers without a tag type. Aliases are trivial (one more global of the same type).
- The old `KeyWords::File{"name"}` / `ClearFile{"name"}` constructor form is **deprecated and scheduled for refactoring** under the new `file0..file8` model (§13.1). The replacement is the plain field/method form on the `FileStream` itself: `fileN.name = "path"` or `fileN.open("path")` (§8).
- **`StreamGuard` and `hold` are both provided.** `StreamGuard` is the RAII scope for multi-line switches; `hold` is the chain-local keyword released by `endLine` (§5, §13.8). The difference `hold` makes over plain persistence still needs an example (§13.18).

## 5. Color (lazy)

- **Emission is lazy:** on the first byte written while the stream is **idle** (no colour active), `Stream::sendByteRaw` emits the active colour. A stream is idle after construction, after a target switch away from it, and after `endLine` — so the next line on the same stream re-emits its colour too. When `endLine` is streamed, the printer calls `pStream->resetColor()`, which emits `ESC[0m` if a colour was active and marks the stream idle.
- **Mid-line switch:** `prt1 << streamLog << "a" << streamError << "b" << endLine` calls `resetColor()` on the outgoing stream before re-pointing `pStream`, so no escape leaks across a switch. Then `streamError` emits its own colour before `"b"`.
- **`flushDma` never alters colour state.** The operator flushes the DMA unconditionally, wherever it appears. The terminal's colour is unaffected by a flush, so the next byte must **not** re-emit the colour. Only `endLine` (and a mid-line target switch) resets colour.
- **Colour precedence.** A `Stream` emits colour only when all three hold:
  1. `cfg::platformHasColor` — compile-time platform capability (`false` on MC, `true` on PC);
  2. `cfg::useColorDefault` — compile-time project switch; `false` disables colour everywhere, even on PC;
  3. `pInner->allowsAnsi()` — runtime sink capability (`false` for `FileStream`, `UsartStream`, `NullStream`, and for a PC console where VT mode could not be enabled, §9).

  The per-stream `Color` (§9) is used only when all three are true.
- **Files never colour:** `fileN` are `FileStream` instances, not `Stream` wrappers, so they never see colour at all; a `Stream` whose `pInner` is a `FileStream` fails condition 3. This enforces "no coloured prints in files" structurally, not by convention.
- **MC never colour, at zero cost:** `Stream::sendByteRaw` wraps the colour path in `if constexpr (cfg::platformHasColor && cfg::useColorDefault)`. In `config_mc.h`, `platformHasColor = false`, so the colour code and the virtual `allowsAnsi()` call are discarded at compile time. On PC the `allowsAnsi()` check remains a runtime check.
- Only `Stream` overrides `resetColor()` meaningfully; every other `BaseStream` keeps the no-op default (§3).

## 6. printf-style output

Confirmed signature — **method on the stream** (Option B):

```cpp
streamError.printf("mask %d out of range", badMask);
file0.printf("t,pos\n");
streamLog.printf("frame %u received", n);
```

- A free-function format engine (`printFormatted(BaseStream&, const char*, va_list)`) does the work; `BaseStream::printf` forwards to it. Both `Stream` and `FileStream` expose it uniformly.
- Declared with `__attribute__((format(printf, 2, 3)))` (argument 1 is the implicit `this`), so GCC/Clang check format strings against arguments at compile time.
- Formatting: `%d %i %u %x %X %f %s %c %p %%`, `l`/`ll` length modifiers. `%f` precision still ignored in v1.
- No auto-newline and no automatic target — callers write `<< endLine` explicitly. The old "auto-newline on error" convenience is gone.
- `prt1.error(fmt, ...)` is removed; replaced by `streamError.printf(...)` or by stream-style writes to `streamError`.
- **Open:** a direct `stream.printf(...)` bypasses `prt1`, so its interaction with `endLine`, colour and `hold` is not yet defined (§13.15).

## 7. Buffering (MC)

Buffering is a core algorithm: **two buffers or no buffering**. No intermediate strategies.

- `UartSettings::bufferSize == 0` → **polled** transmission (wait for TXE, write `DR`). No DMA, no RAM cost.
- `UartSettings::bufferSize > 0` → **double-buffered DMA**: two banks of `bufferSize / 2` bytes. DMA drains the active bank; CPU writes into the inactive bank. On bank-full: wait up to the backpressure budget for DMA completion; if free, swap banks and start transmission of the full bank; if still busy, drop the byte (backpressure by loss, same philosophy as the old module, but the budget is computed from `bufferSize` and `cfg::cpuFrequency_MHz`).
- **Banks are sized per instance.** `UsartStream` is a class template on its buffer size: `UsartStream<cfg::usart2Settings.bufferSize> mcUsart2;`. For `N > 0` the instance holds `uint8_t banks[2][N / 2]`; the `UsartStream<0>` specialisation is polled and holds no bank array. A polled USART therefore really costs no RAM, and a DMA USART costs exactly its own `bufferSize`.
- `static_assert(N % 2 == 0)` — `bufferSize` must be even.
- `static_assert(N <= cfg::printerMaxDmaBuffer)` — `printerMaxDmaBuffer` is an **upper bound only**; it does not size any array. Values above it are a build error.
- The old `update_UsartX()` (non-blocking kick-out of a partially filled bank) is retained under the new name `updateUsartX()` (§13.12).
- **Open:** writes into the banks from an ISR or a second context (handoff P-13) are not yet specified (§13.22).

## 8. File naming

Each `fileN` has a public `const char* name` field and matching open/close methods:

```cpp
file0.name = "logs/run1.csv";   // simple field assignment
prt1 << file0 << "t,pos,vel" << endLine;   // implicit open? see §13.17

file0.open("logs/run1.csv");                      // assigns name, appends, emits BEGIN marker
file1.open("logs/run2.csv", OpenMode::truncate);  // assigns name, truncates, emits BEGIN marker
prt1 << file0 << "t,pos,vel" << endLine;
file0.close();                                    // flushes and emits END marker
```

- Assignment is legal only before first write or after `close()`. The pointed-to string must have static lifetime. Enforcement is open (§13.19).
- **Default name if unset:** `cfg::defaultFileName` (`"noname.log"`), declared in `config.h` and used on both platforms.
- **`open(name, mode = OpenMode::append)`**, with `enum class OpenMode { append, truncate }`. `truncate` replaces the old `ClearFile` semantics.
- **PC:** `open()` opens `std::ofstream(name)` in the requested mode. `close()` flushes and releases.
- **MC:** `open()` emits a BEGIN marker on the backing USART; `close()` emits an END marker. Bytes between markers are plain text. No actual file is created on the MC — the markers delimit text on the USART so a PC-side capture can be split back into files. Whether `OpenMode` is carried in the marker is part of §13.2.
- **Marker text is identical on both platforms** (§13.2). A single splitter parses either platform's captured byte stream and produces the same `{name → content}` map.
- **Open:** two files open at once on the MC share one USART and would interleave (§13.16).
- **Closing is command-driven, not process-exit-driven.** A `stop recording file` command (emitted by the Task Manager or the Recorder) closes the named file: on PC it flushes and releases the `std::ofstream`; on MC it emits the END marker on the backing USART. There is no `atexit`-style shutdown hook — the Printer module does not own program termination.

## 9. Configuration files

Three data-only headers, all in `namespace cfg`, plus one binding entry point. **No assignments at namespace scope** — all configuration is `constexpr` data consumed by `printerInit()` / `printerPlatformInit()`. Common code reaches the platform constants (`cfg::platformHasColor`, …) through one include name that the CMake configuration resolves to `config_mc.h` or `config_pc.h`; the exact mechanism is deferred with §11.

The sketches below fix names and types. The values are examples.

### `config.h` — common
- `constexpr` feature flags: `printHexPrefix`, `doubleScientific`, `doublePlusInExponent`, `useColorDefault` (see §5 for its precedence).
- `Color` is a 16-colour SGR foreground code, with named presets that match the request's names (`darkGray`, `darkGreen`, …). 24-bit RGB is not supported in v1 (§13.13).
- `UartSettings { baudrate, bufferSize, dataBits, parity, stopBits, flowControl }`. `flowControl` comes from the README's "hardware flow control" requirement.
- Default `UartSettings` constants used by both platforms.
- `printerMaxDmaBuffer` (upper bound, §7) and `defaultFileName` (§8).
- No references to platform object names (`USART2`, `console0`, …).

```cpp
#pragma once
#include <cstdint>

namespace cfg {

constexpr bool printHexPrefix       = true;
constexpr bool doubleScientific     = false;
constexpr bool doublePlusInExponent = false;
constexpr bool useColorDefault      = true;   // project switch, §5 precedence

struct Color { uint8_t sgr; };                // emitted as ESC[<sgr>m
constexpr Color black{30},    red{31},       darkGreen{32}, yellow{33},
                blue{34},     magenta{35},   cyan{36},      lightGray{37},
                darkGray{90}, brightRed{91}, green{92},     brightYellow{93},
                brightBlue{94}, brightMagenta{95}, brightCyan{96}, white{97};

enum class Parity      : uint8_t { none, even, odd };
enum class StopBits    : uint8_t { one, two };
enum class FlowControl : uint8_t { none, rtsCts };

struct UartSettings {
    uint32_t    baudrate;
    uint32_t    bufferSize;   // 0 = polled, no DMA (§7); must be even
    uint8_t     dataBits;
    Parity      parity;
    StopBits    stopBits;
    FlowControl flowControl;
};

constexpr UartSettings uartDefault{115200, 0, 8, Parity::none, StopBits::one, FlowControl::none};

constexpr uint32_t    printerMaxDmaBuffer = 4096;          // upper bound only, §7
constexpr const char* defaultFileName     = "noname.log";  // §8, both platforms

} // namespace cfg
```

### `config_mc.h` — STM32F103 (example)
- `constexpr bool platformHasColor = false`.
- Clock inputs from the boot layer (§10): `cpuFrequency_MHz` for the DMA backpressure budget **only**; `apb1Frequency_Hz` (USART2, USART3) and `apb2Frequency_Hz` (USART1) for BRR. On an F103 at 72 MHz, APB1 runs at 36 MHz, so computing BRR from the CPU clock would double every baud rate.
- **Peripheral base addresses as integers**, not CMSIS pointers. `USART2` expands to `((USART_TypeDef*)USART2_BASE)`, and a `reinterpret_cast` is not allowed in a constant expression, so `constexpr USART_TypeDef* p = USART2;` does not compile. The config stores `constexpr uintptr_t usart2Base = USART2_BASE;`, and `Hardware/` casts it at runtime.
- DMA channel numbers (F103: USART2_TX = DMA1 channel 7, USART3_TX = DMA1 channel 2).
- `usart2Settings` / `usart3Settings`. Per the request, USART3's `bufferSize` is **zero by default**, so it is polled with no DMA.
- Role → physical `UsartStream` mapping (`streamLogTarget`, `streamMessageTarget`, `streamErrorTarget`, `fileBackingUsart`), expressed as an enum that `printerInit()` resolves, so this header does not need the `UsartStream` declarations (§13.20).
- GPIO AF pin constants, pending §13.6.
- Declares `void printerPlatformInit();`

```cpp
#pragma once
#include "config.h"
#include "stm32f1xx.h"   // CMSIS: only the *_BASE integer macros are used here

namespace cfg {

constexpr bool platformHasColor = false;

constexpr uint32_t cpuFrequency_MHz = 72;           // backpressure budget only
constexpr uint32_t apb1Frequency_Hz = 36'000'000;   // USART2, USART3 BRR
constexpr uint32_t apb2Frequency_Hz = 72'000'000;   // USART1 BRR

constexpr uintptr_t usart2Base = USART2_BASE;       // cast in Hardware/
constexpr uintptr_t usart3Base = USART3_BASE;
constexpr uintptr_t dma1Base   = DMA1_BASE;
constexpr uint8_t   usart2TxDmaChannel = 7;
constexpr uint8_t   usart3TxDmaChannel = 2;

// GPIO AF (pending §13.6)
constexpr uintptr_t usart2TxPortBase = GPIOA_BASE;  constexpr uint8_t usart2TxPin = 2;   // PA2
constexpr uintptr_t usart3TxPortBase = GPIOB_BASE;  constexpr uint8_t usart3TxPin = 10;  // PB10

constexpr UartSettings usart2Settings{115200, 2048, 8, Parity::none, StopBits::one, FlowControl::none};
constexpr UartSettings usart3Settings{230400, 0,    8, Parity::none, StopBits::one, FlowControl::none};
                                      // bufferSize 0 by default: polled, no DMA

enum class McSink : uint8_t { usart2, usart3 };     // proposal, §13.20
constexpr McSink streamLogTarget     = McSink::usart2;
constexpr McSink streamMessageTarget = McSink::usart2;
constexpr McSink streamErrorTarget   = McSink::usart3;
constexpr McSink fileBackingUsart    = McSink::usart3;

} // namespace cfg

void printerPlatformInit();
```

### `config_pc.h` — PC
- `constexpr bool platformHasColor = true`. Colour still needs VT mode at runtime (see `printerInit()` step 3).
- Role → endpoint mapping. A role can target `console0` or a `fileN`; the request's example sends `streamError` to `file1`.
- Per-`Stream` `Color` values, using the presets from `config.h`.
- Declares `void printerPlatformInit();`
- Not decided: whether the PC config also emulates the MC registers with static variables, as the request suggests (§13.21).

```cpp
#pragma once
#include "config.h"

namespace cfg {

constexpr bool platformHasColor = true;

enum class PcSink : uint8_t { console0, file0, file1, file2, file3, file4, file5, file6, file7, file8 };
constexpr PcSink streamLogTarget     = PcSink::console0;
constexpr PcSink streamMessageTarget = PcSink::console0;
constexpr PcSink streamErrorTarget   = PcSink::file1;   // colour skipped: FileStream

constexpr Color streamLogColor     = darkGray;
constexpr Color streamMessageColor = darkGreen;
constexpr Color streamErrorColor   = brightRed;

} // namespace cfg

void printerPlatformInit();
```

### `printerInit()` — binding
Declared in common, **defined per platform** (`Hardware/printerInit_mc.cpp` / `Hardware/printerInit_pc.cpp`), selected by the CMake configuration. Until it runs, every stream writes to `nullSink` (§4). Responsibilities:

1. Bind `streamLog.pInner`, `streamMessage.pInner`, `streamError.pInner` by resolving the role→target enums to objects (`McSink::usart2` → `&mcUsart2`, `PcSink::file1` → `&file1`, …). On PC, also set each stream's colour from the `cfg::*Color` constants.
2. **MC:** point each `fileN`'s backing `UsartStream` at the object named by `cfg::fileBackingUsart`. **PC:** nothing to bind. `fileN` is already a `FileStream` and opens its own `std::ofstream` on `open()`.
3. Call `printerPlatformInit()`:
   - **MC:** programs USART CR1/CR2/CR3 from `UartSettings` (including RTS/CTS when `flowControl == rtsCts`). Programs BRR as `(apbN + baud / 2) / baud`, where `apbN` is `apb1Frequency_Hz` for USART2/3 and `apb2Frequency_Hz` for USART1. Programs the DMA channel when `bufferSize > 0`, and the GPIO AF (pending §13.6).
   - **PC:** enables ANSI processing on the console: `GetConsoleMode` on `STD_OUTPUT_HANDLE`, then `SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)`. Windows 10 shows raw escape bytes without this. If either call fails (old Windows, output redirected to a file or pipe), `console0.allowsAnsi()` returns `false`, and colour is skipped by §5 precedence rule 3.

## 10. Hardware directory (MC)

- `Hardware/` holds `printerPlatformInit()` and the platform-specific `UsartStream` implementation (USART CR/BRR programming, DMA channel setup, GPIO alternate-function setup). It is the only place where the `cfg::*Base` integers are cast to CMSIS pointer types (`reinterpret_cast<USART_TypeDef*>(cfg::usart2Base)`).
- **Clock configuration is not part of the Printer module.** A future standalone MC boot layer owns RCC/PLL. The Printer receives the resulting frequencies as constants: `cfg::cpuFrequency_MHz`, used only for the DMA backpressure budget, and `cfg::apb1Frequency_Hz` / `cfg::apb2Frequency_Hz`, used only for BRR.
- GPIO AF setup ownership: **currently owned by `printerPlatformInit()`** in `Hardware/` (see §13.6 for whether to keep or hand off to the host boot layer).
- On PC, `Hardware/` holds only `printerInit_pc.cpp` (binding and VT-mode setup). No register code is present unless §13.21 decides otherwise. CMake links only the PC translation units.

## 11. Build system

- CMake, one configuration per platform (MC / PC), chosen at configure time — same approach as the old project.
- `mc_*` / `pc_*` translation units are selected per configuration; polymorphism is link-time (abstract `BaseStream` in common, concrete implementations in `mc_` or `pc_` files).
- Exact target layout is deferred; the old project's scheme will be reused. Requirement: `Printer` is a self-contained CMake target that links into a new project without pulling in the old `Common/` tree.

## 12. Testing

- Tests target the new API only. The old `set_sendByte_*_override` hooks are **not** carried over; the new project will have a different test fracture.
- Recommended hook: a `CaptureStream : BaseStream` that records raw bytes, installed by tests either as a `pInner` on a `Stream` or directly as `prt1.pStream`. No test hook lives inside production `UsartStream` / `ConsoleStream`.
- Exact framework and file layout TBD (follow the new project's conventions).

## 13. Decisions and open items

Item numbers are stable: a decided item keeps its number, so references such as §13.8 stay valid.

### Decisions

| ID | Decision | Owner wording / date |
|---|---|---|
| 13.3 | **Shutdown marker is command-driven.** A `stop recording file` command from the Task Manager / Recorder closes the file (flush on PC, END marker on MC). The Printer has no `printerDeinit()` / `atexit` responsibility. Implementation-time sub-item: a missing END marker (crash or reset before the stop command) is treated by the splitter as truncated / abnormal and recorded in its report (proposal). | Recorded as resolved in the 2026‑10‑23 draft; owner wording not captured verbatim |
| 13.4 | **printf signature:** `stream.printf(fmt, ...)` (Option B). | Recorded as confirmed, 2026‑10‑23 |
| 13.5 | **Naming convention:** types `FooBar`, instances `fooBar`, macros UPPERCASE. `STR_LOG` → `streamLog`, `STR_MSG` → `streamMessage`, `STR_ERR` → `streamError`. | Recorded as confirmed, 2026‑10‑23 |
| 13.7 | **Default `fileN.name`:** `"noname.log"` on MC, and on PC for consistency (`cfg::defaultFileName`). | Recorded as confirmed, 2026‑10‑23 |
| 13.8 | **`StreamGuard` and `hold`: both.** `StreamGuard` is RAII for multi-line scopes. `hold` saves `pStream` into a single slot and is released by `endLine` (single level, no nesting). Only `endLine` releases a hold; `flushDma` and `printf` do not. A `StreamGuard` on top of an active hold is allowed and scoped independently. | Recorded as confirmed, 2026‑10‑23 |
| 13.9 | **`flushDma`:** flushes DMA unconditionally, wherever it appears. Colour state is not changed; the next byte does not re-emit. | Recorded as confirmed, 2026‑10‑23 |
| 13.10 | **Glossary migration** applied throughout (`streamLog` / `streamMessage` / `streamError`; `file0..file8`; `BaseStream`; `pStream`; `mcUsartN`). | Recorded as confirmed, 2026‑10‑23 |

### Open items

- **13.1** **`KeyWords::File{"name"}` / `ClearFile{"name"}` refactoring.** Decided: the form needs refactoring. Proposal to confirm: drop the constructor form entirely and rely on `fileN.name = "..."`, `fileN.open("...", OpenMode::append|truncate)` and `fileN.close()`. The constructor form was only a way to redirect the single old `file` target. With nine explicit `fileN` keywords it is redundant, and `OpenMode::truncate` takes over `ClearFile`.
- **13.2** **Marker text and trigger point.**
   - *Requirement (confirmed):* the marker string is identical on MC and PC, so one splitter produces the same `{name → content}` map from either platform's captured bytes. Owner wording: "parsed string equal for both platforms".
   - *Proposal:* `=== BEGIN <name> ===` / `=== END <name> ===`, where `<name>` is exactly `fileN.name`. On MC, markers go to the backing USART. On PC, the same markers are written **into the file itself** so a splitter sees identical bytes on both platforms.
   - *Trigger question, restated concretely:* if `file0.name = "run.csv"` and then `file0.close()` is called **without any bytes written**, should the output contain `=== BEGIN run.csv ===` + `=== END run.csv ===` (an empty file with markers present), or nothing at all (the file was never opened)? Proposal: markers are emitted at `open()`, so an empty opened file still yields a marker pair. The "name assigned, no `open()`" case is §13.17. Confirm, or specify "markers only on first byte".
   - *Sub-question:* does the PC file contain the markers, or are they stripped before writing? (Proposal: kept, for platform parity.)
   - *Sub-question:* on MC, should `OpenMode` be carried in the BEGIN marker (e.g. `=== BEGIN <name> truncate ===`) so the splitter can append or replace? Proposal: no. Each marker pair produces one fresh file, and `OpenMode` affects PC only.
- **13.6** **GPIO AF setup.** §9 currently assigns GPIO AF to `printerPlatformInit()` in `Hardware/`. If you prefer the host boot layer to own it, move the pin/AF constants out of `config_mc.h` and remove the GPIO AF part from `printerPlatformInit()`. The USART and DMA programming stays. **Confirm or override.**
- **13.11** **`printerPlatformInit()` naming.** `hardwareInit()` from earlier drafts is renamed `printerPlatformInit()` to match the camelCase convention and to pair with `printerInit()`. Confirm.
- **13.12** **`updateUsartX()` naming.** Old `update_UsartX()` renamed `updateUsartX()` per camelCase. Confirm.
- **13.13** **Colour model.** Applied in this draft: `Color` is a 16-colour SGR code with named presets (`darkGray` = 90, `darkGreen` = 32, …) that match the request's names. The alternative is `Color { r, g, b }`, emitted as 24-bit `ESC[38;2;r;g;bm`, which the request's palette names don't need and older terminals don't support. Confirm 16-colour, or ask for RGB, either instead or in addition.
- **13.14** **Unit-suffix naming exception** (§2). `cpuFrequency_MHz` breaks the plain camelCase rule. Applied in this draft: accept `_Unit` suffixes as an explicit exception. The alternative is to rename to `cpuFrequencyMhz`, `apb1FrequencyHz`, and so on. Confirm.
- **13.15** **Direct `stream.printf` vs `prt1` state.** `streamError.printf(...)` followed by `prt1 << endLine` resets colour on `prt1.pStream`, not on `streamError`, and it writes the newline to that sink as well. The interaction with `endLine`, colour and `hold` needs a rule. Options: (a) give `BaseStream` its own `endLine()` and colour lifecycle, so direct writes are self-contained; (b) route `printf` through `prt1` (`prt1 << streamError; prt1.printf(...)`); (c) document that direct `printf` is colour-idle until the next `prt1` switch to that stream. Decide.
- **13.16** **Several files open at once on the MC.** If `file0` and `file1` are both open, both forward to `fileBackingUsart`, their bytes interleave between markers, and the splitter cannot attribute them. Options: (a) only one `fileN` open at a time on the MC (debug assert on a second `open()`); (b) tag each chunk with a short marker (e.g. `=== FILE <name> ===` on every target switch). Decide.
- **13.17** **Writing without `open()`.** The first §8 example assigns `name` and writes without `open()`. Is there an implicit `open(name, OpenMode::append)` on first write? Or is a write to an unopened `fileN` dropped (or asserted)? Pick one rule. It decides the trigger question in §13.2.
- **13.18** **`hold` semantics by example.** Targets already persist until the next switch (§4), so a before/after example is needed to show what `hold` adds. A candidate reading is "restore the previous `pStream` at `endLine`": `prt1 << streamLog << "a" << endLine << hold << streamError << "b" << endLine << "c"`, where `"c"` goes to `streamLog`. Confirm or replace.
- **13.19** **`fileN.name` lifetime and enforcement.** A raw `const char*` must point to static-lifetime storage, and "legal only before first write / after `close()`" is not enforced today. Proposal: make `name` private behind `setName(const char*)` with a debug assert on misuse. The alternative is to keep the public field with a documented contract only.
- **13.20** **Role → target mapping form.** A `constexpr` pointer to `mcUsart2` would require `config_mc.h` to include the `UsartStream` declarations, which risks an include cycle (`UsartStream` needs `cfg::usart2Settings`). This draft shows enums (`McSink`, `PcSink`) that `printerInit()` resolves. Confirm, or choose an index mapping or another form.
- **13.21** **PC register emulation.** The request says the PC config assigns "Registers pointers … to static variables (including GPIO)". This draft does not do that: PC uses `ConsoleStream` / `FileStream`, and `Hardware/` has no register code on PC. Accept the omission, or require emulation (for example, to run `UsartStream` logic on the host in tests).
- **13.22** **Other unspecified behaviour.**
    - `flushDma` on PC: a no-op, or `std::cout.flush()` / `ofstream::flush()`?
    - `UartSettings` on PC: unused, rejected by a `static_assert`, or used by a future PC serial-port sink?
    - More MC platforms (the request's TODO): one `config_<chip>.h` per chip, selected by CMake?
    - ISR or concurrent writes into the DMA banks (handoff P-13): forbidden, guarded by a critical section, or given a separate ISR-safe path?
