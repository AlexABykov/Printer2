#pragma once

// Common Printer configuration (Config Specification §9). Data only, namespace cfg.

#include <cstdint>

namespace cfg {
    // Feature flags
    inline constexpr bool printHexPrefix = true;
    inline constexpr bool doubleScientific = false;
    inline constexpr bool doublePlusInExponent = false;
    inline constexpr bool useColorDefault = true; // project switch, §5 precedence

    // Colour: 16-colour SGR foreground code, emitted as ESC[<sgr>m (§13.13)
    struct Color {
        uint8_t sgr;
    };

    inline constexpr Color black{30}, red{31}, darkGreen{32}, yellow{33},
            blue{34}, magenta{35}, cyan{36}, lightGray{37},
            darkGray{90}, brightRed{91}, green{92}, brightYellow{93},
            brightBlue{94}, brightMagenta{95}, brightCyan{96}, white{97};

    // UART settings
    enum class Parity : uint8_t { none, even, odd };

    enum class StopBits : uint8_t { one, two };

    enum class FlowControl : uint8_t { none, rtsCts };

    struct UartSettings {
        uint32_t baudRate;
        uint32_t bankSize; // one bank; RAM = 2 x bankSize; 0 = polled, no DMA (§7)
        uint8_t dataBits; // data bits, parity bit not counted (§13.42, deferred)
        Parity parity;
        StopBits stopBits;
        FlowControl flowControl;
    };

    inline constexpr uint32_t printerMinBankSize = 4; // §7, §13.33; 1-3 not yet proven
    inline constexpr uint32_t printerMaxBankSize = 4096; // upper bound only, §7; <= 65535 (CNDTR)

    inline constexpr UartSettings uartDefault{115200, 0, 8, Parity::none, StopBits::one, FlowControl::none};

    inline constexpr auto defaultFileName = "noname.log"; // §8, both platforms

    // A settings constant is valid when the USART can be programmed from it as written.
    constexpr bool isValid(const UartSettings &s) {
        const bool bankOk = s.bankSize == 0
                            || (s.bankSize >= printerMinBankSize && s.bankSize <= printerMaxBankSize);
        const bool bitsOk = s.dataBits == 8 || (s.dataBits == 9 && s.parity == Parity::none);
        return s.baudRate > 0 && bankOk && bitsOk;
    }

    static_assert(printerMinBankSize >= 1 && printerMinBankSize <= printerMaxBankSize);
    static_assert(printerMaxBankSize <= 65535, "DMA CNDTR is a 16-bit register");
    static_assert(isValid(uartDefault));
} // namespace cfg
