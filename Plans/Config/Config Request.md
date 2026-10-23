# Request: implement Printer Configuration  settings
**Date:** 2026‑10‑23  
**Status:** Draft (REMEMBER: only BY can change the Status)

## Proposal
Let's start with 3 configurations files: 
1. Common `config.h`
2. MC specific `config_mc.h` 
3. PC-specific `config_pc.h`

## Decisions

1. **`prt1` stays** as the single global printer. Only the **keyword target names** change (`usart2`/`usart3`/`file` → new logical stream names). Old-project call sites will be fixed by hand/IDE rename.
2. **`STR_LOG`, `STR_MSG`, `STR_ERR` are not separate `Printer` instances** — they are the new targets, i.e. what old `usart2`/`usart3` were.
3. **`BaseUsart` → `BaseStream`, `pUsart` → `pStream`.** Same role, renamed.
4. **New system, incompatible with old.** Transport, buffer sizing, parity, color routing — all free to redesign. Old code is only a source of algorithms.

## Consequence: what the draft examples actually describe

With those decisions the draft implies two layers of objects:

- **Physical transports** (`BaseStream` subclasses): real byte sinks. On MC these are the USARTs and the file-alias; on PC they are console endpoints and file endpoints.
- **Logical streams** (`STR_LOG`, `STR_MSG`, `STR_ERR`): what `prt1 << ...` selects. Each has a `pStream` pointing at a physical transport, plus a `color` (no-op on MC).

That matches `STR_LOG.pStream = USART2.pStream;` — except the draft has `USART2` itself exposing `.pStream`, which only makes sense if `USART2` is *also* a logical stream (a physical-facing one with `.settings`). I'd rather not have that third concept. Below is a cleaner version of the same idea.

## Proposed object model

```cpp
// Common/Printer/BaseStream.h
class BaseStream {
public:
    virtual void sendByte(uint8_t b) = 0;
    virtual void flush() = 0;
    virtual ~BaseStream() = default;
};

// Common/Printer/Stream.h  — logical target, what the keywords are
struct Color { uint8_t r, g, b; };

struct Stream : BaseStream {
    BaseStream* pStream = nullptr;   // physical transport
    Color       color{};
    bool        hasColor = false;    // false on MC
    void sendByte(uint8_t b) override;   // applies color on PC, forwards
    void flush() override { if (pStream) pStream->flush(); }
};

// Common/Printer/Printer.h
class Printer {
    BaseStream*       pStream;       // currently &STR_xxx
    BasePrintNumber*  pPrintNumber;
public:
    Printer& operator<<(const Stream& s) { pStream = const_cast<Stream*>(&s); return *this; }
    // ... existing operators unchanged
};

extern Printer prt1;
extern Stream STR_LOG, STR_MSG, STR_ERR;
```

MC side:

```cpp
// config_mc.h
struct UartSettings { uint32_t baudrate; uint32_t buffer; uint8_t dataBits, parity, stopBits; };

class UsartStream : public BaseStream {
public:
    UartSettings settings;
    void sendByte(uint8_t) override;
    void flush() override;
};

extern UsartStream mcUsart2, mcUsart3;   // physical (note: NOT named USART2)
```

```cpp
// PrinterConfig_mc.cpp
void printer_init() {
    mcUsart2.settings = { 115200, 2048, 8, 0, 1 };
    mcUsart3.settings = { 230400,    0, 8, 0, 1 };   // buffer==0 → polled, no DMA

    STR_LOG.pStream = &mcUsart2;   // no color on MC
    STR_MSG.pStream = &mcUsart2;
    STR_ERR.pStream = &mcUsart3;

    mcUsart2.init(); mcUsart3.init();   // programs USART + DMA from settings
}
```

PC side:

```cpp
// config_pc.h
class ConsoleStream : public BaseStream { public: void sendByte(uint8_t) override; void flush() override; };
class FileStream    : public BaseStream { public: void sendByte(uint8_t) override; void flush() override;
                                          bool open(const char* path, bool truncate=false); };

namespace pcOut {
    extern ConsoleStream console_0;
    extern FileStream    file_0, file_1;
}
```

```cpp
// PrinterConfig_pc.cpp
void printer_init() {
    STR_LOG.pStream = &pcOut.console_0; STR_LOG.color = { 64, 64, 64}; STR_LOG.hasColor = true;
    STR_MSG.pStream = &pcOut.console_0; STR_MSG.color = {  0,128,  0}; STR_MSG.hasColor = true;
    STR_ERR.pStream = &pcOut.file_1;    STR_ERR.color = {200,  0,  0}; STR_ERR.hasColor = true;
}
```

## Issues in the draft to fix before implementation

1. **`USART2` / `USART3` collide with the CMSIS register macros.** In the old project `USART2` is `((USART_TypeDef*)USART2_BASE)`. `USART2.settings.baudrate` cannot compile there. Use `mcUsart2` / `mcUsart3` (or similar).

2. **File-scope assignments are not valid C++.** `USART2.settings.baudrate = 115200;`, `STR_LOG.pStream = ...` cannot appear at namespace scope. All of this must live in `printer_init()` (or a constructor of a config struct instantiated once). The draft should show the mechanism.

