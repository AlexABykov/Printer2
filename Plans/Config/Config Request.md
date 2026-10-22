# Request: implement Printer Configuration  settings
**Date:** 2026‑10‑23  
**Status:** Draft (REMEMBER: only BY can change the Status)

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
STR_LOG.pStream = USART2.pStream;
STR_MSG.pStream = USART2.pStream;
STR_LOG.pStream = USART3.pStream;
// No color support for the whole MC platform
```

### PC

```cpp
// STM32F103VET6 (example)
// Registers pointers assignment to static variables (including GPIO)
// TODO: implement more registers

// WARNING! No colored prints in files

// Streams assignments (example)
STR_LOG = pcOut.console_0;
STR_MSG = pcOut.console_0;
STR_ERR = pcOut.file_1;
...

// Colors
STR_LOG.color = darkGray;
STR_MSG.color = darkGreen;
...
```