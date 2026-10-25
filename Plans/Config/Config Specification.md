# Printer Configuration Specification (Draft)
**Status:** Draft. Companion to [`Config Request.md`](Config%20Request.md) in this folder.  
**Date:** 2026‑10‑23 (revised 2026‑10‑23 after review; 2026‑10‑24: Hardware, chip files, MC boot; CPU boot out of scope; keywords, .hpp, bare metal, constructor boot, clock check; config in Printer/Config, link-time platform selection; 2026‑10‑25: Printer API milestone — `prt2`, keywords `message` / `error` / `file0` … `file8`, formatted-output methods, flush model, blocking writes with stuck detection, `hold` confirmed, keyword types and aliases; 2026‑10‑26: `debug` role and `none` target; review fixes from `Found Issues.md`)

> The examples in `Config Request.md` are illustrative and predate the decisions below. Where they differ, **this specification is authoritative**.
>
> **Scope of this document.** §9 is the configuration specification proper. §§3–8 and §§10–12 are **Printer-wide context** the configuration depends on; they will move to a future `Printer Specification.md` once the request is accepted. §13 tracks decisions and open items for both.

## 1. Scope

The new `Printer` module is a **standalone, portable** subsystem, intentionally incompatible with the old project's `Printer`. The old code is a source of algorithms only; call sites will be migrated by hand. Design goals:

- Same public `operator<<` streaming model, no dynamic allocation.
- Single global `prt2` (named after the old `prt1`, with which it is not fully compatible, §13.43); all targets are keywords.
- Buffering and colour are core algorithm choices, not afterthoughts.
- Platform-specific code (USART/DMA/GPIO, console, real files) is isolated from common logic.

### 1.1 Memory and library rules (MC)

The MC has no operating system, very little RAM, and only a poor heap allocator with no defragmentation. Even when free heap is available, allocating anything larger than a byte can't be relied on. So these rules apply to **all common code** (`Printer/` outside `pc_*` files, including all of `Printer/Config/`) and to **all MC code** (`mc_*`, chip headers), §13.35:

1. **No C++ standard library, with one exception.** No `std::` facilities and no C++ library headers: not `<array>`, `<new>`, `<type_traits>`, `<algorithm>`, `<iostream>`, and so on. The allowed system headers hold types and macros only, and cost no RAM and no code:
   - `<cstdint>`, the one allowed C++ header (decision 13.35, revised 2026‑10‑24: "it works fine in similar SIL projects");
   - the compiler's own freestanding C headers `<stdint.h>`, `<stddef.h>`, `<stdarg.h>`, `<limits.h>` and `<float.h>`.

   Types are written unqualified (`uint8_t`, `uint32_t`). GCC's `<cstdint>` includes `<stdint.h>`, so the global names are declared too.
2. **No heap.** No `new`, `delete`, `malloc` or `free`, and nothing that calls them. All storage is static and sized at compile time.
3. **No exceptions and no RTTI.** The MC build uses `-fno-exceptions -fno-rtti`.
4. **Trivial destructors for globals.** A global with a non-trivial destructor is registered through `__cxa_atexit`, and the C library's `atexit` may allocate. Every global stream, `nullSink` and `prt2` keeps a trivial destructor.
5. **No function-local statics with dynamic initialization.** Their thread-safe guards pull in `__cxa_guard_*`; the MC build also uses `-fno-threadsafe-statics`.

**Enforcement:**
- **An include allow-list,** checked by CMake at configure time. Every `#include <...>` in a Common or MC file must name an allowed header (rule 1), or configuration fails; `CMAKE_CONFIGURE_DEPENDS` re-runs the check whenever such a file changes. This was checked with CMake 4.3.1 on 2026‑10‑24: adding `<array>` to `config.hpp` failed the next build with "`<array>` is not allowed in Common or MC code". It replaces the earlier `-nostdinc++` approach, which would also have blocked `<cstdint>`.
- The MC link provides no `_sbrk`, so any use of the heap becomes a link error instead of a runtime failure.

**PC-only code** (`pc_*.cpp` files, and headers included only by them) is free to use the full standard library, the heap, exceptions and the OS APIs (Windows, POSIX): the PC is a big developer's machine (§13.36). The boundary is that no `std` type, OS type or PC library header appears in any header that common code includes. A header follows the strictest area of any file that includes it. Shared driver code includes the PC chip headers, so they follow the common rules, and all of `Printer/Config/` (including `config_pc.hpp`) follows them too, so that one include check covers the whole directory. The working rules for each code area are in the repository's `AGENTS.md`.

## 2. Naming conventions

- **Types/classes:** `PascalCase` — `BaseStream`, `Stream`, `FileStream`, `UsartStream`, `ConsoleStream`, `NullStream`, `Printer`, `Color`, `UartSettings`.
- **Instances, members, functions, `constexpr` values:** `camelCase` — `prt2`, `message`, `error`, `debug`, `file0` … `file8`, `endLine`, `flush`, `hold`, `autoFlush`, `pStream`, `pInner`, `hasColor`, `printerInit`, `printerPlatformInit`, `printHexPrefix`, `platformHasColor`.
- **Unit-suffix exception:** a `constexpr` holding a physical quantity may end in `_Unit` — `cpuFrequency_MHz`, `apb1Frequency_Hz`, `apb2Frequency_Hz`. The suffix keeps the unit visible at every use site (§13.14).
- **Configuration constants** live inside `namespace cfg`. The namespace is `cfg`, not `Config`.
- **Preprocessor macros** (only when a macro is genuinely required): `SCREAMING_SNAKE_CASE` — e.g. `PLATFORM_HAS_COLOR` if a `#if` switch is needed. Preferred form is `constexpr bool` so `#if` is avoided; if a macro is used, it is UPPERCASE.
- **Headers are `.hpp`** throughout (§13.29). Every project header is C++17 and not C-compatible (`namespace`, `enum class`, `constexpr`), and the extension says so. Each header starts with `#pragma once` alone.
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
  Common streams:    messageStream, errorStream, debugStream (Stream)
                     fileStreams[0..8] (FileStream)

Keyword types (empty tags, §4):
  Message, Error, Debug, File<N> (File0..File8)
  Base keywords:     message, error, file0..file8
