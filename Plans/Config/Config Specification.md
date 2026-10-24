# Printer Configuration Specification (Draft)
**Status:** Draft. Companion to [`Config Request.md`](Config%20Request.md) in this folder.  
**Date:** 2026‑10‑23 (revised 2026‑10‑23 after review; 2026‑10‑24: Hardware, chip files, MC boot; CPU boot out of scope; keywords, .hpp, bare metal, constructor boot, clock check)

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
- **Instances, members, functions, `constexpr` values:** `camelCase` — `prt1`, `info`, `message`, `error`, `file0` … `file8`, `pStream`, `pInner`, `hasColor`, `printerInit`, `printerPlatformInit`, `printHexPrefix`, `platformHasColor`.
- **Unit-suffix exception:** a `constexpr` holding a physical quantity may end in `_Unit` — `cpuFrequency_MHz`, `apb1Frequency_Hz`, `apb2Frequency_Hz`. The suffix keeps the unit visible at every use site (§13.14).
- **Configuration constants** live inside `namespace cfg`. The namespace is `cfg`, not `Config`.
- **Preprocessor macros** (only when a macro is genuinely required): `SCREAMING_SNAKE_CASE` — e.g. `PLATFORM_HAS_COLOR` if a `#if` switch is needed. Preferred form is `constexpr bool` so `#if` is avoided; if a macro is used, it is UPPERCASE.
- **Headers are `.hpp`** throughout (§13.29). Every project header is C++ only (`namespace`, `enum class`, `constexpr`), and the extension says so in a C-heavy MC ecosystem. Each header starts with `#pragma once` followed by `#ifndef __cplusplus` / `#error "C++ header"` / `#endif`, so a C file that includes one fails with a clear message.
- **Chip files** are named after the ST device-line designation, with a platform prefix: `mc_STM32F103xE.hpp`, `pc_STM32F103xE.hpp`, and the shared layout `STM32F103xE_regs.hpp` (§10). The designation names the register map, which a whole line shares; the full part number (`STM32F103VET6`) adds only package and flash size.
- **Hardware register names** follow the reference manual (RM0008 for the F103): peripherals `USART2`, `DMA1`, `GPIOA`, `RCC`; registers `CR1`, `BRR`, `CFGR`; bits `TXE`, `TC`. The project is **bare metal** (§13.30): it uses no vendor or CMSIS headers, and CMSIS appears in this document only as an illustration. The `UsartStream` instances are `mcUsart2` / `mcUsart3`, never `USART2`, so they don't clash with the register-block names.

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
  Common keywords:   info, message, error (Stream)
                     file0..file8 (FileStream)
