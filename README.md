# Printer2
MC-oriented printer with streams

## Base idea: create a printer that can be used in an MC environment and on PC, similar to std::cout

## Main features

### Printer: Type-Safe I/O Subsystem

- **Dual USART Support**: USART2 (production) and USART3 (debug/calibration) with hardware flow control
- **DMA Acceleration**: Optional DMA for both USART2 and USART3 Tx channels
- **Double-Buffered Output**: Bank-switching memory architecture for non-blocking serial transmission
- **Stream-Style API**: Operator overloading (`<<`) for intuitive output composition
- **Multiple Formats**: Decimal and hexadecimal output modes for all types
- **Comprehensive Type Support**: int8/16/32/64, uint8/16/32/64, float, double, char, strings
- **Format Specifiers**: Printf-style formatting (`%d`, `%u`, `%f`, `%s`, `%c`)
- **Cross-Platform**: Abstracted USART interface works on both STM32 and PC (console and file output)