```

- `Printer::pStream` is `BaseStream*`. `operator<<` takes a **keyword** (`message`, `error`, `debug`, `fileN`, or any object of their types, §4) and points `pStream` at the stream that the keyword's type names. It also takes a `BaseStream&` directly, which selects that very object: tests use this with their own local `Stream`s, and a project can use it for a sink it owns and binds itself.
- `Stream` owns the colour logic. It wraps a `BaseStream* pInner`. `Stream::sendByteRaw(b)` emits an ANSI colour sequence on transition, then forwards `b` to `pInner`. When colour is disabled (§5, precedence) the bytes go straight through. `Stream` also holds an auto-flush flag, set at binding from the config (§4.1, §13.49); `Stream::flush()` forwards to `pInner`. Binding goes through a public `bind(BaseStream*)`, and `inner()` reads `pInner` back, because `printerInit()` is a free function in another file.
- `FileStream` is a `BaseStream` in its own right. It never carries colour. On MC it forwards text to a `UsartStream` and emits begin/end markers; on PC it writes to its own `std::ofstream`. A `Stream` may also use a file stream as its `pInner` (e.g. `errorStream` → `fileStreams[1]`, if a project configures it so, §9); colour is then skipped because `FileStream::allowsAnsi()` is `false`.
- `BaseStream` exposes `sendByteRaw`, `flush`, `allowsAnsi()`, and `resetColor()`. `sendByteRaw` and `flush` are **pure virtual**; only `allowsAnsi()` (default `false`), `resetColor()` (default no-op) and `autoFlush()` (default `false`) have defaults. `flush()` blocks until the bytes have left (§4.1). `Stream` overrides `autoFlush()` to return its flag, so `Printer` can ask `pStream->autoFlush()` through the base pointer, with no RTTI and no second pointer (§13.58).
- **Formatted output is a method on the printer, not on the stream** (§6, §13.45): `prt2.message(fmt, …)`, `prt2.error(fmt, …)`, `prt2.debug(fmt, …)` and `prt2.file(index, fmt, …)`. Every write goes through `prt2`, so colour, `endLine` and `hold` stay consistent. *Revised 2026‑10‑25:* the earlier `stream.printf(fmt, ...)` (§13.4) is removed, and the "`prt1.error` is removed" note of the earlier draft is superseded.

## 4. Target keywords

| Keyword | Keyword type | Stream it selects | Role |
|---|---|---|---|
| `message` | `Message` | `messageStream` (`Stream`) | Normal output, like `std::cout` (was `streamMessage`; also takes over the former `info` general log, §13.44) |
| `error` | `Error` | `errorStream` (`Stream`) | Errors, like `std::cerr`: a role, bindable to any sink; flushes itself by default (`autoFlush`, §4.1) |
| `debug` | `Debug` | `debugStream` (`Stream`) | Diagnostics, on a link of their own (MC default: USART3, the debug / calibration link); can be switched off by binding it to `none` (§13.57) |
| `file0` … `file8` | `File0` … `File8` (`File<0>` … `File<8>`) | `fileStreams[0]` … `[8]` (`FileStream`) | Nine named file sinks (extensible later) |

Type and stream names other than the base keywords are provisional (§13.56).

- **Two output roles** (§13.44). `info` and `message` were redundant: both example configurations sent them to the same sink, and only the colour differed. `message` is the `std::cout` of the Printer and `error` its `std::cerr`. `error` is a role, not a link to the PC's stderr: it is bound to whatever sink the configuration names (§9). *Revised 2026‑10‑26:* the earlier sentence "a project that needs a separate debug channel adds a keyword back, or uses a `fileN`" is superseded by the base keyword `debug` (below).
- **`debug`, the third output role** (§13.57). Unlike the dropped `info`, `debug` is not a copy of `message`: by default it goes to a different link (MC: USART3, which the original README calls the debug / calibration link), and it can be bound to **`none`**, which drops its output. An alias cannot provide this, because an alias is the same stream (§13.55); a separate channel has to be a base keyword.
  - **`none` is a target value for every role** (`McSink::none`, `PcSink::none`). `printerInit()` binds such a role to `nullSink`, so a project can silence `debug`, or any other role, from the configuration without touching a call site.
  - **Known limit:** a silenced role still costs the call. The arguments are evaluated and formatted before `nullSink` drops the bytes. Removing the calls entirely would need a compile-time switch at the call site, that is a macro, because common code can't see platform configuration (§5). Accepted for now.
- **Keyword names are short and must not clash with the C library** (§13.5). Keywords are global names, so `log` is ruled out (it hides `::log()` from `<math.h>`), and so is `err` (BSD `err()`). `message`, `error`, `debug`, `endLine`, `flush` and `hold` are safe (`debug` is lower case, so it does not meet the `DEBUG` / `NDEBUG` macros). A project can add its own alias for any base keyword (below).
- All are globals with static storage duration, defined in common code: the keywords as `inline constexpr` empty objects, and the streams behind them. Platform translation units bind the streams' inner sinks via `printerInit()`.
- **Use before boot is safe for the streams.** Every `Stream::pInner` (and the MC `FileStream` backing pointer) defaults to a common `NullStream nullSink`, which drops bytes. The constructors are `constexpr`, so the default is set during constant initialization and holds even for prints from other static constructors or early boot. Output written before `printerInit()` is lost, not a crash.
- **`prt2` itself is dynamically initialized,** because its constructor boots the hardware (§9) and therefore can't be `constexpr`. Its static storage is zero-filled first, so `pStream` reads as `nullptr` until the constructor has run, and `operator<<` treats a `nullptr` `pStream` like `nullSink`. That covers every operator: text, `endLine` and `flush` do nothing, and the first target switch has no outgoing stream to reset (§5). This is a practical guard only: formally, using `prt2` from another global's constructor before its own has run is undefined behaviour, so the rule stays "don't print through `prt2` from static constructors".
- `prt2 << message` sets `pStream = &messageStream` and leaves it there until the next switch. Same for `error` and `fileN`. `hold` makes one switch temporary (§4.1).
- **Each base keyword has its own type, and the type is the keyword** (§13.55). A keyword is an empty tag; `Printer` chooses the stream by the keyword's type, through one overload per type (`operator<<(Message)`, `operator<<(Error)`, `operator<<(Debug)`, and a template `operator<<(File<N>)`). Overload resolution also tells keywords apart from `const char*` and integers.
  - `File<N>` has `static_assert(N < 9)`, so `File<9>` is a compile error, not a run-time check.
  - Keyword objects are empty, `constexpr` and trivially destructible, so they cost no RAM and are safe before boot (§1.1).
- **Base keywords and aliases** (§13.54, §13.55). The base keywords are `message`, `error`, `debug` and `file0` … `file8`; there is no indexed `file(i)` keyword. Each `fileN` is its own sink: on the PC, a file with its own unique name (§8). **An alias is any object of a base keyword's type:**

  ```cpp
  inline constexpr File6   recorder{};   // prt2 << recorder << "blah" works exactly like prt2 << file6 << "blah"
  inline constexpr Message debug{};      // the same stream as message
  ```

  Because `Printer` looks only at the type, `recorder` and `file6` select the same `FileStream`, with the same binding, file name and `ofstream`, and `recorder.open(…)` opens the same file (§8). A plain `File6 recorder;` works too; `inline constexpr` is the recommended form in headers. An alias is only a second name. A project that needs a new, independent sink defines its own `Stream`, binds it, and selects it as a `BaseStream&` (§3).
- The old `KeyWords::File{"name"}` / `ClearFile{"name"}` constructor form is **deprecated and scheduled for refactoring** under the new `file0..file8` model (§13.1). The replacement is the method form on the file keyword: `fileN.setName("path")` or `fileN.open("path")` (§8).
- **`StreamGuard` and `hold` are both provided.** `StreamGuard` is the RAII scope for multi-line switches; `hold` is the chain-local keyword released by `endLine` (§4.1, §13.8, §13.18).

### 4.1 `endLine`, `flush` and `hold`

| Call | Effect |
|---|---|
| `'\n'` | A plain byte. No flush, no colour reset |
| `<< flush` | Flushes the hardware behind the current target. Blocks until it has been sent |
| `<< endLine` | `'\n'`, then `resetColor()`, then flush (§5, §13.40). Also releases `hold` |
| `prt2.flush(stream)` | Flushes the hardware behind that one stream. Blocks |
| `prt2.flush()` | Flushes every hardware sink. Blocks |
| `prt2.message(…)` / `prt2.error(…)` / `prt2.debug(…)` / `prt2.file(i, …)` | Format and write (§6). No flush, except on an `autoFlush` stream |

- **In the basic case the user doesn't care about flushing** (§13.46). Printing is blocking: when a buffer is full, the write waits until enough of it is free (§7). A flush is needed only where output must have left before the program goes on, and then it is one simple call.
- **A flush is per hardware stream.** On the MC a flush blocks until the USART has sent everything, which may take a long time, so the user must be able to flush one link alone. `prt2.flush(error)` forwards through `errorStream.pInner` to the sink behind it (`mcUsart3` in the §9 example). Keywords that share a sink share its flush. `prt2.flush(file0)` flushes the backing USART on the MC, and the `std::ofstream` on the PC. `prt2.flush` takes a keyword (one overload per keyword type, like `operator<<`), or a `BaseStream&`, so platform code may also pass a transport (`mcUsart2`, `console0`).
- **`prt2.flush()`** visits each hardware sink once, not each keyword: `mcUsart2`, `mcUsart3` on the MC; `console0` and every open `fileN` on the PC (provisional, §13.51).
- **What a flush is, per platform** (§13.22):
  - **MC:** every stream is a hardware link, `fileN` included (a `fileN` is a USART stream between markers, §8). A flush starts the partly filled bank (as `updateUsartX()` does) and waits until both banks have drained and the last byte has left (`TC`), under stuck detection (§7). On a polled USART it waits for `TC` only.
  - **PC:** `console0` → `std::cout.flush()`; `fileN` → `ofstream::flush()`; `nullSink` → no-op.
- **`autoFlush`, like `std::cerr`** (§13.49). A `Stream` flag, read through the virtual `BaseStream::autoFlush()` (§3, §13.58) and set at binding from `cfg::messageAutoFlush` / `cfg::errorAutoFlush` / `cfg::debugAutoFlush` (§9). `error` has it set by default. Every write to an `autoFlush` stream through `prt2` ends with a flush: each `<<` item while it is the target, and each `prt2.error(…)` call (granularity provisional, §13.51). On the MC that is a blocking wait per item, which is acceptable because errors are rare.
- **A flush never changes colour state** (§5, §13.9). The next byte does not re-emit the colour.
- **`flush` replaces `flushDma`** (§13.46). The name no longer refers to DMA, because the flush now applies to every sink.

**`hold`: a one-line stream guard** (§13.8, §13.18). `hold` makes the next target switch temporary. Typical use: an error appears in the middle of normal output, it goes to its own stream, and the default stream comes back without a temporary variable:

```cpp
prt2 << message << "position " << pos;
if (bad)
    prt2 << hold << error << "mask = " << mask << " out of range" << endLine;
