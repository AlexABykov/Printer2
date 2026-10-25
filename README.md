# Printer2

A stream-style printer for microcontrollers and the PC, similar to `std::cout`.

Printer2 is a **technical demo**. The same code prints from a bare-metal STM32 microcontroller (MC) to its USARTs, and from a PC program to the console and to files.

## Status

**Design done, implementation not started** (2026‑10‑26). The specification and the first implementation plans are written, and the Printer's user API (keywords, formatted output, flushing) is settled; no Printer code exists yet. The next step is Phase 1: the common configuration header, `Printer/Config/config.hpp`.

Live status and the session history are kept in [`_ProjectHandOff.md`](_ProjectHandOff.md).

## What it will look like

```cpp
#include "Printer/Printer.hpp"
#include "Tests/Tests.hpp"

Printer prt2 = {};                        // the constructor boots the Printer

int main() {
    const int failed = Tests::runAll();   // the demo runs its tests first
    prt2 << message << "Hello Printer 2!" << endLine;
    return failed == 0 ? 0 : 1;           // on the MC: an endless loop instead
}
```

- **One global printer, `prt2`, and targets as keywords:** `message` (like `std::cout`), `error` (like `std::cerr`), `debug` (diagnostics, on a link of its own, and easy to switch off), and the file sinks `file0` … `file8`. A target stays selected until the next switch. Every keyword has its own type, so any object of that type is an alias: after `constexpr File6 recorder{};`, `prt2 << recorder << …` works exactly like `prt2 << file6 << …`.
- **Errors are ordinary output:** `prt2 << error << "mask = " << mask << " out of range" << endLine;`, or `prt2.error("mask = %d out of range\n", mask);`. Both print the same line; the second one adds no newline by itself, so the format string ends with `'\n'`.
- **A temporary switch with `hold`:** `prt2 << hold << error << … << endLine;` prints one line to `error`, and the following output goes back to the previous target.
- **Flushing is simple:** `<< endLine` ends the line and flushes; `<< flush` or `prt2.flush(stream)` flushes one hardware stream; `prt2.flush()` flushes all. A `'\n'` is just a character. `error` flushes itself, like `std::cerr`.

## Planned features

- **Type-safe stream API** with `operator<<`, for `int8`…`int64`, `uint8`…`uint64`, `float`, `double`, `char` and strings, in decimal and hexadecimal.
- **Formatted output** with a subset of the printf specifiers (`%d %i %u %x %X %f %s %c %p %%`): `prt2.message(…)`, `prt2.error(…)`, `prt2.debug(…)` and `prt2.file(index, …)`. They are not called `printf`, because not all of printf is supported.
- **Blocking output, with no loss:** when a buffer is full, printing waits. A stream whose hardware is stuck drops its bytes instead of hanging the program.
- **Two USARTs on the MC** (USART2 and USART3 on an STM32F103), with optional RTS/CTS hardware flow control.
- **Optional double-buffered DMA** per USART. `bankSize == 0` means polled transmission with no extra RAM; `bankSize > 0` means two banks of that size (RAM = 2 × `bankSize`, minimum 4), one filling while the other drains.
- **Colour on the PC only:** a 16-colour named palette (`darkGray`, `darkGreen`, …), emitted lazily as ANSI codes, so the emulated hardware streams sharing one console can be told apart. Never on the MC, never in files.
- **Files:** real files on the PC. On the MC, file text goes to a USART between BEGIN/END markers, so a PC-side capture can be split back into files.
- **Printer-owned boot of the hardware it needs** on the MC (GPIO, USART, DMA). The CPU clock boot is not part of the Printer.

## Platforms and code rules

| | MC | PC |
|---|---|---|
| Environment | Bare metal: no OS, no vendor or CMSIS headers | Windows or Linux developer machine |
| C++ library | None, except `<cstdint>`; no heap, no exceptions, no RTTI | Full standard library and OS APIs |
| Output | USART2 / USART3 | Console (`console0`) and files |
| Tests | Brand-new MC suite (planned), plain-text output | Own simple suite, run from `main()` |

- Common code is shared by both platforms and follows the MC rules.
- Platform code lives in `pc_*` / `mc_*` files, and the build compiles one set or the other, so the linker selects the platform.
- The full rules for contributors (people and AI agents) are in [`AGENTS.md`](AGENTS.md).

## Build

- C++17, CMake.
- Planned: two CMake presets, `pc` and `mc`.
- Today the project builds on the PC with CLion's bundled MinGW (g++ 15.2.0) and CMake 4.3.1.

## Documents

| Document | Content |
|---|---|
| [`AGENTS.md`](AGENTS.md) | Code rules per platform, test rules, working rules |
| [`_ProjectHandOff.md`](_ProjectHandOff.md) | Live status, open decisions, session mailbox |
| [`Plans/Config/Config Specification.md`](Plans/Config/Config%20Specification.md) | The design; its §13 lists all decisions and open items |
| [`Plans/Config/Config Base Plan.md`](Plans/Config/Config%20Base%20Plan.md) | The first implementation slice, Phases 0–6 |
| [`Plans/Config/Phase 1 plan.md`](Plans/Config/Phase%201%20plan.md) | The detailed plan for Phase 1 |

## History of the idea

The first discussion seed was `cons1 << "Hello, world!" << 123 << 3.14 << '\n';`. It grew into one global printer with target keywords, `prt2 << message << … << endLine`, as recorded in the specification. The printer is named `prt2` because the old project's `prt1` has a similar but not fully compatible API.