```

- `Printer::pStream` is `BaseStream*`. Both `Stream` (with colour) and `FileStream` (without) are accepted by `operator<<`.
- `Stream` owns the colour logic. It wraps a `BaseStream* pInner`. `Stream::sendByteRaw(b)` emits an ANSI colour sequence on transition, then forwards `b` to `pInner`. When colour is disabled (§5, precedence) the bytes go straight through.
- `FileStream` is a `BaseStream` in its own right. It never carries colour. On MC it forwards text to a `UsartStream` and emits begin/end markers; on PC it writes to its own `std::ofstream`. A `Stream` may also use a `fileN` as its `pInner` (e.g. `error` → `file1` on PC, §9); colour is then skipped because `FileStream::allowsAnsi()` is `false`.
- `BaseStream` exposes `printf(fmt, ...)` (§6), `sendByteRaw`, `flush`, `allowsAnsi()`, and `resetColor()`. `sendByteRaw` and `flush` are **pure virtual**; only `allowsAnsi()` (default `false`) and `resetColor()` (default no-op) have defaults.
- `prt1.error(fmt, ...)` is **removed** as a special method. Errors are written with ordinary stream syntax against `error` (e.g. `prt1 << error << "..." << endLine`). printf-style output is a general capability (§6).

## 4. Target keywords

| Keyword | Type | Role |
|---|---|---|
| `info` | `Stream` | General log (was `streamLog`) |
| `message` | `Stream` | Message / production link (was `streamMessage`) |
| `error` | `Stream` | Errors (was `streamError`) |
| `file0` … `file8` | `FileStream` | Nine named file sinks (extensible later) |

- **Keyword names are short and must not clash with the C library** (§13.5). Keywords are global names, so `log` is ruled out (it hides `::log()` from `<math.h>`), and so is `err` (BSD `err()`). `info`, `message` and `error` are safe. A project can add its own alias as one more global of the same type.
- All are globals with static storage duration. `info` / `message` / `error` / `fileN` are defined in common code; platform translation units bind their inner sinks via `printerInit()`.
- **Use before boot is safe for the streams.** Every `Stream::pInner` (and the MC `FileStream` backing pointer) defaults to a common `NullStream nullSink`, which drops bytes. The constructors are `constexpr`, so the default is set during constant initialization and holds even for prints from other static constructors or early boot. Output written before `printerInit()` is lost, not a crash.
- **`prt1` itself is dynamically initialized,** because its constructor boots the hardware (§9) and therefore can't be `constexpr`. Its static storage is zero-filled first, so `pStream` reads as `nullptr` until the constructor has run, and `operator<<` treats a `nullptr` `pStream` like `nullSink`. This is a practical guard only: formally, using `prt1` from another global's constructor before its own has run is undefined behaviour, so the rule stays "don't print through `prt1` from static constructors".
- `prt1 << info` sets `pStream = &info` and leaves it there until the next switch. Same for `fileN`.
- **Stream name is a unique type of operator:** each target is a distinct global object, so overload resolution distinguishes it from `const char*`/integers without a tag type. Aliases are trivial (one more global of the same type).
- The old `KeyWords::File{"name"}` / `ClearFile{"name"}` constructor form is **deprecated and scheduled for refactoring** under the new `file0..file8` model (§13.1). The replacement is the plain field/method form on the `FileStream` itself: `fileN.name = "path"` or `fileN.open("path")` (§8).
- **`StreamGuard` and `hold` are both provided.** `StreamGuard` is the RAII scope for multi-line switches; `hold` is the chain-local keyword released by `endLine` (§5, §13.8). The difference `hold` makes over plain persistence still needs an example (§13.18).

## 5. Color (lazy)

- **Emission is lazy:** on the first byte written while the stream is **idle** (no colour active), `Stream::sendByteRaw` emits the active colour. A stream is idle after construction, after a target switch away from it, and after `endLine` — so the next line on the same stream re-emits its colour too. When `endLine` is streamed, the printer calls `pStream->resetColor()`, which emits `ESC[0m` if a colour was active and marks the stream idle.
- **Mid-line switch:** `prt1 << info << "a" << error << "b" << endLine` calls `resetColor()` on the outgoing stream before re-pointing `pStream`, so no escape leaks across a switch. Then `error` emits its own colour before `"b"`.
- **`flushDma` never alters colour state.** The operator flushes the DMA unconditionally, wherever it appears. The terminal's colour is unaffected by a flush, so the next byte must **not** re-emit the colour. Only `endLine` (and a mid-line target switch) resets colour.
- **Colour precedence.** A `Stream` emits colour only when all three hold:
  1. `cfg::platformHasColor` — compile-time platform capability (`false` on MC, `true` on PC);
  2. `cfg::useColorDefault` — compile-time project switch; `false` disables colour everywhere, even on PC;
  3. `pInner->allowsAnsi()` — runtime sink capability (`false` for `FileStream`, `UsartStream`, `NullStream`, and for a PC console where VT mode could not be enabled, §9).

  The per-stream `Color` (§9) is used only when all three are true.
- **Files never colour:** `fileN` are `FileStream` instances, not `Stream` wrappers, so they never see colour at all; a `Stream` whose `pInner` is a `FileStream` fails condition 3. This enforces "no coloured prints in files" structurally, not by convention.
- **MC never colour, at zero cost:** `Stream::sendByteRaw` wraps the colour path in `if constexpr (cfg::platformHasColor && cfg::useColorDefault)`. In `config_mc.hpp`, `platformHasColor = false`, so the colour code and the virtual `allowsAnsi()` call are discarded at compile time. On PC the `allowsAnsi()` check remains a runtime check.
- Only `Stream` overrides `resetColor()` meaningfully; every other `BaseStream` keeps the no-op default (§3).

## 6. printf-style output

Confirmed signature — **method on the stream** (Option B):

```cpp
error.printf("mask %d out of range", badMask);
file0.printf("t,pos\n");
info.printf("frame %u received", n);
```

- A free-function format engine (`printFormatted(BaseStream&, const char*, va_list)`) does the work; `BaseStream::printf` forwards to it. Both `Stream` and `FileStream` expose it uniformly.
- Declared with `__attribute__((format(printf, 2, 3)))` (argument 1 is the implicit `this`), so GCC/Clang check format strings against arguments at compile time.
- Formatting: `%d %i %u %x %X %f %s %c %p %%`, `l`/`ll` length modifiers. `%f` precision still ignored in v1.
- No auto-newline and no automatic target — callers write `<< endLine` explicitly. The old "auto-newline on error" convenience is gone.
- `prt1.error(fmt, ...)` is removed; replaced by `error.printf(...)` or by stream-style writes to `error`.
- **Open:** a direct `stream.printf(...)` bypasses `prt1`, so its interaction with `endLine`, colour and `hold` is not yet defined (§13.15).

## 7. Buffering (MC)

Buffering is a core algorithm: **two buffers or no buffering**. No intermediate strategies.

**`UartSettings::bankSize` is the size of one bank** (§13.32). **RAM = 2 × `bankSize`; 0 = polled, no DMA.**

- `UartSettings::bankSize == 0` → **polled** transmission (wait for TXE, write `DR`). No DMA, no RAM cost.
- `UartSettings::bankSize > 0` → **double-buffered DMA**: two banks of `bankSize` bytes each. DMA drains the active bank; CPU writes into the inactive bank. On bank-full: wait up to the backpressure budget for DMA completion; if free, swap banks and start transmission of the full bank; if still busy, drop the byte (backpressure by loss, same philosophy as the old module, but the budget is computed from `bankSize` and `cfg::cpuFrequency_MHz`).
- There is no even-size rule. `bankSize` is also the longest burst the CPU can write before a swap, and the length of one DMA transfer (CNDTR).
- **Minimum bank size: 4** (`cfg::printerMinBankSize`, §13.33). Formally, any `bankSize ≥ 1` should work, and even a 1-byte bank can speed up printing to the UART. But the algorithm taken from the previous project describes the buffer state as "head", "body" and "tail", and it is not yet known to handle banks of 1–3 bytes correctly. With at least 4 elements, those three parts can't collapse onto each other, so the comparisons stay simple. Lowering the minimum to 1 is a promise kept open until edge-case tests prove it (§13.34).
- **Banks are sized per instance.** `UsartStream` is a class template on its bank size: `UsartStream<cfg::usart2Settings.bankSize> mcUsart2;`. For `N > 0` the instance holds `uint8_t banks[2][N]`; the `UsartStream<0>` specialisation is polled and holds no bank array. A polled USART therefore really costs no RAM, and a DMA USART costs exactly `2 × bankSize`.
- `static_assert(N == 0 || N >= cfg::printerMinBankSize)` — the minimum above.
- `static_assert(N <= cfg::printerMaxBankSize)` — `printerMaxBankSize` is an **upper bound only**; it does not size any array. Values above it are a build error. It must itself stay ≤ 65535, because CNDTR is a 16-bit register.
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
- **Default name if unset:** `cfg::defaultFileName` (`"noname.log"`), declared in `config.hpp` and used on both platforms.
- **`open(name, mode = OpenMode::append)`**, with `enum class OpenMode { append, truncate }`. `truncate` replaces the old `ClearFile` semantics.
- **PC:** `open()` opens `std::ofstream(name)` in the requested mode. `close()` flushes and releases.
- **MC:** `open()` emits a BEGIN marker on the backing USART; `close()` emits an END marker. Bytes between markers are plain text. No actual file is created on the MC — the markers delimit text on the USART so a PC-side capture can be split back into files. Whether `OpenMode` is carried in the marker is part of §13.2.
- **Marker text is identical on both platforms** (§13.2). A single splitter parses either platform's captured byte stream and produces the same `{name → content}` map.
- **Open:** two files open at once on the MC share one USART and would interleave (§13.16).
- **Closing is command-driven, not process-exit-driven.** A `stop recording file` command (emitted by the Task Manager or the Recorder) closes the named file: on PC it flushes and releases the `std::ofstream`; on MC it emits the END marker on the backing USART. There is no `atexit`-style shutdown hook — the Printer module does not own program termination.

## 9. Configuration files

Three data-only headers, all in `namespace cfg`, plus one binding entry point. **No assignments at namespace scope** — all configuration is `constexpr` data consumed by `printerInit()` / `printerPlatformInit()`. Common code reaches the platform constants (`cfg::platformHasColor`, …) through one include name that the CMake configuration resolves to `config_mc.hpp` or `config_pc.hpp`; the exact mechanism is deferred with §11.

The sketches below fix names and types. The values are examples.

### `config.hpp` — common
- `constexpr` feature flags: `printHexPrefix`, `doubleScientific`, `doublePlusInExponent`, `useColorDefault` (see §5 for its precedence).
- `Color` is a 16-colour SGR foreground code, with named presets that match the request's names (`darkGray`, `darkGreen`, …). 24-bit RGB is not supported in v1 (§13.13).
- `UartSettings { baudrate, bankSize, dataBits, parity, stopBits, flowControl }`. `flowControl` comes from the README's "hardware flow control" requirement.
- Default `UartSettings` constants used by both platforms.
- `printerMinBankSize` and `printerMaxBankSize` (bounds, §7), and `defaultFileName` (§8).
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
    uint32_t    bankSize;     // one bank; RAM = 2 x bankSize; 0 = polled, no DMA (§7)
    uint8_t     dataBits;
    Parity      parity;
    StopBits    stopBits;
    FlowControl flowControl;
};

constexpr UartSettings uartDefault{115200, 0, 8, Parity::none, StopBits::one, FlowControl::none};

constexpr uint32_t    printerMinBankSize  = 4;             // §7, §13.33; 1-3 not yet proven
constexpr uint32_t    printerMaxBankSize  = 4096;          // upper bound only, §7; <= 65535 (CNDTR)
constexpr const char* defaultFileName     = "noname.log";  // §8, both platforms

} // namespace cfg
```

### `config_mc.hpp` — STM32F103 (example)
- **Choices only, no silicon facts.** Register addresses, DMA channel mapping, pin tables and clock limits are facts about the chip, and they live in the chip headers in `Hardware/` (§10). `config_mc.hpp` therefore includes no vendor header and compiles on any host.
- `constexpr bool platformHasColor = false`.
- **Clocks — the clock in effect, not a clock the Printer sets (§10, §13.23).** The Printer does not boot the CPU. `hseFrequency_Hz` records the board crystal for an optional CPU boot. The PLL and divider values are part of that CPU boot, so they are commented out here. `sysclkFrequency_Hz` is the clock running when the Printer boots (in `prt1`'s constructor): without a CPU boot, an F103 runs on HSI = 8 MHz with all dividers at 1. The rest of the Printer uses frequencies **derived** from it: `cpuFrequency_MHz` for the DMA backpressure budget **only**, and `apb1Frequency_Hz` (USART2, USART3) / `apb2Frequency_Hz` (USART1) for BRR. Once a CPU boot runs the F103 at 72 MHz, APB1 runs at 36 MHz, so computing BRR from the CPU clock would double every baud rate. The chip header `static_assert`s these values against the silicon limits. Keeping them in step with a CPU boot is §13.27.
- GPIO remap choice per USART (`usart2Remap`, `usart3Remap`). The pins each remap selects come from the chip header.
- `usart2Settings` / `usart3Settings`. Per the request, USART3's `bankSize` is **zero by default**, so it is polled with no DMA. USART2's example `bankSize` of 2048 costs 2 × 2048 = 4 KB of RAM (64 KB on the F103VET6).
- Role → physical `UsartStream` mapping (`infoTarget`, `messageTarget`, `errorTarget`, `fileBackingUsart`), expressed as an enum that `printerInit()` resolves, so this header does not need the `UsartStream` declarations (§13.20).

```cpp
#pragma once
#include "config.hpp"   // no vendor headers: silicon facts live in Hardware/mc_<chip>.hpp (§10)

namespace cfg {

constexpr bool platformHasColor = false;

// Clocks. The Printer does not boot the CPU (§10, §13.23); it runs on the clock in effect.
constexpr uint32_t hseFrequency_Hz = 8'000'000;   // board crystal, used by an optional CPU boot

// Optional CPU boot, not part of the Printer:
// constexpr uint32_t pllMultiplier = 9;          // SYSCLK = HSE x 9 = 72 MHz
// constexpr uint32_t ahbDivider    = 1;
// constexpr uint32_t apb1Divider   = 2;          // APB1 = 36 MHz
// constexpr uint32_t apb2Divider   = 1;

// Clock in effect when prt1 boots. Without a CPU boot: HSI = 8 MHz, all dividers 1 (§13.27).
constexpr uint32_t sysclkFrequency_Hz = 8'000'000;                      // HSI after reset
constexpr uint32_t cpuFrequency_MHz   = sysclkFrequency_Hz / 1'000'000;  // backpressure budget only
constexpr uint32_t apb1Frequency_Hz   = sysclkFrequency_Hz;              // USART2, USART3 BRR
constexpr uint32_t apb2Frequency_Hz   = sysclkFrequency_Hz;              // USART1 BRR

enum class UsartRemap : uint8_t { none, partial, full };   // pins per remap: chip header
constexpr UsartRemap usart2Remap = UsartRemap::none;       // F103: PA2/PA3
constexpr UsartRemap usart3Remap = UsartRemap::none;       // F103: PB10/PB11

constexpr UartSettings usart2Settings{115200, 2048, 8, Parity::none, StopBits::one, FlowControl::none};
constexpr UartSettings usart3Settings{230400, 0,    8, Parity::none, StopBits::one, FlowControl::none};
                                      // usart2: 2 x 2048 = 4 KB RAM; usart3: bankSize 0 by default, polled

enum class McSink : uint8_t { usart2, usart3 };     // proposal, §13.20
constexpr McSink infoTarget     = McSink::usart2;
constexpr McSink messageTarget = McSink::usart2;
constexpr McSink errorTarget   = McSink::usart3;
constexpr McSink fileBackingUsart    = McSink::usart3;

} // namespace cfg
```

### `config_pc.hpp` — PC
- `constexpr bool platformHasColor = true`. Colour still needs VT mode at runtime (see the PC boot below).
- Role → endpoint mapping. A role can target `console0` or a `fileN`; the request's example sends `error` to `file1`.
- Per-`Stream` `Color` values, using the presets from `config.hpp`.
- The emulated chip is chosen in `Hardware/pc_chip.hpp`, not here (§10, §13.24).

```cpp
#pragma once
#include "config.hpp"

namespace cfg {

constexpr bool platformHasColor = true;

enum class PcSink : uint8_t { console0, file0, file1, file2, file3, file4, file5, file6, file7, file8 };
constexpr PcSink infoTarget     = PcSink::console0;
constexpr PcSink messageTarget = PcSink::console0;
constexpr PcSink errorTarget   = PcSink::file1;   // colour skipped: FileStream

constexpr Color infoColor     = darkGray;
constexpr Color messageColor = darkGreen;
constexpr Color errorColor   = brightRed;

} // namespace cfg
```

### Boot and binding
The user's program needs one include and one definition (§13.28):

```cpp
#include "Printer/Printer.hpp"   // umbrella header: Printer, keywords, config

Printer prt1 = {};               // the constructor boots the Printer

int main() {
    prt1 << message << "Hello Printer 2!" << endLine;
}
```

- `Printer/Printer.hpp` declares `extern Printer prt1;`, and the user's program defines it. Library code can print through `prt1`, and a project that forgets the definition gets a link error, not a silent failure.
- **The constructor calls `boot()`.** `boot()` is public and **idempotent**: a static flag makes the second and later calls no-ops, so a second `Printer` instance or an explicit `prt1.boot()` never re-programs the hardware.
- A constructor can't report failure (no exceptions on the MC). A boot failure, such as a failed clock check (below), is handled by the Printer's assert.

`Hardware/PrinterInit.hpp` declares two functions, each **defined per platform** in `Hardware/mc_PrinterInit.cpp` / `Hardware/pc_PrinterInit.cpp` and selected by the CMake configuration. `boot()` runs them in this order (§13.25):

1. **`printerPlatformInit()` — hardware boot.** It runs first, so no byte reaches an interface that is not yet running. The MC sequence is in §10. On the MC it starts with the clock check (§10, §13.27).
   - **PC:** enables ANSI processing on the console: `GetConsoleMode` on `STD_OUTPUT_HANDLE`, then `SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)`. Windows 10 shows raw escape bytes without this. If either call fails (old Windows, output redirected to a file or pipe), `console0.allowsAnsi()` returns `false`, and colour is skipped by §5 precedence rule 3. A mock of the MC boot may be added on PC if needed.
2. **`printerInit()` — binding.**
   - Bind `info.pInner`, `message.pInner` and `error.pInner` by resolving the role→target enums to objects (`McSink::usart2` → `&mcUsart2`, `PcSink::file1` → `&file1`, …). On PC, also set each stream's colour from the `cfg::*Color` constants.
   - **MC:** point each `fileN`'s backing `UsartStream` at the object named by `cfg::fileBackingUsart`. **PC:** nothing to bind. `fileN` is already a `FileStream` and opens its own `std::ofstream` on `open()`.

Until `printerInit()` has run, every stream writes to `nullSink` (§4).

## 10. Hardware directory

`Hardware/` is part of the `printer` target on both platforms. It holds the boot, the binding, and everything that knows about a chip.

| File | Platform | Content |
|---|---|---|
| `PrinterInit.hpp` | both | Declares `printerPlatformInit()` and `printerInit()` (§9) |
| `mc_PrinterInit.cpp` | MC | Real boot sequence (below) and binding to `mcUsart2` / `mcUsart3` |
| `pc_PrinterInit.cpp` | PC | Registers bound to static variables, VT-mode setup, PC-specific settings, binding to `console0` / `fileN` |
| `mc_chip.hpp` / `pc_chip.hpp` | MC / PC | **Public, single chip-selection point.** Each holds one manually uncommented `#include` of a chip header. Every file that needs chip definitions (`*_PrinterInit.cpp`, `UsartStream<N>`, tests) includes `mc_chip.hpp` or `pc_chip.hpp`, never a chip header directly |
| `STM32F103xE_regs.hpp` | both | Register **layout** only (`UsartRegs`, `DmaChannelRegs`, `GpioRegs`, `RccRegs`, …), platform-neutral, written from the reference manual (RM0008). The project is bare metal and uses no vendor headers (§13.30). A vendor header such as CMSIS's would not work here anyway, because `core_cm3.h` contains inline assembly that doesn't compile on a PC. One layout per chip line keeps MC and PC from drifting apart |
| `mc_STM32F103xE.hpp` | MC | Silicon facts: base addresses as `constexpr uintptr_t`, DMA channel mapping (USART2_TX = DMA1 channel 7, USART3_TX = DMA1 channel 2), pin tables per remap, the flash latency rule, and clock limits as `static_assert`s on `cfg::` (SYSCLK ≤ 72 MHz, APB1 ≤ 36 MHz, PLL multiplier 2–16). Accessors such as `usart2()` return `reinterpret_cast<UsartRegs*>(usart2Base)` at runtime, because a `reinterpret_cast` is not allowed in a constant expression |
| `pc_STM32F103xE.hpp` | PC | The same accessors, returning pointers to static `volatile` register storage |

- **Driver code is shared.** `UsartStream<N>` and the boot code reach registers only through the chip accessors. On the MC those point at the real addresses; on the PC they point at static storage. The same code therefore compiles and runs on both platforms.
- **PC register model.** At minimum, the registers are static `volatile` variables that tests read and set. A more sophisticated model may replace them: register updates at the proper simulated time and simulated interrupts (for example, TXE setting after one byte time, or a DMA transfer-complete interrupt). Any driver path that waits on a flag needs such a model, because a static flag never changes by itself.
- **PC-only chip headers are allowed.** A `pc_STM32F407xx.hpp` (with its `STM32F407xx_regs.hpp`) may exist without a matching `mc_` header when there is no plan to compile for that chip.
- **MC boot sequence** (`printerPlatformInit()`, §13.23). The Printer boots **only the hardware it needs**, and it runs on whatever CPU clock is active:
  0. **Clock check:** read the running clock from `RCC->CFGR` (SWS, PLLMUL, HPRE, PPRE1, PPRE2), compute SYSCLK and the APB frequencies, and **assert** that they equal `cfg::sysclkFrequency_Hz` / `apb1Frequency_Hz` / `apb2Frequency_Hz`. BRR keeps using the `constexpr` values, and the check guarantees they are right. Store the `CFGR` clock fields as a snapshot for the running check (below).
  1. **GPIO remapping,** if any `cfg::usartNRemap` is not `none`: enable the AFIO clock and set the remap bits.
  2. **GPIO,** always: enable the clocks of the ports used by the active interfaces and configure their pins (TX as alternate-function push-pull, RX as input, and RTS/CTS when `flowControl == rtsCts`).
  3. **Interfaces:** enable each USART's clock. Program CR1/CR2/CR3 from `UartSettings`, and program BRR as `(apbN + baud / 2) / baud`, where `apbN` is `apb1Frequency_Hz` for USART2/3 and `apb2Frequency_Hz` for USART1. SPI may follow later as another interface.
  4. **DMA,** if any `bankSize > 0`: enable the DMA1 clock and configure each TX channel.

  Each step enables the RCC clock of a peripheral (an `RCC->APBxENR` / `AHBENR` bit) before it touches that peripheral's registers. Those enable bits are the only RCC registers the Printer writes.
- **CPU boot is out of scope** (§13.23). The CPU boot covers starting HSE, flash latency, the PLL, the bus dividers and the SYSCLK switch. It may be implemented separately, but the Printer must start without it. After reset, an F103 runs on HSI at 8 MHz with all dividers at 1, so the Printer then works at that lower speed. A separate CPU boot must set the flash latency *before* raising SYSCLK (F103: 2 wait states above 48 MHz), and it must run **before** `prt1`'s constructor, because BRR is computed from the clock in effect at boot time. Where that hook lives depends on who owns the startup code (§13.31).
- **Running clock check** (§13.27). Before each transmission (each line in polled mode, each bank swap in DMA mode), the Printer reads `RCC->CFGR` once and compares its clock fields with the boot snapshot. A mismatch means a CPU boot changed the clock after the Printer booted, so every baud rate is now wrong: the Printer **asserts**. The check is always on, including release builds; its cost is one register read per line or bank. On PC, the RCC model returns the reset values, so the check passes.
- **The Printer's assert** halts the MC: it disables interrupts, executes `bkpt` if a debugger is attached, and then loops forever. On PC it calls `std::abort()`. It can't print, because the failure it reports may be the output path itself.

## 11. Build system

- CMake, one configuration per platform (MC / PC), chosen at configure time — same approach as the old project. They are two **configurations**, not two targets in one build tree, because the MC build needs its own compiler (an ARM toolchain file) and a build tree has only one compiler. A `CMakePresets.json` provides `pc` and `mc` presets, which CLion shows as two profiles.
- User code includes the umbrella header as `"Printer/Printer.hpp"`, so the directory that contains `Printer/` is on the include path.
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
| 13.5 | **Naming convention:** types `FooBar`, instances `fooBar`, macros UPPERCASE. **Keywords:** `info`, `message`, `error` (§4). *Revised 2026‑10‑24:* the 2026‑10‑23 version migrated `STR_LOG` → `streamLog`, `STR_MSG` → `streamMessage`, `STR_ERR` → `streamError`. Those names turned out hard to remember, so they were shortened, avoiding names that clash with the C library (`log`, `err`). | "streamMessage appeared harder to remember. Let's try to simplify keywords. Anyway, it is very simple to create an alias for a keyword"; set A (`info` / `message` / `error`) agreed, 2026‑10‑24. Earlier version recorded as confirmed, 2026‑10‑23 |
| 13.7 | **Default `fileN.name`:** `"noname.log"` on MC, and on PC for consistency (`cfg::defaultFileName`). | Recorded as confirmed, 2026‑10‑23 |
| 13.8 | **`StreamGuard` and `hold`: both.** `StreamGuard` is RAII for multi-line scopes. `hold` saves `pStream` into a single slot and is released by `endLine` (single level, no nesting). Only `endLine` releases a hold; `flushDma` and `printf` do not. A `StreamGuard` on top of an active hold is allowed and scoped independently. | Recorded as confirmed, 2026‑10‑23 |
| 13.9 | **`flushDma`:** flushes DMA unconditionally, wherever it appears. Colour state is not changed; the next byte does not re-emit. | Recorded as confirmed, 2026‑10‑23 |
| 13.10 | **Glossary migration** applied throughout (`info` / `message` / `error`; `file0..file8`; `BaseStream`; `pStream`; `mcUsartN`). *Revised 2026‑10‑24:* the keyword names follow §13.5. | Recorded as confirmed, 2026‑10‑23; keywords revised 2026‑10‑24 |
| 13.6 | **GPIO setup is owned by the Printer's MC boot** (§10 steps 1–2): remapping if needed, and always the GPIO used by the active interfaces. Pin tables live in the chip header; the remap choice lives in `config_mc.hpp`. | "if needed, boot and set GPIO Remapping, always boot GPIO needed by used interfaces", 2026‑10‑24 |
| 13.21 | **PC emulates registers with static variables** through `pc_<chip>.hpp` (§10). At minimum, static `volatile` storage; a more sophisticated model with timed register updates and simulated interrupts may replace it. | "PC can use known sophisticated model with registers updates at proper simulated time and interrupts simulation. Static values like 'pointer to a volatile int32' static is OK", 2026‑10‑24 |
| 13.23 | **The Printer boots only the hardware it needs; CPU boot is out of scope.** Order: GPIO remap if needed → GPIO for the used interfaces → interfaces (UART, later SPI) → DMA if needed (§10). The CPU boot (HSE, flash latency, PLL, dividers, SYSCLK switch) may be implemented separately, but the Printer must start without it, at the lower reset clock (F103: HSI 8 MHz). On PC, a mock may be implemented if needed. *Revised 2026‑10‑24:* an earlier version of this decision, from the same day, had the Printer also run the CPU boot (quoted second). | "Printer must boot needed hardware only. CPU boot is out of scope. It can be implemented, but printer must start without it - at lower speed. … comment out PLL - it's a part of CPU boot (optional), not printer", 2026‑10‑24. Superseded wording: "we must implement complex boot for MC … turn on PLL, achieve high frequency, switch CPU clock source. Then, if needed, boot and set GPIO Remapping, always boot GPIO needed by used interfaces, boot interfaces like UART or, potentially, SPI. Then boot and set DMA if needed. On PC a mock may be implemented if needed", 2026‑10‑24 |
| 13.24 | **Chip selection and chip files** (§2, §10). `Hardware/PrinterInit.hpp` declares the platform functions. Public `mc_chip.hpp` / `pc_chip.hpp` are the single selection points, each with one manually uncommented chip include. `mc_<chip>.hpp` holds the real addresses, `pc_<chip>.hpp` binds the registers to static variables, and a shared `<chip>_regs.hpp` holds the layout. Chip files are named after the CMSIS device define (`STM32F103xE`). PC-only chip headers (e.g. `pc_STM32F407xx.hpp`) are allowed. This also settles the "more MC platforms" question from §13.22. | "Pick the chip in one place … absolutely agree – add public mc_chip.h (and pc_chip.h)"; "we can potentially add pc_STM_F407xxxx.h only, without mc_STM_F407xxxx.h couple if we are not going to compile for STM32F407", 2026‑10‑24 |
| 13.26 | **HSE start failure is out of Printer scope.** Starting HSE belongs to the optional CPU boot (§13.23). The Printer never starts HSE, so it has no HSE failure path. | Follows from the revised §13.23, 2026‑10‑24 |
| 13.27 | **Clock check: assert.** BRR and the backpressure budget keep using the `constexpr` `cfg::` frequencies. At boot, the Printer reads the running clock from `RCC->CFGR` and asserts that it matches them. Before each transmission (each line, or each bank swap), it compares `CFGR` with the boot snapshot and asserts on a mismatch. Always on, including release builds (§10). | "assert, we are on a strict MC territory", 2026‑10‑24 |
| 13.28 | **Boot in the constructor; one umbrella header.** User code is `#include "Printer/Printer.hpp"` plus `Printer prt1 = {};`. The constructor calls `boot()`, which is public and idempotent. `Printer.hpp` declares `extern Printer prt1;`. `pStream` is `nullptr`-safe before construction (§4). Two CMake configurations, PC and MC, via presets (§11). | "single include like `#include "Printer/printer.h"`, CMake supports 2 targets - PC and MC. Printer prt1 = {}; creates Printer instance, (boot in constructor?)" (file name later corrected to `Printer/Printer.hpp`); "Boot in the constructor, with a nullptr-safe pStream and an idempotent boot() – OK", 2026‑10‑24 |
| 13.29 | **Headers are `.hpp` throughout,** each with a `__cplusplus` guard (§2). | "I'd go with .hpp throughout too", 2026‑10‑24 |
| 13.30 | **Bare metal only.** No vendor or CMSIS headers; register layouts are written from the reference manual (§2, §10). CMSIS may be named in documentation as an illustration. | "CMSIS – we can use that acronym in documentation for illustration, but now we implement bare metal only", 2026‑10‑24 |
| 13.32 | **`bankSize` replaces `bufferSize`.** The value is the size of **one** bank, and two banks are created: RAM = 2 × `bankSize`; 0 = polled, no DMA (§7). There is no even-size rule; the minimum is §13.33. `printerMaxDmaBuffer` is renamed `printerMaxBankSize` (≤ 65535, the CNDTR limit). The earlier text (total size, split in two, must be even) is superseded. | "Or result is not divide by 2, but multiply - create 2 buffers of given size. Let's decide"; "B with bankSize – agree", 2026‑10‑24 |
| 13.33 | **Minimum `bankSize` is 4** (when not 0), enforced by `isValid()` and by a `static_assert` in `UsartStream<N>` (§7). Banks of 1–3 bytes are formally meant to be supported, but they wait until the "head / body / tail" algorithm from the previous project is proven on them (§13.34). | "Formally, minimal bank size 1 must be supported … But I'm not sure that old algorithm … can handle this case correctly … let's say minimal bank size is 4 elements, but we promise to test it somehow later", 2026‑10‑24 |

### Open items

- **13.1** **`KeyWords::File{"name"}` / `ClearFile{"name"}` refactoring.** Decided: the form needs refactoring. Proposal to confirm: drop the constructor form entirely and rely on `fileN.name = "..."`, `fileN.open("...", OpenMode::append|truncate)` and `fileN.close()`. The constructor form was only a way to redirect the single old `file` target. With nine explicit `fileN` keywords it is redundant, and `OpenMode::truncate` takes over `ClearFile`.
- **13.2** **Marker text and trigger point.**
   - *Requirement (confirmed):* the marker string is identical on MC and PC, so one splitter produces the same `{name → content}` map from either platform's captured bytes. Owner wording: "parsed string equal for both platforms".
   - *Proposal:* `=== BEGIN <name> ===` / `=== END <name> ===`, where `<name>` is exactly `fileN.name`. On MC, markers go to the backing USART. On PC, the same markers are written **into the file itself** so a splitter sees identical bytes on both platforms.
   - *Trigger question, restated concretely:* if `file0.name = "run.csv"` and then `file0.close()` is called **without any bytes written**, should the output contain `=== BEGIN run.csv ===` + `=== END run.csv ===` (an empty file with markers present), or nothing at all (the file was never opened)? Proposal: markers are emitted at `open()`, so an empty opened file still yields a marker pair. The "name assigned, no `open()`" case is §13.17. Confirm, or specify "markers only on first byte".
   - *Sub-question:* does the PC file contain the markers, or are they stripped before writing? (Proposal: kept, for platform parity.)
   - *Sub-question:* on MC, should `OpenMode` be carried in the BEGIN marker (e.g. `=== BEGIN <name> truncate ===`) so the splitter can append or replace? Proposal: no. Each marker pair produces one fresh file, and `OpenMode` affects PC only.
- **13.11** **`printerPlatformInit()` naming.** `hardwareInit()` from earlier drafts is renamed `printerPlatformInit()` to match the camelCase convention and to pair with `printerInit()`. Confirm.
- **13.12** **`updateUsartX()` naming.** Old `update_UsartX()` renamed `updateUsartX()` per camelCase. Confirm.
- **13.13** **Colour model.** Applied in this draft: `Color` is a 16-colour SGR code with named presets (`darkGray` = 90, `darkGreen` = 32, …) that match the request's names. The alternative is `Color { r, g, b }`, emitted as 24-bit `ESC[38;2;r;g;bm`, which the request's palette names don't need and older terminals don't support. Confirm 16-colour, or ask for RGB, either instead or in addition.
- **13.14** **Unit-suffix naming exception** (§2). `cpuFrequency_MHz` breaks the plain camelCase rule. Applied in this draft: accept `_Unit` suffixes as an explicit exception. The alternative is to rename to `cpuFrequencyMhz`, `apb1FrequencyHz`, and so on. Confirm.
- **13.15** **Direct `stream.printf` vs `prt1` state.** `error.printf(...)` followed by `prt1 << endLine` resets colour on `prt1.pStream`, not on `error`, and it writes the newline to that sink as well. The interaction with `endLine`, colour and `hold` needs a rule. Options: (a) give `BaseStream` its own `endLine()` and colour lifecycle, so direct writes are self-contained; (b) route `printf` through `prt1` (`prt1 << error; prt1.printf(...)`); (c) document that direct `printf` is colour-idle until the next `prt1` switch to that stream. Decide.
- **13.16** **Several files open at once on the MC.** If `file0` and `file1` are both open, both forward to `fileBackingUsart`, their bytes interleave between markers, and the splitter cannot attribute them. Options: (a) only one `fileN` open at a time on the MC (debug assert on a second `open()`); (b) tag each chunk with a short marker (e.g. `=== FILE <name> ===` on every target switch). Decide.
- **13.17** **Writing without `open()`.** The first §8 example assigns `name` and writes without `open()`. Is there an implicit `open(name, OpenMode::append)` on first write? Or is a write to an unopened `fileN` dropped (or asserted)? Pick one rule. It decides the trigger question in §13.2.
- **13.18** **`hold` semantics by example.** Targets already persist until the next switch (§4), so a before/after example is needed to show what `hold` adds. A candidate reading is "restore the previous `pStream` at `endLine`": `prt1 << info << "a" << endLine << hold << error << "b" << endLine << "c"`, where `"c"` goes to `info`. Confirm or replace.
- **13.19** **`fileN.name` lifetime and enforcement.** A raw `const char*` must point to static-lifetime storage, and "legal only before first write / after `close()`" is not enforced today. Proposal: make `name` private behind `setName(const char*)` with a debug assert on misuse. The alternative is to keep the public field with a documented contract only.
- **13.20** **Role → target mapping form.** A `constexpr` pointer to `mcUsart2` would require `config_mc.hpp` to include the `UsartStream` declarations, which risks an include cycle (`UsartStream` needs `cfg::usart2Settings`). This draft shows enums (`McSink`, `PcSink`) that `printerInit()` resolves. Confirm, or choose an index mapping or another form.
- **13.22** **Other unspecified behaviour.**
    - `flushDma` on PC: a no-op, or `std::cout.flush()` / `ofstream::flush()`?
    - `UartSettings` on PC: unused, rejected by a `static_assert`, or used by a future PC serial-port sink?
    - ISR or concurrent writes into the DMA banks (handoff P-13): forbidden, guarded by a critical section, or given a separate ISR-safe path?
- **13.25** **The two boot functions.** The entry point is decided (§13.28): the `Printer` constructor calls the idempotent `boot()`, which runs `printerPlatformInit()` and then `printerInit()` (§9). Still to decide: should the two free functions stay public (for tests and for projects without `prt1`), or become private to `boot()`? And should `printerPlatformInit()` be renamed to match, e.g. `bootPlatform()` (this also answers §13.11)?
- **13.31** **Startup code and the CPU-boot hook.** Bare metal (§13.30) needs a vector table and a `Reset_Handler` that copies `.data`, clears `.bss`, runs the global constructors and calls `main()`. Who provides it: the Printer project, or the host project? It is the one place that can guarantee "CPU boot before `prt1`'s constructor" (§10). Proposal: the Printer project ships a startup file for each MC chip. Its `Reset_Handler` calls an optional weak `cpuBoot()` before the global constructors. A small CPU-boot example (72 MHz, flash latency first) defines `cpuBoot()` and shows `cfg::sysclkFrequency_Hz` / `cpuFrequency_MHz` being updated to match; the running clock check (§13.27) catches a mismatch. Decide.
- **13.34** **Bank sizes 1–3: the promised test.** Before `printerMinBankSize` can drop below 4, the DMA bank algorithm needs edge-case tests on the PC register model (§10), for `bankSize` = 1, 2, 3 and 4. They must cover: a write that exactly fills a bank; a write one byte longer than a bank; a swap while DMA is still busy (backpressure and drop); `updateUsartX()` on a partly filled bank; and the "head", "body" and "tail" states each being empty. The tests belong to the MC Hardware plan. If they pass, the minimum drops to 1; if not, the algorithm is fixed first or the minimum stays. Open until the tests run.
