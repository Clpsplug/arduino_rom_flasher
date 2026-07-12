/**
 * @file MC24FC.h
 * @brief API for Microchip Technologies Inc.'s MC24XX32, 64, 128, 256, 512 EEPROMs.
 *        (XX may be AA, LC, or FC)
 *        Refer to the official datasheet for valid specific values.
 * @author ClpsPLUG
 * @license MIT
 */

#ifndef MC24FC_H
#define MC24FC_H

#include <cstdint>

/**
 * MC24FC Error codes. Refer to the doc comment for each member, since this enum includes both I2C error and software error.
 */
enum class MC24FCError: std::uint32_t;

/**
 * MC24FC EEPROM API for Arduino.
 * The default constructor assumes the EEPROM is MC24FC256.
 *
 * @note It is a good practice to call @c MC24FC::getError() after each operation, including initialisation,
 *       as it may contain error even the functions seem to return just fine.
 */
class MC24FC {
public:
    /**
     * Specify the specs of your EEPROM here.
     * @param i2c_addr I2C address. It should be 0x50 ~ 0x57
     * @param max_capacity_kilobits Max capacity in kilobits. If the EEPROM is 24FC256, this is 256.
     * @param page_size Page size, or page buffer size in bytes
     */
    explicit MC24FC(
        std::uint8_t i2c_addr = 0x50,
        std::uint32_t max_capacity_kilobits = 256,
        std::uint32_t page_size = 64
        );

    /**
     * @note This class does NOT call Wire.end() when destructed, as other I2C serial comms might be in progress.
     */
    ~MC24FC() = default;

    /**
     * Initialises this EEPROM and initialises Arduino's Wire API if it isn't.
     * This method performs a quick address ACK check. Call @c MC24FC::getError and check if it returns OK.
     * @see MC24FCError
     * @see MC24FC::getError
     */
    void init();

    /**
     * Read a byte from an arbitrary address.
     * @param[in] address
     * @param[out] data Can be set to nullptr to skip a byte.
     * @return
     */
    bool readByte(std::uint16_t address, std::uint8_t *data);

    /**
     * Read a byte from the current EEPROM's address.
     * After the operation, the internal address pointer is incremented.
     * If the address goes out of range, the internal pointer address rolls over to 0x0000.
     * @return byte at current address
     */
    std::uint8_t readNext();

    /**
     * Read bytes from the current EEPROM's address.
     * After the operation, the address is incremented by the return value.
     * If the address goes out of range during this operation, the read will stop at the end,
     * and the error code will be set to ADDRESS_OUT_OF_RANGE.
     * (If this happens, the internal pointer address likely rolls over to 0x0000.)
     * @param[out] outbuf It is the caller's responsibility to ensure that the buffer is large enough to hold the data
     * @param[in] length
     * @return read bytes
     */
    std::uint16_t readNextBytes(char *outbuf, std::uint16_t length);

    /**
     * Write bytes at the requested address.
     *
     * @param[in] offset Offset at which write starts.
     * @param[in] buf data buffer.
     * @param[in] length buffer length.
     */
    void writeAt(std::uint16_t offset, const char *buf, std::uint16_t length);

    /**
     * Returns the last (current) error code.
     * @return Current error code
     */
    [[nodiscard]] MC24FCError getError() const;

private:
    std::uint16_t writeIntoPage(
        std::uint16_t offset,
        const char *buf,
        std::uint16_t length,
        std::uint16_t i2c_limit
        ) const;

    void setError(int wire_return_code);

    std::uint8_t _i2cAddr;
    std::uint32_t _maxCapacityInBits;
    std::uint32_t _pageSize;
    std::uint16_t _currentAddress;
    MC24FCError _error;
};

/**
 * MC24FC Error codes. Refer to the doc comment since this enum includes both I2C error and software error.
 */
enum class MC24FCError: std::uint32_t {
    /**
     * @brief No error.
     */
    OK,

    /**
     * @brief I2C buffer overflow on Arduino, did you write too much data?
     */
    I2C_BUFFER_OVERFLOW,

    /**
     * @brief I2C comm was attempted, but nobody answered (NACK on addressing).
     */
    WRONG_I2C_ADDRESS,

    /**
     * @brief The agent (slave) responded with NACK (NACK on data.)
     */
    I2C_AGENT_DOESNT_ACK,

    /**
     * @brief Unknown I2C error. (Wire lib error 4).
     */
    UNKNOWN_I2C_ERROR,

    /**
     * @brief I2C operation itself failed - this probably is a hardware fault, but most likely, something is miswired.
     */
    I2C_FAIL,

    /**
     * @brief Wire.endTransmission() returned a legitimately unknown error.
     */
    I2C_UNIMPLEMENTED_ERROR,

    // Non-I2C errors below

    /**
     * @brief Detected an invalid param. This is a software fault.
     */
    INVALID_PARAMETER = 0x0100,

    /**
     * @brief Tried to read an out-of-range address. This isn't an I2C error.
     */
    ADDRESS_OUT_OF_RANGE,

    /**
     * @brief You're trying to write more data than the EEPROM's internal buffer can hold.
     * Check the datasheet and construct an MC24FC instance with the correct page size.
     */
    WRITE_DATA_TOO_LARGE,

    /**
     * @brief EEPROM did not send data supposed to be sent. This might be a hardware fault.
     */
    EEPROM_DID_NOT_REPLY,
};

// Alternative names for other 24XX EEPROMs

using MC24AA = MC24FC;
using MC24LC = MC24FC;
using MC24AAError = MC24FCError;
using MC24LCError = MC24FCError;

#endif