3. **Duplicate `STR_LOG` and missing `STR_ERR` in the MC example** — typo in the draft.

4. **Contradictory USART3 buffer comments** — `buffer = 2048` plus “buffer is zero by default”. Decide: does the standalone module default to DMA on or off for USART3?

5. **`config.h` cannot reference platform objects.** If `config.h` is truly common, `USART2.settings...` must move to `config_mc.h`. Common should only hold types, defaults, and feature flags (`PRINT_HEX_PREFIX`, `DOUBLE_SCIENTIFIC`, default baud/parity/stop).

6. **PC section is titled “STM32F103VET6 (example)”** — copy/paste.

7. **`color` must exist on MC too** so that `STR_LOG.color = ...` in shared code compiles. Either keep `Color` in common and gate behavior with `PLATFORM_HAS_COLOR`, or make `hasColor` the only PC-specific thing.

8. **“PC register pointers to static variables”** — I don't think the standalone Printer needs this. The Printer only needs byte sinks. Register mocking is a concern of the host project's PC simulation layer. If you do want it in scope, say so; otherwise drop it from `config_pc.h`.

9. **“No colored prints in files.”** Make it an invariant enforced by the `FileStream` sink (raw bytes only), with ANSI wrapping applied only by `Stream::sendByte` when `hasColor`. Then `STR_ERR` can map to a file and still be safe.

## Remaining open questions

1. **What replaces the `file` keyword?** Old `prt1 << file` selected the log file. Under the new model, file is a physical sink that some `STR_*` points at. Options:
    - drop the keyword entirely (`STR_ERR`/`STR_LOG` is all you get), or
    - add a fourth logical stream `STR_FILE` mapped to `pcOut.file_0` on PC and to `mcUsart3` on MC.

2. **Dynamic file naming (the Recorder/CSV case).** Old `KeyWords::File{"run1.csv"}` and `StreamGuard + File::openOutput`. Under the new model, what is the equivalent? Candidate APIs:
    - `STR_LOG.openFile("run1.csv");` — `Stream` method that forwards to the inner `FileStream` (valid only if the inner sink is a file).
    - A local `Stream`:
      ```cpp
      Stream run;  run.pStream = &pcOut.file_0;  pcOut.file_0.open("run1.csv");
      prt1 << run << "row" << endLine;
      ```
    - Keep the temporary-keyword trick: `prt1 << KeyWords::File{"run1.csv"}` where `operator<<` returns a `StreamGuard`-like scope. This is the old behavior, and since old code is not expected to work you can redesign freely — but you still need *some* answer for “switch the file for this call/scope”. I recommend a plain method on the logical stream + a `StreamGuard` that saves/restores `pStream`.

3. **`error()` semantics.** Old `error()` routed to USART3. Under the new model it should route to `STR_ERR` (temporarily). Confirm — and confirm that `error()` restores the previous `pStream` (it should, same as old).

4. **Test overrides.** Old hooks were per hardware target (`set_sendByte_Usart2_override`). New tests care about roles, not hardware. Options:
    - per physical stream: `set_sendByte(mcUsart2, fn)` / `set_sendByte(pcOut.console_0, fn)`,
    - per logical stream: `STR_LOG.override(fn)` (installed on `Stream`, catches everything routed through it regardless of transport),
    - one global capture sink: `prt1.setCaptureSink(fn)`.
      I'd recommend **per logical stream**, because it survives a physical re-mapping in config.

5. **Buffer semantics.** Old banks were a hard-coded 32 bytes × 2. The new config suggests configurable size. Proposed contract:
    - `buffer == 0` → polled (no DMA).
    - `buffer > 0` → double-buffered DMA, two banks of `buffer/2`.
    - An upper bound `PRINTER_MAX_DMA_BUFFER` (compile-time) to avoid dynamic allocation. Values above it → static_assert or clamp.
      Is that acceptable, or do you want a single monolithic buffer with a separate drain?

6. **Ownership of USART/DMA/GPIO init.** In the huge project this lived outside the Printer. For a standalone module, `printer_init()` must program USART CRs, DMA channel config, and GPIO from the config. That is new code, not a port — worth acknowledging as a real work item. Does the module also own clock enable (RCC), or does the host provide it?

7. **`BasePrintNumber`** (dec/hex, float/double paths) is unchanged by the config request — confirm.

8. **Platform selection mechanism** for MC (`config_mc_stm32f103.h` + a forwarding `config_mc.h` chosen by a build macro, versus one editable `config_mc.h`). Pick one now so the file layout doesn't have to change later.

9. **`Stream`-level color emission details.** If `STR_LOG` and `STR_MSG` share `pcOut.console_0` and each carries its own color, `Stream::sendByte` must emit ANSI state. Two viable policies:
    - per byte (simple, matches old behavior, verbose),
    - lazy: emit `ESC[38;5;…m` on the first byte after a target switch and a reset on `endLine`.
      The second is cheaper and produces clean log files. Worth deciding now because it affects the `sendByte` signature and whether `Printer` or `Stream` tracks “last color emitted”.

If you confirm (or revise) points 1–9, I can turn this into the actual `config.h` / `config_mc.h` / `config_pc.h` files plus the `printer_init()` scaffold, with the physical-stream and logical-stream definitions already in place.