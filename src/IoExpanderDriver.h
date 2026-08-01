//
// Created by Collapsed PLUG on 2026/07/11.
//

#ifndef ROM_FLASHER_IOEXPANDERDRIVER_H
#define ROM_FLASHER_IOEXPANDERDRIVER_H

#include "Adafruit_MCP23X17.h"

class Adafruit_MCP23X17;
namespace ecp {
enum class IoExpanderError: std::uint16_t;

struct IoExpanderDriver {
    IoExpanderDriver();

    void toggleStatus(bool sd, bool access, bool error);
    void toggleSDIndicator(bool on);
    void toggleAccessIndicator(bool on);
    void toggleErrorIndicator(bool on);

    bool readStartWriteSwitch();
    // True if write protected.
    bool readWriteProtectionSwitch();

private:
    IoExpanderError error;
    Adafruit_MCP23X17 mcp;
};

enum class IoExpanderError: std::uint16_t {
    OK,
    INIT_FAIL,
    MAX,
};
} // ecp

#endif //ROM_FLASHER_IOEXPANDERDRIVER_H
