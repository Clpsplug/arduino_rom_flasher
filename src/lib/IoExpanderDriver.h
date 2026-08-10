#ifndef ROM_FLASHER_IOEXPANDERDRIVER_H
#define ROM_FLASHER_IOEXPANDERDRIVER_H

#include "Adafruit_MCP23X17.h"

class Adafruit_MCP23X17;
namespace ecp {
enum class IoExpanderError : std::uint16_t;

struct IoExpanderDriver {
    IoExpanderDriver();

    bool init(std::uint8_t i2c_addr = 0x20);

    void toggleStatus(bool sd, bool access, bool error);
    void toggleSDIndicator(bool on);
    void toggleAccessIndicator(bool on);
    void toggleErrorIndicator(bool on);

    bool getStartSwitchDown();
    // True if write protected.
    bool isWriteProtected();

    [[nodiscard]] IoExpanderError getError() const {
        return error;
    }

private:
    IoExpanderError error;
    Adafruit_MCP23X17 mcp;
    std::uint8_t i2c_addr;
};

enum class IoExpanderError : std::uint16_t {
    OK,
    NO_INIT,
    INIT_FAIL,
    MAX,
};
} // namespace ecp

#endif // ROM_FLASHER_IOEXPANDERDRIVER_H