prt2 << " done" << endLine;   // back on message
```

- `hold` saves the current target in a single slot. The following switch (`<< error`) works as usual.
- The next `endLine` does its own work on the held stream (`'\n'`, `resetColor()`, flush), then switches back to the saved target.
- Only `endLine` releases a hold. A `'\n'` byte, `<< flush` and the formatted-output methods do not (§13.8). A formatted-output call restores its own target and leaves the hold slot alone (§6).
- **One slot, no nesting.** In `<< hold << error << "a" << file0 << "b" << endLine`, the `endLine` returns to the original target, not to `error`.
- `hold` with no switch after it is harmless: `endLine` "restores" the same stream.
- **Trap:** a hold without its `endLine` stays active, and a later `endLine` on some other line restores unexpectedly.
- For several lines, use `StreamGuard`: the same save and restore, in a scope.

| Need | Use |
|---|---|
| One formatted error line | `prt2.error("mask = %d out of range\n", mask);` |
| An error line built with `<<` | `prt2 << hold << error << … << endLine;` |
| Several lines to another stream | `StreamGuard` |

## 5. Color (lazy)

- **Emission is lazy:** on the first byte written while the stream is **idle** (no colour active), `Stream::sendByteRaw` emits the active colour. A stream is idle after construction, after a target switch away from it, and after `endLine` — so the next line on the same stream re-emits its colour too. When `endLine` is streamed, the printer sends `'\n'` **first**, then calls `pStream->resetColor()`, which emits `ESC[0m` if a colour was active and marks the stream idle, and then flushes (§13.40, §4.1). The order matters: resetting first would leave the stream idle, so the newline itself would re-emit the colour, and flushing before the reset would leave `ESC[0m` in the buffer. A coloured line is therefore `ESC[<sgr>m text \n ESC[0m`.
- **What colour is for.** On the MC a stream is a hardware link and has no colour. On the PC, several emulated hardware streams share one console, and colour tells them apart; files and `nullSink` have none.
- **A `'\n'` byte is not a line end for colour.** It is plain data (§4.1). Text after it on the same stream keeps the same colour, which is correct, because it is the same stream. Every switch to another stream resets the colour first (below), including the switch back after a formatted-output call (§6).
- **Mid-line switch:** `prt2 << message << "a" << error << "b" << endLine` calls `resetColor()` on the outgoing stream before re-pointing `pStream`, so no escape leaks across a switch. Then `error` emits its own colour before `"b"`.
- **Selecting the current target again is not a switch.** `prt2 << message << "a" << message << "b"` keeps one colour run, with no `ESC[0m` between `a` and `b`. The same holds for an alias of the current target (§4) and for a formatted-output call on the stream that is already the target (§6). With no target yet (`pStream == nullptr`), there is nothing to reset.
- **`flush` never alters colour state** (§13.9, §13.46). The terminal's colour is unaffected by a flush, so the next byte must **not** re-emit the colour. Only `endLine` and a target switch reset colour.
- **Colour precedence.** A `Stream` emits colour only when all three hold:
  1. `cfg::platformHasColor` — compile-time platform capability (`false` on MC, `true` on PC). Only platform code reads it: on the MC there is no colour code at all (see "MC never colour" below);
  2. `cfg::useColorDefault` — compile-time project switch; `false` disables colour everywhere, even on PC;
  3. `pInner->allowsAnsi()` — runtime sink capability (`false` for `FileStream`, `UsartStream`, `NullStream`, and for a PC console where VT mode could not be enabled, §9).

  The per-stream `Color` (§9) is used only when all three are true.
- **Files never colour:** `fileN` are `FileStream` instances, not `Stream` wrappers, so they never see colour at all; a `Stream` whose `pInner` is a `FileStream` fails condition 3. This enforces "no coloured prints in files" structurally, not by convention.
- **MC never colour, at zero cost** (§13.38, provisional). Common code can't see platform constants (§9), so `if constexpr` on `cfg::platformHasColor` is not available there. Instead, `Stream`'s colour path is chosen at **link time**, like every other platform difference:
  - `Stream.hpp` / `Stream.cpp` (common) hold the class, `pInner`, the `idle` state and the binding.
  - `pc_Stream.cpp` defines `Stream::sendByteRaw` and `Stream::resetColor` with the colour logic above.
  - `mc_Stream.cpp` defines them as plain forwarding: no escape bytes and no `allowsAnsi()` call.

  So the MC binary contains no colour code. On PC, the `allowsAnsi()` check remains a runtime check.
- Only `Stream` overrides `resetColor()` meaningfully; every other `BaseStream` keeps the no-op default (§3).

## 6. Formatted output

**One method per role, on the printer** (§13.45):

```cpp
prt2.message("frame %u received\n", n);
prt2.error("mask = %d out of range\n", mask);
prt2.debug("state %u, pos %d\n", state, pos);
prt2.file(0, "%u,%d,%d\n", t, pos, vel);
```

- **Not called `printf`.** Only a subset of printf is supported, so the name must not promise more. Supported: `%d %i %u %x %X %f %s %c %p %%`, with `l` / `ll` length modifiers. `%f` precision is still ignored in v1.
- **No automatic newline.** The caller writes `'\n'` in the format string where a line ends. It is a plain byte: it does not flush and does not reset the colour (§4.1, §5).
- **The target is restored.** A call switches to its stream exactly as `<< message` would, calling `resetColor()` on the outgoing stream (§5). It writes, calls `resetColor()` on its own stream, and switches back. `prt2`'s current target and any active `hold` (§4.1) are unchanged afterwards.
- **No flush,** except on an `autoFlush` stream (`error` by default), which flushes at the end of the call (§4.1).
- **`prt2.file(index, …)`** takes the index of `file0` … `file8` as a run-time value, so it can be computed. An index out of range prints a line to `error` (`file index 12 out of range`) and drops the text (§13.45).
- **Compile-time format check.** The methods are declared with `__attribute__((format(printf, 2, 3)))`, and `file` with `format(printf, 3, 4)` (argument 1 is the implicit `this`), so GCC/Clang check the arguments against the format string. Only the compiler's checker is borrowed; the name `printf` does not appear in the API. The checker also accepts specifiers we don't support (`%e`, `%g`, `%5.2f`), so the engine prints those literally and the mistake is visible in the output (provisional, §13.52).
- A free-function format engine (`printFormatted(BaseStream&, const char*, va_list)`, with `va_list` from the compiler's `<stdarg.h>`, §1.1) does the work; the three methods forward to it.
- **No formatted output on a stream directly.** `stream.printf(...)` is removed (§13.4, superseded), so no write can bypass `prt2`, and its interaction with `endLine`, colour and `hold` is defined (§13.15).
- A project alias (§4) has no method of its own; it is written to with `<<`.
- Inside `Printer`'s member functions, the method names `message`, `error`, `debug` and `flush` hide the global keywords, so the implementation writes `::message`, `::error`, `::debug`, `::flush`. User code is unaffected.

## 7. Buffering (MC)

Buffering is a core algorithm: **two buffers or no buffering**. No intermediate strategies.

**`UartSettings::bankSize` is the size of one bank** (§13.32). **RAM = 2 × `bankSize`; 0 = polled, no DMA.**

- `UartSettings::bankSize == 0` → **polled** transmission (wait for TXE, write `DR`). No DMA, no RAM cost.
- `UartSettings::bankSize > 0` → **double-buffered DMA**: two banks of `bankSize` bytes each. DMA drains the active bank; CPU writes into the inactive bank. On bank-full: wait for DMA completion, swap banks, start transmission of the full bank, and go on writing.
- **Writing blocks; no byte is lost while the hardware works** (§13.47). A write into a full bank, a polled write waiting for TXE, and a flush (§4.1) all wait as long as it takes. *Revised 2026‑10‑25:* the earlier rule "wait up to the backpressure budget, then drop the byte" (backpressure by loss) is superseded.
- **Stuck detection** (§13.47). Printing to broken hardware (a USART that isn't clocked, a DMA that never started, CTS held off for good) would otherwise hang the whole program. The old Printer had a defensive mechanism for this, and it is kept:
  - A wait continues as long as the hardware **makes progress**. If it makes none for a budget, the stream is **stuck**, and the byte is dropped.
  - On **every later write** the stream is checked once. If it has recovered, the write proceeds normally. If it is still stuck, the byte is dropped and forgotten, and the call returns as fast as it can, without waiting. Only the first wait, the one that detects the stuck state, costs the budget.
  - A flush on a stuck stream returns at once.
  - *Provisional details* (§13.50): progress means the DMA counter (`CNDTR`) moves or `TXE` sets; the budget is a few byte-times at the configured baud rate, counted in CPU cycles from `cfg::cpuFrequency_MHz`.
  - The PC needs no stuck detection: console and file writes do not hang this way.
- **No ISR code in the Printer** (§13.48). Interrupts are enabled only so the DMA can run; the Printer defines no interrupt handlers. Bank swaps happen in the writing code, and the hardware drains by itself, so `main` can't deadlock waiting for an interrupt.
- There is no even-size rule. `bankSize` is also the longest burst the CPU can write before a swap, and the length of one DMA transfer (CNDTR).
- **Minimum bank size: 4** (`cfg::printerMinBankSize`, §13.33). Formally, any `bankSize ≥ 1` should work, and even a 1-byte bank can speed up printing to the UART. But the algorithm taken from the previous project describes the buffer state as "head", "body" and "tail", and it is not yet known to handle banks of 1–3 bytes correctly. With at least 4 elements, those three parts can't collapse onto each other, so the comparisons stay simple. Lowering the minimum to 1 is a promise kept open until edge-case tests prove it (§13.34).
- **Banks are sized per instance.** `UsartStream` is a class template on its bank size: `UsartStream<cfg::usart2Settings.bankSize> mcUsart2;`. For `N > 0` the instance holds `uint8_t banks[2][N]`; the `UsartStream<0>` specialisation is polled and holds no bank array. A polled USART therefore really costs no RAM, and a DMA USART costs exactly `2 × bankSize`.
- `static_assert(N == 0 || N >= cfg::printerMinBankSize)` — the minimum above.
- `static_assert(N <= cfg::printerMaxBankSize)` — `printerMaxBankSize` is an **upper bound only**; it does not size any array. Values above it are a build error. It must itself stay ≤ 65535, because CNDTR is a 16-bit register.
- The old `update_UsartX()` (non-blocking kick-out of a partially filled bank) is retained under the new name `updateUsartX()` (§13.12). A flush does the same kick-out and then waits (§4.1).
- **Open:** writes into the banks from an ISR or a second context (handoff P-13) are not yet specified (§13.22). The Printer itself has no ISR code (§13.48).

## 8. File naming

Each file keyword has `setName`, `open` and `close` methods. They are `const` members, because `file0` … `file8` are `inline constexpr` objects (review F-03); they act on the `FileStream` that the keyword's type selects, so an alias (§4) names, opens and closes the same file:

```cpp
file0.setName("logs/run1.csv");                    // name only
prt2 << file0 << "t,pos,vel" << endLine;   // implicit open? see §13.17

file0.open("logs/run1.csv");                      // assigns name, appends, emits BEGIN marker
file1.open("logs/run2.csv", OpenMode::truncate);  // assigns name, truncates, emits BEGIN marker
prt2 << file0 << "t,pos,vel" << endLine;
prt2.file(0, "%u,%d,%d\n", t, pos, vel);          // the same file, formatted (§6)
file0.close();                                    // flushes and emits END marker
```

- **The name is set through `setName`, not a public field.** A keyword is an empty tag (§4), so it can't hold the name; the name lives in the `FileStream`. *Revised 2026‑10‑25:* the earlier `file0.name = "…"` field form is replaced. This takes the `setName` side of §13.19.
- Naming is legal only before first write or after `close()`. The pointed-to string must have static lifetime. Enforcement is open (§13.19).
- **Default name if unset:** `cfg::defaultFileName` (`"noname.log"`), declared in `config.hpp` and used on both platforms.
- **`open(name, mode = OpenMode::append)`**, with `enum class OpenMode { append, truncate }`. `truncate` replaces the old `ClearFile` semantics.
- **PC:** `open()` opens `std::ofstream(name)` in the requested mode. `close()` flushes and releases. The `std::ofstream` objects live in a function-local static array inside `pc_FileStream.cpp`, one per `fileStreams[N]`, constructed on the first `open()`, so the common `FileStream.hpp` never includes `<fstream>` (§1.1). `FileStream` itself holds only its index, its name pointer and an `isOpen` flag.
- **MC:** `open()` emits a BEGIN marker on the backing USART; `close()` emits an END marker. Bytes between markers are plain text. No actual file is created on the MC — the markers delimit text on the USART so a PC-side capture can be split back into files. Whether `OpenMode` is carried in the marker is part of §13.2.
- **Marker text is identical on both platforms** (§13.2). A single splitter parses either platform's captured byte stream and produces the same `{name → content}` map.
- **Open:** two files open at once on the MC share one USART and would interleave (§13.16).
- **Closing is command-driven, not process-exit-driven.** A `stop recording file` command (emitted by the Task Manager or the Recorder) closes the named file: on PC it flushes and releases the `std::ofstream`; on MC it emits the END marker on the backing USART. There is no `atexit`-style shutdown hook — the Printer module does not own program termination.

## 9. Configuration files

Three data-only headers, all in `namespace cfg`, in **`Printer/Config/`** (§13.37): the common `config.hpp`, and the platform-architecture headers `config_pc.hpp` and `config_mc.hpp` (the usual PC / MC capabilities). Chip-specific details live in `Hardware/` instead (§10), with one selection entry per platform (`mc_chip.hpp`, `pc_chip.hpp`). **No assignments at namespace scope**: all configuration is `constexpr` data consumed by `printerInit()` / `printerPlatformInit()`.

**Who includes what** (§13.37). Common `.cpp` files include only common headers, `config.hpp` among them, and never a platform config. Platform-specific `.cpp` files (`pc_*` / `mc_*`) include their own platform config directly. The build selects the `pc_*` or `mc_*` translation units, so the platform difference is resolved by the linker (§11). There is no forwarding header and no platform macro.

The sketches below fix names and types. The values are examples.

### `config.hpp` — common
- Feature flags: `printHexPrefix`, `doubleScientific`, `doublePlusInExponent`, `useColorDefault` (see §5 for its precedence).
- `Color` is a 16-colour SGR foreground code, with named presets that match the request's names (`darkGray`, `darkGreen`, …). 24-bit RGB is not supported in v1 (§13.13).
- `UartSettings { baudRate, bankSize, dataBits, parity, stopBits, flowControl }`. `flowControl` comes from the README's "hardware flow control" requirement.
- Default `UartSettings` constants used by both platforms.
- `printerMinBankSize` and `printerMaxBankSize` (bounds, §7), and `defaultFileName` (§8).
- No references to platform object names (`USART2`, `console0`, …).
- `isValid(const UartSettings&)`: a `constexpr` check that `bankSize` is 0 or within the bounds, `dataBits` is 8 (any parity) or 9 (no parity), and `baudRate > 0`. It does not check `stopBits`, `parity` or `flowControl`: they are `enum class` values, which can only be out of range through a cast, and a cast is the caller's responsibility (§13.59). Platform headers `static_assert` it for each of their settings constants. The header also `static_assert`s its own bounds and `uartDefault`.
- All variables are `inline constexpr` (one object per program, not one per translation unit). The only system header is `<cstdint>` (§1.1).

```cpp
#pragma once

// Common Printer configuration (Config Specification §9). Data only, namespace cfg.

#include <cstdint>

namespace cfg {

// Feature flags
inline constexpr bool printHexPrefix       = true;
inline constexpr bool doubleScientific     = false;
inline constexpr bool doublePlusInExponent = false;
inline constexpr bool useColorDefault      = true;   // project switch, §5 precedence

// Colour: 16-colour SGR foreground code, emitted as ESC[<sgr>m (§13.13)
struct Color { uint8_t sgr; };
inline constexpr Color black{30},      red{31},        darkGreen{32}, yellow{33},
                       blue{34},       magenta{35},    cyan{36},      lightGray{37},
                       darkGray{90},   brightRed{91},  green{92},     brightYellow{93},
                       brightBlue{94}, brightMagenta{95}, brightCyan{96}, white{97};

// UART settings
enum class Parity      : uint8_t { none, even, odd };
enum class StopBits    : uint8_t { one, two };
enum class FlowControl : uint8_t { none, rtsCts };

struct UartSettings {
    uint32_t    baudRate;
    uint32_t    bankSize;     // one bank; RAM = 2 x bankSize; 0 = polled, no DMA (§7)
    uint8_t     dataBits;     // data bits, parity bit not counted (§13.42, deferred)
    Parity      parity;
    StopBits    stopBits;
    FlowControl flowControl;
};

inline constexpr uint32_t printerMinBankSize = 4;      // §7, §13.33; 1-3 not yet proven
inline constexpr uint32_t printerMaxBankSize = 4096;   // upper bound only, §7; <= 65535 (CNDTR)

inline constexpr UartSettings uartDefault{115200, 0, 8, Parity::none, StopBits::one, FlowControl::none};

inline constexpr auto defaultFileName = "noname.log";          // §8, both platforms

// A settings constant is valid when the USART can be programmed from it as written.
constexpr bool isValid(const UartSettings& s) {
    const bool bankOk = s.bankSize == 0
                     || (s.bankSize >= printerMinBankSize && s.bankSize <= printerMaxBankSize);
    const bool bitsOk = s.dataBits == 8 || (s.dataBits == 9 && s.parity == Parity::none);
    return s.baudRate > 0 && bankOk && bitsOk;
}

static_assert(printerMinBankSize >= 1 && printerMinBankSize <= printerMaxBankSize);
static_assert(printerMaxBankSize <= 65535, "DMA CNDTR is a 16-bit register");
static_assert(isValid(uartDefault));

} // namespace cfg
```

*Revised 2026‑10‑26 (implementation review 000):* the sketch follows the implemented `Printer/Config/config.hpp`. The field is `baudRate`, not `baudrate`, because "baud rate" is two words (§2, §13.61). `defaultFileName` is declared `auto`, which deduces the same type, `const char* const` (§13.62). The implemented file indents the namespace body; that is formatting only, and the project has no rule for it.

### `config_mc.hpp` — STM32F103 (example)
- **Choices only, no silicon facts.** Register addresses, DMA channel mapping, pin tables and clock limits are facts about the chip, and they live in the chip headers in `Hardware/` (§10). `config_mc.hpp` therefore includes no vendor header and compiles on any host.
- `constexpr bool platformHasColor = false`.
- **Clocks — the clock in effect, not a clock the Printer sets (§10, §13.23).** The Printer does not boot the CPU. `hseFrequency_Hz` records the board crystal for an optional CPU boot. The PLL and divider values are part of that CPU boot, so they are commented out here. `sysclkFrequency_Hz` is the clock running when the Printer boots (in `prt2`'s constructor): without a CPU boot, an F103 runs on HSI = 8 MHz with all dividers at 1. The rest of the Printer uses frequencies **derived** from it: `cpuFrequency_MHz` for the stuck-detection budget **only** (§7; called the DMA backpressure budget before 2026‑10‑25), and `apb1Frequency_Hz` (USART2, USART3) / `apb2Frequency_Hz` (USART1) for BRR. Once a CPU boot runs the F103 at 72 MHz, APB1 runs at 36 MHz, so computing BRR from the CPU clock would double every baud rate. The chip header `static_assert`s these values against the silicon limits. Keeping them in step with a CPU boot is §13.27.
- GPIO remap choice per USART (`usart2Remap`, `usart3Remap`). The pins each remap selects come from the chip header.
- `usart2Settings` / `usart3Settings`. Per the request, USART3's `bankSize` is **zero by default**, so it is polled with no DMA. USART2's example `bankSize` of 2048 costs 2 × 2048 = 4 KB of RAM (64 KB on the F103VET6).
- Role → physical `UsartStream` mapping (`messageTarget`, `errorTarget`, `debugTarget`, `fileBackingUsart`), expressed as an enum that `printerInit()` resolves, so this header does not need the `UsartStream` declarations (§13.20).
- `autoFlush` per role (`messageAutoFlush`, `errorAutoFlush`, `debugAutoFlush`), copied into each `Stream` by `printerInit()` (§4.1, §13.49).

```cpp
#pragma once
#include "config.hpp"   // no vendor headers: silicon facts live in Hardware/mc_<chip>.hpp (§10)

namespace cfg {

inline constexpr bool platformHasColor = false;

// Clocks. The Printer does not boot the CPU (§10, §13.23); it runs on the clock in effect.
inline constexpr uint32_t hseFrequency_Hz = 8'000'000;   // board crystal, used by an optional CPU boot

// Optional CPU boot, not part of the Printer:
// inline constexpr uint32_t pllMultiplier = 9;          // SYSCLK = HSE x 9 = 72 MHz
// inline constexpr uint32_t ahbDivider    = 1;
// inline constexpr uint32_t apb1Divider   = 2;          // APB1 = 36 MHz
// inline constexpr uint32_t apb2Divider   = 1;

// Clock in effect when prt2 boots. Without a CPU boot: HSI = 8 MHz, all dividers 1 (§13.27).
inline constexpr uint32_t sysclkFrequency_Hz = 8'000'000;                      // HSI after reset
inline constexpr uint32_t cpuFrequency_MHz   = sysclkFrequency_Hz / 1'000'000;  // stuck-detection budget only (§7)
inline constexpr uint32_t apb1Frequency_Hz   = sysclkFrequency_Hz;              // USART2, USART3 BRR
inline constexpr uint32_t apb2Frequency_Hz   = sysclkFrequency_Hz;              // USART1 BRR

enum class UsartRemap : uint8_t { none, partial, full };   // pins per remap: chip header
inline constexpr UsartRemap usart2Remap = UsartRemap::none;       // F103: PA2/PA3
inline constexpr UsartRemap usart3Remap = UsartRemap::none;       // F103: PB10/PB11

inline constexpr UartSettings usart2Settings{115200, 2048, 8, Parity::none, StopBits::one, FlowControl::none};
inline constexpr UartSettings usart3Settings{230400, 0,    8, Parity::none, StopBits::one, FlowControl::none};
                                             // usart2: 2 x 2048 = 4 KB RAM; usart3: bankSize 0 by default, polled

enum class McSink : uint8_t { none, usart2, usart3 };   // proposal, §13.20; none: nullSink (§13.57)
inline constexpr McSink messageTarget    = McSink::usart2;
inline constexpr McSink errorTarget      = McSink::usart3;
inline constexpr McSink debugTarget      = McSink::usart3;   // the debug / calibration link
inline constexpr McSink fileBackingUsart = McSink::usart3;

inline constexpr bool messageAutoFlush = false;
inline constexpr bool errorAutoFlush   = true;     // like std::cerr (§4.1, §13.49)
inline constexpr bool debugAutoFlush   = false;

} // namespace cfg
```

### `config_pc.hpp` — PC
- `constexpr bool platformHasColor = true`. Colour still needs VT mode at runtime (see the PC boot below).
- Role → endpoint mapping. A role can target `console0`, a `fileN`, or `none` (§13.57); the request's example sent `error` to `file1`. *Revised 2026‑10‑26 (§13.60):* the example now sends `error` to `console0` in red, because nothing opens `file1` and its errors would be dropped (review F-04).
- Per-`Stream` `Color` values, using the presets from `config.hpp`.
- `autoFlush` per role, as on the MC (§4.1, §13.49).
- The emulated chip is chosen in `Hardware/pc_chip.hpp`, not here (§10, §13.24).

```cpp
#pragma once
#include "config.hpp"

namespace cfg {

inline constexpr bool platformHasColor = true;

enum class PcSink : uint8_t { none, console0, file0, file1, file2, file3, file4, file5, file6, file7, file8 };   // none: nullSink (§13.57)
inline constexpr PcSink messageTarget = PcSink::console0;
inline constexpr PcSink errorTarget   = PcSink::console0;   // red in the console (§13.60)
inline constexpr PcSink debugTarget   = PcSink::console0;

inline constexpr Color messageColor = darkGreen;
inline constexpr Color errorColor   = brightRed;
inline constexpr Color debugColor   = darkGray;

inline constexpr bool messageAutoFlush = false;
inline constexpr bool errorAutoFlush   = true;           // like std::cerr (§4.1, §13.49)
inline constexpr bool debugAutoFlush   = false;

} // namespace cfg
```

### Boot and binding
The user's program needs one include and one definition (§13.28):

```cpp
#include "Printer/Printer.hpp"   // umbrella header: Printer, keywords, config

Printer prt2 = {};               // the constructor boots the Printer

int main() {
    prt2 << message << "Hello Printer 2!" << endLine;
}
```

- `Printer/Printer.hpp` declares `extern Printer prt2;`, and the user's program defines it. Library code can print through `prt2`, and a project that forgets the definition gets a link error, not a silent failure.
- **The constructor calls `boot()`.** `boot()` is public and **idempotent**: a static flag makes the second and later calls no-ops, so a second `Printer` instance or an explicit `prt2.boot()` never re-programs the hardware.
- A constructor can't report failure (no exceptions on the MC). A boot failure, such as a failed clock check (below), is handled by the Printer's assert.

`Hardware/PrinterInit.hpp` declares two functions, each **defined per platform** in `Hardware/mc_PrinterInit.cpp` / `Hardware/pc_PrinterInit.cpp` and selected by the CMake configuration. `boot()` runs them in this order (§13.25):

1. **`printerPlatformInit()` — hardware boot.** It runs first, so no byte reaches an interface that is not yet running. The MC sequence is in §10. On the MC it starts with the clock check (§10, §13.27).
   - **PC:** enables ANSI processing on the console: `GetConsoleMode` on `STD_OUTPUT_HANDLE`, then `SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)`. Windows 10 shows raw escape bytes without this. If either call fails (old Windows, output redirected to a file or pipe), `console0.allowsAnsi()` returns `false`, and colour is skipped by §5 precedence rule 3. A mock of the MC boot may be added on PC if needed.
2. **`printerInit()` — binding.**
   - Bind `messageStream.pInner`, `errorStream.pInner` and `debugStream.pInner` by resolving the role→target enums to objects (`McSink::usart2` → `&mcUsart2`, `PcSink::file1` → `&fileStreams[1]`, …), and set each stream's `autoFlush` from `cfg::*AutoFlush`. A role whose target is `none` is bound to `&nullSink`, so its output is dropped (§13.57). On PC, also set each stream's colour from the `cfg::*Color` constants.
   - **MC:** point each `fileStreams[N]`'s backing `UsartStream` at the object named by `cfg::fileBackingUsart`. **PC:** nothing to bind. Each `fileStreams[N]` is already a `FileStream` and opens its own `std::ofstream` on `open()`.
   - The keywords need no binding: they are empty tags, and their types name the streams (§4).

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
- **CPU boot is out of scope** (§13.23). The CPU boot covers starting HSE, flash latency, the PLL, the bus dividers and the SYSCLK switch. It may be implemented separately, but the Printer must start without it. After reset, an F103 runs on HSI at 8 MHz with all dividers at 1, so the Printer then works at that lower speed. A separate CPU boot must set the flash latency *before* raising SYSCLK (F103: 2 wait states above 48 MHz), and it must run **before** `prt2`'s constructor, because BRR is computed from the clock in effect at boot time. Where that hook lives depends on who owns the startup code (§13.31).
- **Running clock check** (§13.27). Before each transmission (each line in polled mode, each bank swap in DMA mode), the Printer reads `RCC->CFGR` once and compares its clock fields with the boot snapshot. A mismatch means a CPU boot changed the clock after the Printer booted, so every baud rate is now wrong: the Printer **asserts**. The check is always on, including release builds; its cost is one register read per line or bank. On PC, the RCC model returns the reset values, so the check passes.
- **The Printer's assert** halts the MC: it disables interrupts, executes `bkpt` if a debugger is attached, and then loops forever. On PC it calls `std::abort()`. It can't print, because the failure it reports may be the output path itself.

## 11. Build system

- CMake, one configuration per platform (MC / PC), chosen at configure time — same approach as the old project. They are two **configurations**, not two targets in one build tree, because the MC build needs its own compiler (an ARM toolchain file) and a build tree has only one compiler. A `CMakePresets.json` provides `pc` and `mc` presets, which CLion shows as two profiles.
- User code includes the umbrella header as `"Printer/Printer.hpp"`, so the directory that contains `Printer/` is on the include path.
- **Library rules in the build (§1.1):** CMake checks the `#include <...>` lines of every Common and MC source against the allow-list at configure time, and the MC configuration adds `-fno-exceptions -fno-rtti -fno-threadsafe-statics`. `pc_*` sources are not checked.
- `mc_*` / `pc_*` translation units are selected per configuration; polymorphism is link-time (abstract `BaseStream` in common, concrete implementations in `mc_` or `pc_` files). A `pc_X.cpp` and an `mc_X.cpp` may define the same symbols, so a build that compiled both would fail to link. The duplicate-symbol error makes that mistake visible at once.
- **Other build systems** (IAR, CubeMX-generated projects, …) must follow the same rule when collecting files: an MC project excludes every `pc_*.cpp`, and a PC project excludes every `mc_*.cpp`. The prefix makes this a single file pattern, but it may have to be set up by hand in such tools (§13.37). `pc_*` / `mc_*` headers need no exclusion, because only files of their own platform include them.
- Exact target layout is deferred; the old project's scheme will be reused. Requirement: `Printer` is a self-contained CMake target that links into a new project without pulling in the old `Common/` tree.

## 12. Testing

- **Two absolutely independent test suites, one per platform** (§13.36). They share no test code, no helpers and no framework.
  - **PC suite** (`Tests/pc/`): our own simple suite with no external framework (§13.39); otherwise it may use everything the PC offers (the standard library, files, processes, Windows or Linux APIs). It also tests the common code, compiled for the host. That a common header is clean is shown by the CMake include check (§1.1), not by the test itself.
  - **MC suite** (`Tests/mc/`): does its own job under the §1.1 rules, and prints plain text rather than emoji.
- **Frameworks** (§13.39): the PC suite is our own simple suite, similar to the old project's, with no external framework. The MC suite is a brand-new suite, written later with the MC Hardware plan.
- **Tests run from `main()`** (§13.41). Printer2 is a technical demo, so there is no separate test executable. `Tests/Tests.hpp` declares `struct Tests { static int runAll(); };`, the only file the suites have in common, holding no test code. `pc_Tests.cpp` or `mc_Tests.cpp` defines it, selected at link time like every `pc_` / `mc_` pair. `main()` calls it first: on the MC before the endless loop, on the PC before whatever the demo does. It returns the number of failed tests.
- **Isolation** (§13.41). Every test is independent. It builds its own `Printer`, `Stream`s and `CaptureStream`s, and may drive them into any state, even an inconsistent one, without affecting the next test. Tests don't change the global keyword streams or `prt2`. A binding test that must change one restores it afterwards. "Safe before boot" (§4) is proved at compile time, with `constexpr` probe objects, because the tests run after `prt2` has booted.
- Tests target the new API only. The old `set_sendByte_*_override` hooks are **not** carried over.
- Recommended hook: a `CaptureStream : BaseStream` that records raw bytes, installed by tests as the `pInner` of a test's own `Stream`, or as the `pStream` of a test's own `Printer`. No test hook lives inside production `UsartStream` / `ConsoleStream`. Each suite has its own `CaptureStream`.

## 13. Decisions and open items

Item numbers are stable: a decided item keeps its number, so references such as §13.8 stay valid.

### Decisions

| ID | Decision | Owner wording / date |
|---|---|---|
| 13.3 | **Shutdown marker is command-driven.** A `stop recording file` command from the Task Manager / Recorder closes the file (flush on PC, END marker on MC). The Printer has no `printerDeinit()` / `atexit` responsibility. Implementation-time sub-item: a missing END marker (crash or reset before the stop command) is treated by the splitter as truncated / abnormal and recorded in its report (proposal). | Recorded as resolved in the 2026‑10‑23 draft; owner wording not captured verbatim |
| 13.4 | **printf signature:** `stream.printf(fmt, ...)` (Option B). *Superseded 2026‑10‑25 by §13.45:* formatted output is a method on the printer (`prt2.message` / `error` / `file`), and `stream.printf` is removed. | Recorded as confirmed, 2026‑10‑23 |
| 13.5 | **Naming convention:** types `FooBar`, instances `fooBar`, macros UPPERCASE. **Keywords:** `info`, `message`, `error` (§4). *Revised 2026‑10‑24:* the 2026‑10‑23 version migrated `STR_LOG` → `streamLog`, `STR_MSG` → `streamMessage`, `STR_ERR` → `streamError`. Those names turned out hard to remember, so they were shortened, avoiding names that clash with the C library (`log`, `err`). *Revised 2026‑10‑25 (§13.44):* `info` is dropped; the keywords are `message` and `error`. | "streamMessage appeared harder to remember. Let's try to simplify keywords. Anyway, it is very simple to create an alias for a keyword"; set A (`info` / `message` / `error`) agreed, 2026‑10‑24. Earlier version recorded as confirmed, 2026‑10‑23 |
| 13.7 | **Default `fileN.name`:** `"noname.log"` on MC, and on PC for consistency (`cfg::defaultFileName`). | Recorded as confirmed, 2026‑10‑23 |
| 13.8 | **`StreamGuard` and `hold`: both.** `StreamGuard` is RAII for multi-line scopes. `hold` saves `pStream` into a single slot and is released by `endLine` (single level, no nesting). Only `endLine` releases a hold; `flushDma` and `printf` do not. A `StreamGuard` on top of an active hold is allowed and scoped independently. *Note 2026‑10‑25:* `flushDma` is now `flush` (§13.46) and `printf` is now the formatted-output methods (§13.45); neither releases a hold. The meaning of `hold` is confirmed in §13.18. | Recorded as confirmed, 2026‑10‑23 |
| 13.9 | **`flushDma`:** flushes DMA unconditionally, wherever it appears. Colour state is not changed; the next byte does not re-emit. *Superseded 2026‑10‑25 by §13.46:* renamed `flush`; it flushes the hardware behind the current target and blocks. The colour rule stands. | Recorded as confirmed, 2026‑10‑23 |
| 13.10 | **Glossary migration** applied throughout (`info` / `message` / `error`; `file0..file8`; `BaseStream`; `pStream`; `mcUsartN`). *Revised 2026‑10‑24:* the keyword names follow §13.5. *Revised 2026‑10‑25:* `info` is dropped (§13.44), and the printer is `prt2` (§13.43). | Recorded as confirmed, 2026‑10‑23; keywords revised 2026‑10‑24 |
| 13.6 | **GPIO setup is owned by the Printer's MC boot** (§10 steps 1–2): remapping if needed, and always the GPIO used by the active interfaces. Pin tables live in the chip header; the remap choice lives in `config_mc.hpp`. | "if needed, boot and set GPIO Remapping, always boot GPIO needed by used interfaces", 2026‑10‑24 |
| 13.21 | **PC emulates registers with static variables** through `pc_<chip>.hpp` (§10). At minimum, static `volatile` storage; a more sophisticated model with timed register updates and simulated interrupts may replace it. | "PC can use known sophisticated model with registers updates at proper simulated time and interrupts simulation. Static values like 'pointer to a volatile int32' static is OK", 2026‑10‑24 |
| 13.23 | **The Printer boots only the hardware it needs; CPU boot is out of scope.** Order: GPIO remap if needed → GPIO for the used interfaces → interfaces (UART, later SPI) → DMA if needed (§10). The CPU boot (HSE, flash latency, PLL, dividers, SYSCLK switch) may be implemented separately, but the Printer must start without it, at the lower reset clock (F103: HSI 8 MHz). On PC, a mock may be implemented if needed. *Revised 2026‑10‑24:* an earlier version of this decision, from the same day, had the Printer also run the CPU boot (quoted second). | "Printer must boot needed hardware only. CPU boot is out of scope. It can be implemented, but printer must start without it - at lower speed. … comment out PLL - it's a part of CPU boot (optional), not printer", 2026‑10‑24. Superseded wording: "we must implement complex boot for MC … turn on PLL, achieve high frequency, switch CPU clock source. Then, if needed, boot and set GPIO Remapping, always boot GPIO needed by used interfaces, boot interfaces like UART or, potentially, SPI. Then boot and set DMA if needed. On PC a mock may be implemented if needed", 2026‑10‑24 |
| 13.24 | **Chip selection and chip files** (§2, §10). `Hardware/PrinterInit.hpp` declares the platform functions. Public `mc_chip.hpp` / `pc_chip.hpp` are the single selection points, each with one manually uncommented chip include. `mc_<chip>.hpp` holds the real addresses, `pc_<chip>.hpp` binds the registers to static variables, and a shared `<chip>_regs.hpp` holds the layout. Chip files are named after the CMSIS device define (`STM32F103xE`). PC-only chip headers (e.g. `pc_STM32F407xx.hpp`) are allowed. This also settles the "more MC platforms" question from §13.22. | "Pick the chip in one place … absolutely agree – add public mc_chip.h (and pc_chip.h)"; "we can potentially add pc_STM_F407xxxx.h only, without mc_STM_F407xxxx.h couple if we are not going to compile for STM32F407", 2026‑10‑24 |
| 13.26 | **HSE start failure is out of Printer scope.** Starting HSE belongs to the optional CPU boot (§13.23). The Printer never starts HSE, so it has no HSE failure path. | Follows from the revised §13.23, 2026‑10‑24 |
| 13.27 | **Clock check: assert.** BRR and the backpressure budget (the stuck-detection budget since 2026‑10‑25, §13.47) keep using the `constexpr` `cfg::` frequencies. At boot, the Printer reads the running clock from `RCC->CFGR` and asserts that it matches them. Before each transmission (each line, or each bank swap), it compares `CFGR` with the boot snapshot and asserts on a mismatch. Always on, including release builds (§10). | "assert, we are on a strict MC territory", 2026‑10‑24 |
| 13.28 | **Boot in the constructor; one umbrella header.** User code is `#include "Printer/Printer.hpp"` plus `Printer prt1 = {};`. The constructor calls `boot()`, which is public and idempotent. `Printer.hpp` declares `extern Printer prt1;`. `pStream` is `nullptr`-safe before construction (§4). Two CMake configurations, PC and MC, via presets (§11). *Revised 2026‑10‑25:* the instance is named `prt2` (§13.43); the quoted wording keeps `prt1`. | "single include like `#include "Printer/printer.h"`, CMake supports 2 targets - PC and MC. Printer prt1 = {}; creates Printer instance, (boot in constructor?)" (file name later corrected to `Printer/Printer.hpp`); "Boot in the constructor, with a nullptr-safe pStream and an idempotent boot() – OK", 2026‑10‑24 |
| 13.29 | **Headers are `.hpp` throughout,** starting with `#pragma once` alone (§2). *Revised 2026‑10‑24:* the `__cplusplus` guard of the first version was dropped; the `.hpp` name already says C++17, not C-compatible. | "I'd go with .hpp throughout too"; then "#pragma once alone works fine for now. .hpp names already says that project is C not compatible, C++ 17 or better", 2026‑10‑24 |
| 13.30 | **Bare metal only.** No vendor or CMSIS headers; register layouts are written from the reference manual (§2, §10). CMSIS may be named in documentation as an illustration. | "CMSIS – we can use that acronym in documentation for illustration, but now we implement bare metal only", 2026‑10‑24 |
| 13.32 | **`bankSize` replaces `bufferSize`.** The value is the size of **one** bank, and two banks are created: RAM = 2 × `bankSize`; 0 = polled, no DMA (§7). There is no even-size rule; the minimum is §13.33. `printerMaxDmaBuffer` is renamed `printerMaxBankSize` (≤ 65535, the CNDTR limit). The earlier text (total size, split in two, must be even) is superseded. | "Or result is not divide by 2, but multiply - create 2 buffers of given size. Let's decide"; "B with bankSize – agree", 2026‑10‑24 |
| 13.33 | **Minimum `bankSize` is 4** (when not 0), enforced by `isValid()` and by a `static_assert` in `UsartStream<N>` (§7). Banks of 1–3 bytes are formally meant to be supported, but they wait until the "head / body / tail" algorithm from the previous project is proven on them (§13.34). | "Formally, minimal bank size 1 must be supported … But I'm not sure that old algorithm … can handle this case correctly … let's say minimal bank size is 4 elements, but we promise to test it somehow later", 2026‑10‑24 |
| 13.35 | **No C++ standard library and no heap in common and MC code** (§1.1). Allowed system headers: `<cstdint>` and the compiler's freestanding C headers (`<stdint.h>`, `<stddef.h>`, `<stdarg.h>`, …). No `new` / `malloc`, no exceptions, no RTTI, trivial destructors for globals. Enforced with a CMake include allow-list and an MC link without `_sbrk`. *Revised 2026‑10‑24:* `<cstdint>` was added to the allowed headers, so the first version's `-nostdinc++` enforcement (which blocks it) was replaced by the allow-list. | "MC platform has no operation system and a very poor heap allocator … there is no heap defragmenter. So, using std:: is prohibited"; then "I like that: #include <cstdint>, it works fine in similar SIL projects", 2026‑10‑24 |
| 13.36 | **PC code is free; tests are split per platform.** `pc_*` code may use the standard library, the heap and the OS APIs, as long as nothing PC-specific reaches a header that common code includes (§1.1). Tests are two independent suites, PC and MC (§12). The rules are also written down in `AGENTS.md`. | "PC platform branch can use std:: and feel free on a big developers PC. Tests are separated - absolutely independent suite for each platform. PC tests can use all PC Windows/Linux capabilities. MC tests will do their own job", 2026‑10‑24 |
| 13.37 | **Config location and platform selection.** All config headers live in `Printer/Config/`: the common `config.hpp`, and the platform-architecture `config_pc.hpp` / `config_mc.hpp`. Chip specifics live in `Hardware/`, with one selection entry per platform. Common `.cpp` files include only common headers. Platform `.cpp` files are selected by `CMakeLists.txt`, so the platform difference is resolved by the linker. Non-CMake projects (IAR, CubeMX) exclude the other platform's prefixed `.cpp` files, possibly by hand (§9, §11). This supersedes base plan decisions D-1 (`Config/` outside the library) and D-2 (`config_platform.hpp` forwarders). | "common cross-platform config can be placed in the Printer/Config. Target Platform architecture-specific (usual PC/MC capabilities) - in this directory too. Easy to find. But once included hardware-specifics live in Hardware directory"; "common .cpp uses common headers. Platform-specific .cpp are selected by CMakeLists.txt - polymorphism at linker level. If we will use IAR or CubeMX projects - the projects must ignore pc_ prefixed .cpp when collecting project files. Maybe, it has to be done manually", 2026‑10‑24 |
| 13.39 | **Test suites:** the PC suite is our own simple suite, similar to the old project's, with no external framework, run from `main()` (§13.41). The MC suite is a brand-new suite, written later (§12). This is base plan D-3, and it settles handoff P-10. | "mostly agree. For PC we will write own simple suite similar to the old one. For MC we will write brand-new test suite later", 2026‑10‑24 |
| 13.40 | **`endLine` sends `'\n'` first, then resets the colour** (§5), so a coloured line is `ESC[<sgr>m text \n ESC[0m`. The line ending is `"\n"` on both platforms for now; whether MC serial terminals need `"\r\n"` is for the Printer core plan. This is base plan D-5. *Revised 2026‑10‑25 (§13.46):* `endLine` also flushes, so the order is `'\n'`, `resetColor()`, flush. | "agree", 2026‑10‑24; the added flush: "yes", 2026‑10‑25 |
| 13.41 | **Tests run from `main()`, with one result type, and every test isolated** (§12). No separate test executable: `main()` runs `Tests::runAll()` before the endless loop (MC) or the demo's own work (PC). All PC suites share one `TestResults` type. The PC summary may use emoji; the MC suite prints something more compatible. Each PC test builds its own `Printer` and objects, and may break them freely without affecting the next test. | "as this is not a real project, but a Printer technical demo, it's OK to run all the tests from main before entering endless loop on MC and doing whatever on PC"; shared `TestResults`: "good"; "emoji works fine on PC. MC tests will print something else, more compatible"; "I like all tests to be isolated. For each PC test we instantiate a new printer and test it as we wish - even breaking up to inconsistent state - next step will be not affected", 2026‑10‑24 |
| 13.13 | **Colour model: a 16-colour palette with names.** `Color` holds a 16-colour SGR foreground code, with named presets (`darkGray` = 90, `darkGreen` = 32, …) matching the request's names. 24-bit RGB is not used. | "nevermind, let's use palette with names", 2026‑10‑24 |
| 13.15 | **No direct stream printing; formatted output goes through `prt2`** (§6). Decided by §13.45: `stream.printf` is removed, and each formatted-output method switches to its stream, writes, resets the colour and switches back, so colour, `endLine` and `hold` stay consistent. This is option (b) of the former open item, as one call. | Follows from §13.45, 2026‑10‑25 |
| 13.18 | **`hold` makes the next target switch temporary** (§4.1). It saves the current target in one slot; the next `endLine` does its own work on the held stream and then switches back. Use: an error appears in the middle of other output, goes to its own stream, and the default stream comes back without a temporary variable. | "\"<< hold\" is a new cool feature, an easy to use \"stream guard\". What can it be use for? In the middle, an error appeared. We want to print an error to a special stream, then restore the default stream. Idea: \"hold\" will meke next stream selection temporary. Otherwise, we have to write: static tempStream = currStream, after printing error - currStream=tempStream", 2026‑10‑25 |
| 13.22 (part) | **Flush per platform** (§4.1). On the MC every stream, `fileN` included, is a hardware link, and a flush drains its USART. On the PC, `console0` flushes `std::cout`, and `file0` … `file8` wrap the `std::` file methods, so they flush their `ofstream`. The other parts of §13.22 stay open. | "on MC file is a stream similar to all others. On PC file stream is a complex wrapper around std:: file methods"; "on MC stream is a hardware link, on pc it is a text in 1 console (colored to separate emulated hardware streams) and a file - with bank of 8 \"files\" and \"null\"", 2026‑10‑25 |
| 13.43 | **The printer instance is `prt2`.** The new Printer is not fully compatible with the old one's `prt1`, and the name says so. User code is `Printer prt2 = {};` (§9). | "old one was prt1, let's name this one prt2 - they are not fully compatible", 2026‑10‑25 |
| 13.44 | **Two output keywords: `message` (like `std::cout`) and `error` (like `std::cerr`)**, plus `file0` … `file8` (§4). `info` is dropped as redundant with `message`. `error` is a role, bindable to any sink, not the PC's stderr. Revises §13.5. *Extended 2026‑10‑26 (§13.57):* a third role, `debug`, is added. It is not the dropped `info` again: it has its own default link and can be switched off. | "error / message / file(index, …) - I think having \"info\" and \"message\" is redundant. message for std::cout and error for std::cerr", 2026‑10‑24; "just \"error\" keyword and method name", 2026‑10‑25 |
| 13.45 | **Formatted output: `prt2.message(fmt, …)`, `prt2.error(fmt, …)`, `prt2.file(index, fmt, …)`** (§6). No `printf` in the name, because only a subset is supported. No automatic newline: the caller writes `'\n'`. After the call, `prt2`'s target is what it was before. An out-of-range file index prints a line to `error`. Supersedes §13.4. | "what if we will use \"prt1.error(...\" and \"prt1.file(<file index>...\"? 7 methods less and without \"printf\" word - I'm not going to support all standard printf features. \"printf\" name is not honest"; "no automatical endlines - let user add '\n' if needed", 2026‑10‑24; target restored: "yes"; bad index: "line to error by default", 2026‑10‑25 |
| 13.46 | **Flush model** (§4.1). `<< flush` and `prt2.flush(stream)` flush the one hardware stream behind the target; `<< endLine` is `'\n'`, `resetColor()`, flush; `prt2.flush()` flushes every stream. A flush blocks. `'\n'` is a plain byte and causes no flush. In the basic case the user doesn't care about flushing, because printing is blocking. The formatted-output methods don't flush, except on an `autoFlush` stream (§13.49). Supersedes §13.9, revises §13.40, and settles handoff P-3. | "`<< flush`, `<< endLine`, `prt.flush(<stream>)` - flushes 1 hardware stream - because on MC flush is blocking and may take a lot of time - ability to flush 1 hardware stream is needed. `prt.flush()` without arguments flushes all streams. '\n' is just a symbol, use it if you want to end the string, doesn't cause flush manually. In basic case, user doesn't care about flush - printing is blocking, if buffer is full - long call waiting for enough to fit free buffer memory will happen. As for `prt.msg(...)` and other \"similar to printf\" calls - I was told that manual flush must be a simple call. `prt.flush()`, `prt.flush(<stream>)` - OK" (after a discussion with an experienced engineer); the model summary: "I absolutely agree with your understanding", 2026‑10‑25 |
| 13.47 | **Blocking writes with no loss, and stuck detection** (§7). A write into a full buffer waits; no byte is lost while the hardware works. A stream whose hardware makes no progress is stuck, and its byte is dropped. Every later write checks the stream once: if it has recovered, the write proceeds; if not, the byte is dropped and forgotten, and the call returns at once. Supersedes the "backpressure by loss" rule of §7. Details: §13.50. | "new rule \"block with no loss\" is correct"; "If we will print to problematic hardware, the whole program can hang. To avoid that original Printer had a defencive mechanism - dropped after stuck"; "on every write we can check stuck stream - and release it. If we are printing byte and can't release stream - drop byte, forget forever, exit as fast as you can", 2026‑10‑25 |
| 13.48 | **No ISR code in the Printer** (§7). Interrupts are enabled so the DMA can work, but the Printer implements no interrupt handlers; the hardware does its job and can't hang `main`. Whether user code may print from an ISR stays open (§13.22). | "we enable ISR to let DMA work, but we don't implement any ISR functions - hardware does it's job and will not hang the main", 2026‑10‑25 |
| 13.49 | **`error` flushes itself, like `std::cerr`** (§4.1). Each `Stream` has an `autoFlush` flag, set from a per-role config constant (`cfg::messageAutoFlush = false`, `cfg::errorAutoFlush = true`, §9). Granularity: §13.51. | "It's good to flush error messages like std::cerr does"; config constant: "I agree", 2026‑10‑25 |
| 13.54 | **Base keywords `file0` … `file8`, no indexed `file(i)` keyword** (§4). Each `fileN` is its own sink; on the PC, a file with a unique name. Projects give the base keywords their own names with aliases. `prt2.file(index, …)` (§13.45) stays the only indexed access. | "let's have base keywords file0..8. Example: `prt2 << file6 << "blah" ...` - file6 on PC is a file with a unique name. The framework let's one to easily create any aliases for the base keywords - just create value with any name and one of given types", 2026‑10‑25 |
| 13.55 | **Each base keyword has its own type; any object of that type is that keyword** (§3, §4). Keywords are empty tags (`Message`, `Error`, `File<N>`, provisional names §13.56), and `Printer` selects the stream by the type, with one `operator<<` overload per keyword type. The streams are separate objects (`messageStream`, `errorStream`, `fileStreams[N]`). `File6 recorder;` makes `prt2 << recorder` work exactly like `prt2 << file6`, and `recorder.open(…)` opens the same file. File names move from a public field to `setName` (§8). This restores the old project's model, in which every keyword was an empty `KeyWords::…` struct with its own overload (`valve_6/Common/Printer/KeyWords.h`). It reverses the 2026‑10‑23 §4 text "each target is a distinct global object … without a tag type". Example: when the Recorder needs a file, it creates its own alias of a file keyword type, `File6 recorder;`; which file it picks doesn't matter. | "In keywords, add `BaseKeyWordTypeFoo bar;` - now bar will work exactly like foo, for example, `prt2 << bar << "blah" << ...` will work exactly like `prt2 << foo << "blah" << ...`"; "How did old project work? when processing a keyword, called operator with a unique type argument"; go-ahead: "yes"; on the Recorder: "When recorder will be needed, it can create an alias "recorder has the same type as file6. Or file2. Doesn't matter"", 2026‑10‑25 |
| 13.57 | **A third output role, `debug`, and a `none` target** (§4, §9). Keyword `debug` (type `Debug`, stream `debugStream`, provisional names §13.56) and method `prt2.debug(fmt, …)`. Default targets: USART3 on the MC (the debug / calibration link), `console0` in `darkGray` on the PC; `debugAutoFlush = false`. `McSink` and `PcSink` gain `none`, which binds a role to `nullSink`, so any role can be switched off from the configuration. Known limit: a silenced call still evaluates and formats its arguments (§4). Extends §13.44. | "I have decided that a one more stream "debug" can be useful. Your thoughts?"; on the defaults, `none` and `prt2.debug(…)`: "Yes for all 3 questions", 2026‑10‑26 |
| 13.58 | **`autoFlush()` is a virtual query on `BaseStream`** (§3, §4.1). Default `false`; `Stream` overrides it to return its flag. `Printer` asks `pStream->autoFlush()`, so a stream selected through any path, including `operator<<(BaseStream&)`, keeps its auto-flush. From review finding F-02 (`Found Issues.md`). | "1. F-02: a virtual autoFlush() on BaseStream" – "yes", 2026‑10‑26 |
| 13.59 | **`isValid()` checks no enum ranges** (§9). `stopBits`, `parity` and `flowControl` are `enum class` values, which can be out of range only through a cast; that is the caller's responsibility, and the limit is documented instead of checked. From review finding F-10. | "F-10: document the limit" – "yes", 2026‑10‑26 |
| 13.60 | **Where errors go by default: the PC console in red, USART3 on the MC** (§9). The PC example's `errorTarget` changes from `PcSink::file1` to `PcSink::console0` (with the existing `errorColor = brightRed`); the MC example already uses `McSink::usart3`. Before this, the PC example sent errors to `file1`, which nothing opens, so they were silently dropped. The demo prints errors with `prt2 << hold << error << … << endLine` (§4.1). From review finding F-04. | "On PC to console, use red color. On MC to USART3"; for the demo: "use prt2 << hold <<error << ... << endLine", 2026‑10‑26 |
| 13.61 | **`UartSettings::baudRate`**, not `baudrate` (§9): "baud rate" is two words, so it is camelCase like `dataBits` and `stopBits` (§2). BY made the change in `config.hpp`; the spec and the plans follow it. The request's `settings.baudrate` is quoted owner text and stays. From implementation review 000, D-1. | "Keep baudRate", 2026‑10‑26 |
| 13.62 | **`defaultFileName` is `inline constexpr auto`** (§9). It deduces `const char* const`, the same type as the explicit `const char*` form, so the address probe and the tests are unchanged. The review recommended the explicit type; BY kept `auto`. From implementation review 000, D-2. | "Keep auto", 2026‑10‑26 |

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
- **13.14** **Unit-suffix naming exception** (§2). `cpuFrequency_MHz` breaks the plain camelCase rule. Applied in this draft: accept `_Unit` suffixes as an explicit exception. The alternative is to rename to `cpuFrequencyMhz`, `apb1FrequencyHz`, and so on. Confirm.
- **13.15** *Decided 2026‑10‑25; see the Decisions table.* The question was: `error.printf(...)` followed by `prt1 << endLine` resets colour on `prt1.pStream`, not on `error`, so direct stream printing needed a rule. Options were (a) a colour lifecycle per `BaseStream`, (b) routing formatted output through the printer, (c) documenting the gap. Option (b) was taken through §13.45.
- **13.16** **Several files open at once on the MC.** If `file0` and `file1` are both open, both forward to `fileBackingUsart`, their bytes interleave between markers, and the splitter cannot attribute them. Options: (a) only one `fileN` open at a time on the MC (debug assert on a second `open()`); (b) tag each chunk with a short marker (e.g. `=== FILE <name> ===` on every target switch). Decide.
- **13.17** **Writing without `open()`.** The first §8 example assigns `name` and writes without `open()`. Is there an implicit `open(name, OpenMode::append)` on first write? Or is a write to an unopened `fileN` dropped (or asserted)? Pick one rule. It decides the trigger question in §13.2.
- **13.18** *Decided 2026‑10‑25; see the Decisions table.* The question was what `hold` adds over targets that already persist. The candidate reading "restore the previous `pStream` at `endLine`" was confirmed, with BY's use case (an error in the middle of other output) as the example in §4.1.
- **13.19** **`fileN.name` lifetime and enforcement.** A raw `const char*` must point to static-lifetime storage, and "legal only before first write / after `close()`" is not enforced today. Proposal: make `name` private behind `setName(const char*)` with a debug assert on misuse. The alternative is to keep the public field with a documented contract only. *Note 2026‑10‑25:* §13.55 settles the form: keywords are empty tags, so the name is set by `fileN.setName(…)` (§8). The enforcement question (a debug assert on misuse) stays open.
- **13.20** **Role → target mapping form.** A `constexpr` pointer to `mcUsart2` would require `config_mc.hpp` to include the `UsartStream` declarations, which risks an include cycle (`UsartStream` needs `cfg::usart2Settings`). This draft shows enums (`McSink`, `PcSink`) that `printerInit()` resolves. Confirm, or choose an index mapping or another form.
- **13.22** **Other unspecified behaviour.**
    - ~~`flushDma` on PC: a no-op, or `std::cout.flush()` / `ofstream::flush()`?~~ *Decided 2026‑10‑25:* `flush` calls `std::cout.flush()` / `ofstream::flush()` (Decisions, "13.22 (part)").
    - `UartSettings` on PC: unused, rejected by a `static_assert`, or used by a future PC serial-port sink?
    - ISR or concurrent writes into the DMA banks (handoff P-13): forbidden, guarded by a critical section, or given a separate ISR-safe path? *Note 2026‑10‑25:* the Printer itself has no ISR code (§13.48). The question now concerns user code that prints from an ISR. With blocking writes (§13.47), such a print can wait inside the ISR for a long time.
- **13.25** **The two boot functions.** The entry point is decided (§13.28): the `Printer` constructor calls the idempotent `boot()`, which runs `printerPlatformInit()` and then `printerInit()` (§9). Still to decide: should the two free functions stay public (for tests and for projects without `prt2`), or become private to `boot()`? And should `printerPlatformInit()` be renamed to match, e.g. `bootPlatform()` (this also answers §13.11)?
- **13.31** **Startup code and the CPU-boot hook.** Bare metal (§13.30) needs a vector table and a `Reset_Handler` that copies `.data`, clears `.bss`, runs the global constructors and calls `main()`. Who provides it: the Printer project, or the host project? It is the one place that can guarantee "CPU boot before `prt2`'s constructor" (§10). Proposal: the Printer project ships a startup file for each MC chip. Its `Reset_Handler` calls an optional weak `cpuBoot()` before the global constructors. A small CPU-boot example (72 MHz, flash latency first) defines `cpuBoot()` and shows `cfg::sysclkFrequency_Hz` / `cpuFrequency_MHz` being updated to match; the running clock check (§13.27) catches a mismatch. Decide.
- **13.34** **Bank sizes 1–3: the promised test.** Before `printerMinBankSize` can drop below 4, the DMA bank algorithm needs edge-case tests on the PC register model (§10), for `bankSize` = 1, 2, 3 and 4. They must cover: a write that exactly fills a bank; a write one byte longer than a bank; a swap while DMA is still busy (a blocking wait, and the stuck-detection drop, §7; revised 2026‑10‑25 from "backpressure and drop"); `updateUsartX()` on a partly filled bank; and the "head", "body" and "tail" states each being empty. The tests belong to the MC Hardware plan. If they pass, the minimum drops to 1; if not, the algorithm is fixed first or the minimum stays. Open until the tests run.
- **13.38** **`Stream`'s colour path per platform.** It follows from §13.37: common code can't see `cfg::platformHasColor`, so the earlier `if constexpr` in common code is gone. This draft splits `Stream`:
  - `Stream.cpp` (common) holds the class, `pInner`, the idle state and the binding;
  - `pc_Stream.cpp` holds the colour emission;
  - `mc_Stream.cpp` holds plain forwarding, so the MC has no colour code (§5).

  The alternative is to keep one common `Stream.cpp` and decide at run time only, through `allowsAnsi()`, which returns `false` on every MC sink. That is simpler, but it puts a small amount of unused colour code and one virtual call per line into the MC binary. Confirm the split, or choose the run-time-only version.
- **13.42** **Does `dataBits` count the parity bit? Deferred.** Parity is used very rarely; one known case is the APPA 106N multimeter, which uses parity over its optical (photo) physical layer. On the F103, the word length (M bit) is 8 or 9 bits *including* parity. Until a real parity user appears, `config.hpp` keeps the provisional rule: `dataBits` counts data bits only, 8 is valid with any parity, 9 only without parity, and 7 is excluded. Revisit when a device that needs parity is connected. (Phase 1 plan Q-1.)
- **13.50** **Stuck detection details** (§7, §13.47). Provisional proposal, 2026‑10‑25:
  - **Progress** means the DMA counter (`CNDTR`) moves, or `TXE` sets on a polled USART.
  - **The budget** is a few byte-times at the configured baud rate, counted in CPU cycles from `cfg::cpuFrequency_MHz`. The old Printer's budget formula, if it can be recovered, should be compared.
  - **The check on later writes** is one register read. If the hardware has caught up (bank free, or `TXE` set), the stream is released and the write proceeds.

  Confirm, or describe the old Printer's mechanism so it can be copied.
- **13.51** **Flush and `autoFlush` details** (§4.1). Provisional proposal, 2026‑10‑25:
  - `prt2.flush(stream)` takes a `BaseStream&`; `Stream::flush()` forwards to `pInner`.
  - `prt2.flush()` visits each hardware sink once (MC: `mcUsart2`, `mcUsart3`; PC: `console0` and every open `fileN`), not each keyword.
  - `autoFlush` flushes after every write operation through `prt2` while that stream is the target: each `<<` item, each formatted-output call. The alternative is a flush at `'\n'` only, which would make `error` line-buffered rather than unbuffered like `std::cerr`.

  Confirm.
- **13.52** **Unsupported format specifiers** (§6). GCC's format checker accepts specifiers the engine doesn't support (`%e`, `%g`, width and precision). Provisional: the engine prints such a specifier literally, so the mistake shows in the output. The alternatives are to drop it silently or to print a marker such as `%?`. Confirm.
- **13.53** **Number of files: 8 or 9?** The spec has nine file sinks, `file0` … `file8`, and `prt2.file(index, …)` accepts 0–8. BY's 2026‑10‑25 description of the PC side mentions "a bank of 8 "files" and "null"". Is that an approximate count, or should there be eight (`file0` … `file7`)? Until confirmed, the spec keeps nine.
- **13.55** *Decided 2026‑10‑25; see the Decisions table.* The open question was the alias form. The provisional reading (a reference, `FileStream& recorder = file6;`) was replaced by BY's rule: keywords have their own types, and any object of a keyword's type is that keyword.
- **13.56** **Names of the keyword types and the streams behind them** (§4, §13.55). Provisional:
  - **Keyword types:** `Message`, `Error`, `Debug` (added 2026‑10‑26, §13.57), `File<N>`, with `File0` … `File8` as `using` aliases. They follow the PascalCase rule (§2), but `Message`, `Error` and `Debug` are common words, and a user project may already have such types. Alternatives: `MessageKeyword` / `ErrorKeyword` / `FileKeyword<N>`, or a namespace, as in the old `KeyWords::` struct.
  - **Streams:** `messageStream`, `errorStream`, `debugStream`, and the array `fileStreams[9]`. They are internal: user code reaches them only through keywords.

  Confirm or rename.
